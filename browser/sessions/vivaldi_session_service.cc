// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//

#include "browser/sessions/vivaldi_session_service.h"

#include <algorithm>
#include <memory>
#include <string>
#include "app/vivaldi_constants.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "browser/sessions/vivaldi_session_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabrestore.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/datasource/vivaldi_image_store.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_service_commands.h"
#include "components/sessions/vivaldi_session_service_commands.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/api/tabs/tabs_private_api.h"
#include "ui/lazy_load_service.h"
#include "ui/vivaldi_browser_window.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#endif

using content::NavigationEntry;
using sessions::ContentSerializedNavigationBuilder;
using sessions::SerializedNavigationEntry;

namespace {

// Copied from session_backend.cc. Unlikely to ever change due to compatibility.
// File version number.
static const int32_t kFileCurrentVersion = 1;

// The signature at the beginning of the file = SSNS (Sessions).
static const int32_t kFileSignature = 0x53534E53;

const int kFileReadBufferSize = 1024;

// The file header is the first bytes written to the file,
// and is used to identify the file as one written by us.
struct FileHeader {
  int32_t signature;
  int32_t version;
};

struct TabDescriptor {
  TabDescriptor(content::WebContents* tab, int index_in_window, bool is_pinned) :
    tab(tab), index_in_window(index_in_window), is_pinned(is_pinned)
  {}
  content::WebContents* tab;
  int index_in_window;
  bool is_pinned;
};

std::vector<TabDescriptor> CollectTabs(Browser* browser,
                                       const std::vector<int>& ids) {
  std::vector<TabDescriptor> res;
  TabStripModel* tab_strip = browser->tab_strip_model();
  for (int i = 0; i < tab_strip->count(); ++i) {
    content::WebContents* tab = tab_strip->GetWebContentsAt(i);
    DCHECK(tab);
    if (!ids.empty()) {
      int id = extensions::ExtensionTabUtil::GetTabId(tab);
      for (unsigned j = 0; j < ids.size(); j++) {  // FIXME: O(n^2)
        if (ids.at(j) == id) {
          res.push_back(TabDescriptor(tab, j, tab_strip->IsTabPinned(i)));
        }
      }
    } else {
      res.push_back(TabDescriptor(tab, i, tab_strip->IsTabPinned(i)));
    }
  }
  return res;
}

}  // namespace

void SessionService::SetWindowVivExtData(const SessionID& window_id,
                                         const std::string& viv_ext_data) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(
      sessions::CreateSetWindowVivExtDataCommand(window_id, viv_ext_data));
}

void SessionService::SetTabVivExtData(const SessionID& window_id,
                                      const SessionID& tab_id,
                                      const std::string& viv_ext_data) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(sessions::CreateSetVivExtDataCommand(tab_id, viv_ext_data));
}

void SessionService::OnExtDataUpdated(content::WebContents* web_contents) {
  sessions::SessionTabHelper* session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(web_contents);
  if (!session_tab_helper)
    return;

  SetTabVivExtData(session_tab_helper->window_id(),
                session_tab_helper->session_id(), web_contents->GetVivExtData());
}

/* static */
bool SessionServiceBase::ShouldTrackVivaldiBrowser(Browser* browser) {
  // Don't track popup windows (like settings) in the session.
  if (browser->is_type_popup()) {
    return false;
  }

  // This is needed because the session system stores and updated the
  // BrowserFrame bounds on display change.
  if (browser->is_type_picture_in_picture()) {
    return true;
  }

  if (browser->window() && static_cast<VivaldiBrowserWindow*>(browser->window())->type() ==
      VivaldiBrowserWindow::WindowType::NORMAL) {
    return true;
  }
  return false;
}

/* static */
bool SessionServiceBase::ShouldTrackBrowserOfType(Browser::Type type) {
  sessions::SessionWindow::WindowType window_type =
      WindowTypeForBrowserType(type);
  return window_type == sessions::SessionWindow::TYPE_NORMAL;
}

namespace resource_coordinator {

void TabLifecycleUnitSource::TabLifecycleUnit::SetIsDiscarded() {
  SetState(LifecycleUnitState::DISCARDED,
           LifecycleUnitStateChangeReason::EXTENSION_INITIATED);
}

void TabLifecycleUnitExternal::SetIsDiscarded() {}

}  // namespace resource_coordinator

namespace vivaldi {

SessionOptions::SessionOptions() {}

SessionOptions::~SessionOptions() {}

VivaldiSessionService::~VivaldiSessionService() {
}

VivaldiSessionService::VivaldiSessionService()
    : errored_(false),
      buffer_(kFileReadBufferSize, 0),
      buffer_position_(0),
      available_count_(0),
      profile_(nullptr) {}

VivaldiSessionService::VivaldiSessionService(Profile* profile)
    : errored_(false),
      buffer_(kFileReadBufferSize, 0),
      buffer_position_(0),
      available_count_(0),
      profile_(profile) {}

// Based on SessionBackend::OpenAndWriteHeader()
base::File* VivaldiSessionService::OpenAndWriteHeader(
    const base::FilePath& path) {
  DCHECK(!path.empty());
  std::unique_ptr<base::File> file(new base::File(
      path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE |
                base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                base::File::FLAG_WIN_EXCLUSIVE_READ));
  if (!file->IsValid())
    return NULL;
  FileHeader header;
  header.signature = kFileSignature;
  header.version = kFileCurrentVersion;
  int wrote =
      file->WriteAtCurrentPos(reinterpret_cast<char*>(&header), sizeof(header));
  if (wrote != sizeof(header))
    return NULL;
  return file.release();
}

// Based on SessionBackend::ResetFile()
void VivaldiSessionService::ResetFile(const base::FilePath& file_name) {
  if (current_session_file_.get()) {
    // File is already open, truncate it. We truncate instead of closing and
    // reopening to avoid the possibility of scanners locking the file out
    // from under us once we close it. If truncation fails, we'll try to
    // recreate.
    const int header_size = static_cast<int>(sizeof(FileHeader));
    if (current_session_file_->Seek(base::File::FROM_BEGIN, header_size) !=
            header_size ||
        !current_session_file_->SetLength(header_size))
      current_session_file_.reset(NULL);
  }
  if (!current_session_file_.get())
    current_session_file_.reset(OpenAndWriteHeader(file_name));
}

// Based on SessionBackend::AppendCommandsToFile()
bool VivaldiSessionService::AppendCommandsToFile(
    base::File* file,
    const std::vector<std::unique_ptr<sessions::SessionCommand>>& commands) {
  for (auto& i : commands) {
    int wrote;
    const size_type content_size = static_cast<size_type>(i->size());
    const size_type total_size = content_size + sizeof(id_type);
    wrote = file->WriteAtCurrentPos(reinterpret_cast<const char*>(&total_size),
                                    sizeof(total_size));
    if (wrote != sizeof(total_size)) {
      NOTREACHED() << "error writing";
      //return false;
    }
    id_type command_id = i->id();
    wrote = file->WriteAtCurrentPos(reinterpret_cast<char*>(&command_id),
                                    sizeof(command_id));
    if (wrote != sizeof(command_id)) {
      NOTREACHED() << "error writing";
      //return false;
    }
    if (content_size > 0) {
      wrote = file->WriteAtCurrentPos(reinterpret_cast<char*>(i->contents()),
                                      content_size);
      if (wrote != content_size) {
        NOTREACHED() << "error writing";
        //return false;
      }
    }
  }
  return true;
}

bool VivaldiSessionService::Save(const base::FilePath& file_name) {
  base::VivaldiScopedAllowBlocking allow_blocking;

  base::CreateDirectory(file_name.DirName());

  ResetFile(file_name);

  // Need to check current_session_file_ again, ResetFile may fail.
  if (!current_session_file_.get() || !current_session_file_->IsValid()) {
    return false;
  }
  if (!AppendCommandsToFile(current_session_file_.get(), pending_commands_)) {
    current_session_file_.reset(NULL);
    return false;
  }
  current_session_file_.reset(NULL);
  return true;
}

bool VivaldiSessionService::ShouldTrackWindow(Browser* browser) {
  if (browser->is_type_app() && browser->is_type_popup() &&
      !browser->is_trusted_source()) {
    return false;
  }

  // Prevent tracking another "person" (user profile).
  if (profile_->GetPath() != browser->profile()->GetPath()) {
    return false;
  }

  // Prevent tracking otr (and quest) profiles form a regular.
  if (profile_ != browser->profile()) {
    if (!profile_->IsOffTheRecord() && browser->profile()->IsOffTheRecord()) {
      return false;
    }
  }

  if (!SessionServiceBase::ShouldTrackVivaldiBrowser(browser)) {
    return false;
  }
  return SessionServiceBase::ShouldTrackBrowserOfType(browser->type());
}

// Based on BaseSessionService::ScheduleCommand
void VivaldiSessionService::ScheduleCommand(
    std::unique_ptr<sessions::SessionCommand> command) {
  DCHECK(command);
  pending_commands_.push_back(std::move(command));
}

// Based on SessionService::BuildCommandsForTab
void VivaldiSessionService::BuildCommandsForTab(const SessionID& window_id,
                                                content::WebContents* tab,
                                                int index_in_window,
                                                bool is_pinned) {
  DCHECK(tab && window_id.id());
  sessions::SessionTabHelper* session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(tab);
  const SessionID& session_id(session_tab_helper->session_id());
  ScheduleCommand(sessions::CreateSetTabWindowCommand(window_id, session_id));

  const int current_index = tab->GetController().GetCurrentEntryIndex();
  const int min_index =
      std::max(current_index - sessions::gMaxPersistNavigationCount, 0);
  const int max_index =
      std::min(current_index + sessions::gMaxPersistNavigationCount,
               tab->GetController().GetEntryCount());
  const int pending_index = tab->GetController().GetPendingEntryIndex();
  tab_to_available_range_[session_id.id()] =
      std::pair<int, int>(min_index, max_index);
  if (is_pinned) {
    ScheduleCommand(sessions::CreatePinnedStateCommand(session_id, true));
  }
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::TabHelper* extensions_tab_helper =
      extensions::TabHelper::FromWebContents(tab);
  if (extensions_tab_helper->is_app()) {
    ScheduleCommand(sessions::CreateSetTabExtensionAppIDCommand(
        session_id, extensions_tab_helper->GetExtensionAppId()));
  }
#endif

  if (!tab->GetVivExtData().empty()) {
    ScheduleCommand(sessions::CreateSetVivExtDataCommand(session_id,
          tab->GetVivExtData()));
  }

  const blink::UserAgentOverride& ua_override = tab->GetUserAgentOverride();
  if (!ua_override.ua_string_override.empty()) {
    ScheduleCommand(sessions::CreateSetTabUserAgentOverrideCommand(
        session_id, sessions::SerializedUserAgentOverride::UserAgentOnly(
                        ua_override.ua_string_override)));
  }
  for (int i = min_index; i < max_index; ++i) {
    NavigationEntry* entry = (i == pending_index)
                                 ? tab->GetController().GetPendingEntry()
                                 : tab->GetController().GetEntryAtIndex(i);
    DCHECK(entry);
    if (ShouldTrackURLForRestore(entry->GetVirtualURL())) {
      const SerializedNavigationEntry navigation =
          ContentSerializedNavigationBuilder::FromNavigationEntry(i, entry);
      ScheduleCommand(CreateUpdateTabNavigationCommand(session_id, navigation));
    }
  }
  ScheduleCommand(sessions::CreateSetSelectedNavigationIndexCommand(
      session_id, current_index));

  if (index_in_window != -1) {
    ScheduleCommand(sessions::CreateSetTabIndexInWindowCommand(
        session_id, index_in_window));
  }
  // Record the association between the sessionStorage namespace and the tab.
  content::SessionStorageNamespace* session_storage_namespace =
      tab->GetController().GetDefaultSessionStorageNamespace();
  ScheduleCommand(sessions::CreateSessionStorageAssociatedCommand(
      session_tab_helper->session_id(), session_storage_namespace->id()));
}


int VivaldiSessionService::SetCommands(
  std::vector<std::unique_ptr<sessions::SessionWindow>>& windows,
  std::vector<std::unique_ptr<sessions::SessionTab>>& tabs)
{
  for (auto& window : windows) {
    for (auto& tab : tabs) {
      if (tab->window_id == window->window_id) {
        SetCommandsForWindow(*window.get(), tabs);
        break;
      }
    }
  }
  return pending_commands_.size();
}

void VivaldiSessionService::SetCommandsForWindow(
    const sessions::SessionWindow& window,
    std::vector<std::unique_ptr<sessions::SessionTab>>& tabs) {
  SessionID window_id = window.window_id;

  ScheduleCommand(sessions::CreateSetWindowBoundsCommand(
      window_id, window.bounds, window.show_state));
  ScheduleCommand(sessions::CreateSetWindowTypeCommand(
      window_id, window.type));
  if (!window.app_name.empty()) {
    ScheduleCommand(sessions::CreateSetWindowAppNameCommand(
        window_id, window.app_name));
  }
  if (!window.viv_ext_data.empty()) {
    ScheduleCommand(sessions::CreateSetWindowVivExtDataCommand(
        window_id, window.viv_ext_data));
  }
  for (auto& tab : tabs) {
    if (tab->window_id == window_id) {
      SetCommandsForTab(*tab);
    }
  }
  ScheduleCommand(sessions::CreateSetSelectedTabInWindowCommand(
      window_id, window.selected_tab_index));
}

void VivaldiSessionService::SetCommandsForTab(const sessions::SessionTab& tab) {
  SessionID window_id = tab.window_id;
  SessionID tab_id = tab.tab_id;

  ScheduleCommand(sessions::CreateSetTabWindowCommand(window_id, tab_id));
  if (tab.pinned) {
    ScheduleCommand(sessions::CreatePinnedStateCommand(tab_id, true));
  }
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!tab.extension_app_id.empty()) {
    ScheduleCommand(sessions::CreateSetTabExtensionAppIDCommand(
        tab_id, tab.extension_app_id));
  }
#endif
  if (!tab.viv_ext_data.empty()) {
    ScheduleCommand(sessions::CreateSetVivExtDataCommand(
        tab_id, tab.viv_ext_data));
  }
  if (!tab.user_agent_override.ua_string_override.empty()) {
    ScheduleCommand(sessions::CreateSetTabUserAgentOverrideCommand(
        tab_id, tab.user_agent_override));
  }
  for (auto& navigation : tab.navigations) {
    ScheduleCommand(CreateUpdateTabNavigationCommand(tab_id, navigation));
  }

  ScheduleCommand(sessions::CreateSetSelectedNavigationIndexCommand(
      tab_id, tab.current_navigation_index));
  if (tab.tab_visual_index != -1) {
    ScheduleCommand(sessions::CreateSetTabIndexInWindowCommand(
        tab_id, tab.tab_visual_index));
  }
  ScheduleCommand(sessions::CreateSessionStorageAssociatedCommand(
      tab_id, tab.session_storage_persistent_id));
}

std::vector<std::string>
VivaldiSessionService::CollectThumbnailUrls(Browser* browser,
    const std::vector<int>& ids)
{
  auto tabs = CollectTabs(browser, ids);

  std::vector<std::string> thubnail_urls;
  for (auto &tab_descr: tabs) {
    auto *tab = tab_descr.tab;
    if (tab->GetVivExtData().empty()) {
      continue;
    }

    const std::string viv_ext_data = tab->GetVivExtData();
    std::optional<base::Value> json =
      base::JSONReader::Read(viv_ext_data, base::JSON_PARSE_RFC);

    if (json && json->is_dict()) {
      std::string * thumbnail = json->GetDict().FindString("thumbnail");
      if (thumbnail) {
        thubnail_urls.push_back(*thumbnail);
      }
    }
  }
  return thubnail_urls;
}

void VivaldiSessionService::BuildCommandsForTabs(Browser* browser,
    const std::vector<int>& ids) {
  auto tabs = CollectTabs(browser, ids);
  for (auto &tab: tabs) {
    BuildCommandsForTab(browser->session_id(), tab.tab, tab.index_in_window,
        tab.is_pinned);
  }
}

// Based on SessionService::BuildCommandsForBrowser
void VivaldiSessionService::BuildCommandsForBrowser(
    Browser* browser,
    const std::vector<int>& ids,
    const vivaldi_image_store::Batch& batch) {
  DCHECK(browser);
  DCHECK(browser->session_id().id());

  ScheduleCommand(sessions::CreateSetWindowBoundsCommand(
      browser->session_id(), browser->window()->GetRestoredBounds(),
      browser->window()->GetRestoredState()));

  ScheduleCommand(sessions::CreateSetWindowTypeCommand(
      browser->session_id(), WindowTypeForBrowserType(browser->type())));

  if (!browser->app_name().empty()) {
    ScheduleCommand(sessions::CreateSetWindowAppNameCommand(
        browser->session_id(), browser->app_name()));
  }
  if (!browser->viv_ext_data().empty()) {
    ScheduleCommand(sessions::CreateSetWindowVivExtDataCommand(
        browser->session_id(), browser->viv_ext_data()));
  }

  BuildCommandsForTabs(browser, ids);

  ScheduleCommand(sessions::CreateSetSelectedTabInWindowCommand(
      browser->session_id(), browser->tab_strip_model()->active_index()));

  for (auto& item : batch) {
    if (item.state != vivaldi_image_store::BatchItemState::kOk ||
        item.data.empty()) {
      LOG(INFO) << "Invalid thumbnail in batch: " << item.url;
      continue;
    }
    ScheduleCommand(sessions::CreateVivCreateThumbnailCommand(
        int(item.format), reinterpret_cast<const char*>(item.data.data()),
        item.data.size()));
  }
}

int VivaldiSessionService::Load(const base::FilePath& path,
                                Browser* browser,
                                const SessionOptions& opts) {
  browser_ = browser;
  opts_ = opts;
  current_session_file_.reset(
      new base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ));
  if (!current_session_file_->IsValid())
    return sessions::kErrorLoadFailure;

  std::vector<std::unique_ptr<sessions::SessionCommand>> cmds;
  std::vector<std::unique_ptr<sessions::SessionWindow>> valid_windows;
  SessionID active_window_id = SessionID::InvalidValue();

  if (Read(current_session_file_.get(), &cmds)) {
    std::vector<std::unique_ptr<sessions::SessionCommand>> commands;
    for (auto &item: cmds) {
      if (item->id() == sessions::GetVivCreateThumbnailCommandId()) {
        base::Pickle pickle = item->PayloadAsPickle();
        base::PickleIterator iterator(pickle);
        int format;
        if (!iterator.ReadInt(&format)) {
          continue;
        }

        const char *data{};
        size_t len = 0;

        if (!iterator.ReadData(&data, &len)) {
          continue;
        }

        scoped_refptr<base::RefCountedMemory> image_data =
            base::MakeRefCounted<base::RefCountedBytes>(
                base::span(reinterpret_cast<const unsigned char*>(data), len));

        VivaldiImageStore::ImagePlace place;
        VivaldiImageStore::StoreImage(profile_.get(), std::move(place),
            vivaldi_image_store::ImageFormat(format), image_data, base::DoNothing());
        continue;
      }
      commands.push_back(std::move(item));
    }

    sessions::RestoreSessionFromCommands(commands, &valid_windows,
                                         &active_window_id);
    RemoveUnusedRestoreWindows(&valid_windows);
    std::vector<SessionRestoreDelegate::RestoredTab> created_contents;
    ProcessSessionWindows(&valid_windows, active_window_id, &created_contents);
    return created_contents.size() == 0
      ? sessions::kErrorNoContent
      : sessions::kNoError;
  }
  return sessions::kErrorLoadFailure;
}

std::vector<std::unique_ptr<sessions::SessionCommand>>
VivaldiSessionService::LoadSettingInfo(const base::FilePath& path) {
  std::vector<std::unique_ptr<sessions::SessionCommand>> commands;
  base::File session_file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!session_file.IsValid())
    return commands;

  Read(&session_file, &commands);

  return commands;
}

// Based on CreateRestoredBrowser in session_restore.cc
Browser* VivaldiSessionService::CreateRestoredBrowser(
    Browser::Type type,
    gfx::Rect bounds,
    ui::mojom::WindowShowState show_state,
    const std::string& app_name) {
  Browser::CreateParams params(profile_, false);
  if (!app_name.empty()) {
    const bool trusted_source = true;  // We only store trusted app windows.
    params = Browser::CreateParams::CreateForApp(app_name, trusted_source,
                                                 bounds, profile_, false);
  } else {
    params.initial_bounds = bounds;
  }
  params.initial_show_state = show_state;
  params.creation_source = Browser::CreationSource::kSessionRestore;
  params.is_vivaldi = true;
  return Browser::Create(params);
}

// Based on ShowBrowser in session_restore.cc
void VivaldiSessionService::ShowBrowser(Browser* browser,
                                        int selected_tab_index) {
  DCHECK(browser);
  DCHECK(browser->tab_strip_model()->count());

  if (browser_ == browser)
    return;

  browser->window()->Show();
  browser->set_is_session_restore(false);

  // TODO(jcampan): http://crbug.com/8123 we should not need to set the
  //                initial focus explicitly.
  browser->tab_strip_model()->ActivateTabAt(selected_tab_index);
}

// Based on RestoreTabsToBrowser in session_restore.cc
// Adds the tabs from |window| to |browser|. Normal tabs go after the existing
// tabs but pinned tabs will be pushed in front.
// If there are no existing tabs, the tab at |selected_tab_index| will be
// selected. Otherwise, the tab selection will remain untouched.
void VivaldiSessionService::RestoreTabsToBrowser(
    const sessions::SessionWindow& window,
    Browser* browser,
    int initial_tab_count,
    int selected_tab_index,
    std::vector<SessionRestoreDelegate::RestoredTab>* created_contents) {
  DCHECK(!window.tabs.empty());
  if (initial_tab_count == 0) {
    // We prefer to select the one that is selected in the session, but fall
    // back to the first if none are.
    int actual_selected_tab_index = 0;
    for (int i = 0; i < static_cast<int>(window.tabs.size()); ++i) {
      const sessions::SessionTab& tab = *(window.tabs[i]);

      // Loads are scheduled for each restored tab unless the tab is going to
      // be selected as ShowBrowser() will load the selected tab.
      bool is_selected_tab = (i == selected_tab_index);
      content::WebContents* contents =
          RestoreTab(tab, i, browser, is_selected_tab);

      // RestoreTab can return nullptr if |tab| doesn't have valid data.
      if (!contents)
        continue;

      std::optional<tab_groups::TabGroupId> group;
      SessionRestoreDelegate::RestoredTab restored_tab(
          contents, is_selected_tab, tab.extension_app_id.empty(), tab.pinned,
          group);
      created_contents->push_back(restored_tab);

      // If this isn't the selected tab, there's nothing else to do.
      if (!is_selected_tab)
        continue;

      actual_selected_tab_index =
        browser->tab_strip_model()->GetIndexOfWebContents(contents);

    }
    if (browser->tab_strip_model()->count() > 0) {
      ShowBrowser(browser, actual_selected_tab_index);
    }
  } else {
    // If the browser already has tabs, we want to restore the new ones after
    // the existing ones.
    int tab_index_offset = initial_tab_count;
    int num_restored = 0;
    for (int i = 0; i < static_cast<int>(window.tabs.size()); ++i) {
      const sessions::SessionTab& tab = *(window.tabs[i]);
      // Always schedule loads as we will not be calling ShowBrowser().
      content::WebContents* contents =
          RestoreTab(tab, tab_index_offset + i, browser, false);
      if (contents) {
        std::optional<tab_groups::TabGroupId> group;
        // Sanitize the last active time.
        SessionRestoreDelegate::RestoredTab restored_tab(
            contents, false, tab.extension_app_id.empty(), tab.pinned, group);
        created_contents->push_back(restored_tab);
        num_restored ++;
      }
    }
    // Activate the first of the restored tabs.
    if (num_restored > 0) {
      browser->tab_strip_model()->ActivateTabAt(tab_index_offset);
    }
  }
}

// Based on RestoreTab in session_restore.cc
// |tab_index| is ignored for pinned tabs which will always be pushed behind
// the last existing pinned tab.
// |tab_loader_| will schedule this tab for loading if |is_selected_tab| is
// false.
content::WebContents* VivaldiSessionService::RestoreTab(
    const sessions::SessionTab& tab,
    const int tab_index,
    Browser* browser,
    bool is_selected_tab) {
  if (sessions::IsTabQuarantined(&tab)) {
    return nullptr;
  }

  if (!opts_.withWorkspace_ &&
      extensions::IsTabInAWorkspace(tab.viv_ext_data)) {
    return nullptr;
  }

  const std::vector<int>& ids = opts_.tabs_to_include_;
  if (ids.size() > 0 &&
      std::find(ids.begin(), ids.end(), tab.tab_id.id()) == ids.end()) {
    return nullptr;
  }

  // It's possible (particularly for foreign sessions) to receive a tab
  // without valid navigations. In that case, just skip it.
  // See crbug.com/154129.
  if (tab.navigations.empty())
    return nullptr;
  int selected_index = tab.current_navigation_index;
  selected_index = std::max(
      0,
      std::min(selected_index, static_cast<int>(tab.navigations.size() - 1)));

  // Associate sessionStorage (if any) to the restored tab.
  scoped_refptr<content::SessionStorageNamespace> session_storage_namespace;
  if (!tab.session_storage_persistent_id.empty()) {
    session_storage_namespace =
        profile_->GetDefaultStoragePartition()
            ->GetDOMStorageContext()
            ->RecreateSessionStorage(tab.session_storage_persistent_id);
  }
  std::optional<tab_groups::TabGroupId> group;
  content::WebContents* web_contents = chrome::AddRestoredTab(
      browser, tab.navigations, tab_index, selected_index, tab.extension_app_id,
      group, false,  // select
      tab.pinned, base::TimeTicks(), base::Time(), session_storage_namespace.get(),
      tab.user_agent_override, tab.extra_data, true /* from_session_restore */,
      true /* is_active_browser */,
      tab.viv_page_action_overrides, tab.viv_ext_data);
  // Regression check: check that the tab didn't start loading right away. The
  // focused tab will be loaded by Browser, and TabLoader will load the rest.
  DCHECK(web_contents->GetController().NeedsReload());

  return web_contents;
}

// Based on NotifySessionServiceOfRestoredTabs in session_restore.cc
// Invokes TabRestored on the SessionService for all tabs in browser after
// initial_count.
void VivaldiSessionService::NotifySessionServiceOfRestoredTabs(
    Browser* browser,
    int initial_count) {
  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile_);
  if (!session_service)
    return;
  TabStripModel* tab_strip = browser->tab_strip_model();
  for (int i = initial_count; i < tab_strip->count(); ++i) {

    session_service->TabRestored(tab_strip->GetWebContentsAt(i),
                                 tab_strip->IsTabPinned(i));
  }
}

// Based on ProcessSessionWindows in session_restore.cc
Browser* VivaldiSessionService::ProcessSessionWindows(
    std::vector<std::unique_ptr<sessions::SessionWindow>>* windows,
    const SessionID& active_window_id,
    std::vector<SessionRestoreDelegate::RestoredTab>* created_contents) {
  DVLOG(1) << "ProcessSessionWindows " << windows->size();

  if (windows->empty()) {
    // Restore was unsuccessful. The DOM storage system can also delete its
    // data, since no session restore will happen at a later point in time.
    profile_->GetDefaultStoragePartition()
        ->GetDOMStorageContext()
        ->StartScavengingUnusedSessionStorage();
    NOTREACHED();
    //return nullptr;
  }
  // After the for loop this contains the last TABBED_BROWSER. Is null if no
  // tabbed browsers exist.
  Browser* last_browser = nullptr;
  bool has_tabbed_browser = false;

  // After the for loop, this contains the browser to activate, if one of the
  // windows has the same id as specified in active_window_id.
  Browser* browser_to_activate = nullptr;

  // Determine if there is a visible window, or if the active window exists.
  // Even if all windows are ui::mojom::WindowShowState::kMinimized, if one
  // of them is the active window it will be made visible by the call to
  // browser_to_activate->window()->Activate() later on in this method.
  bool has_visible_browser = false;
  for (auto i = windows->begin(); i != windows->end(); ++i) {
    if ((*i)->show_state != ui::mojom::WindowShowState::kMinimized ||
        (*i)->window_id == active_window_id)
      has_visible_browser = true;
  }

  for (auto i = windows->begin(); i != windows->end(); ++i) {
    Browser* browser = nullptr;
    if (!has_tabbed_browser &&
        (*i)->type == sessions::SessionWindow::TYPE_NORMAL) {
      has_tabbed_browser = true;
    }
    if (i == windows->begin() && !opts_.newWindow_ &&
        (*i)->type == sessions::SessionWindow::TYPE_NORMAL && browser_ &&
        browser_->is_type_normal() && !browser_->profile()->IsOffTheRecord()) {
      // The first set of tabs is added to the existing browser.
      browser = browser_;
    } else {
      if (opts_.oneWindow_ && last_browser) {
        browser = last_browser;
      } else {
        // Do not create a browser with no tabs.
        if (!HasTabs(*(*i).get())) {
          continue;
        }
        // Show the first window if none are visible.
        ui::mojom::WindowShowState show_state = (*i)->show_state;
        if (!has_visible_browser) {
          show_state = ui::mojom::WindowShowState::kNormal;
          has_visible_browser = true;
        }
        browser = CreateRestoredBrowser(BrowserTypeForWindowType((*i)->type),
                                        (*i)->bounds, show_state,
                                        (*i)->app_name);
      }
    }
    if ((*i)->type == sessions::SessionWindow::TYPE_NORMAL) {
      last_browser = browser;
    }
    int initial_tab_count = browser->tab_strip_model()->count();
    int selected_tab_index =
        initial_tab_count > 0
            ? browser->tab_strip_model()->active_index()
            : std::max(0, std::min((*i)->selected_tab_index,
                                   static_cast<int>((*i)->tabs.size()) - 1));
    if ((*i)->window_id == active_window_id) {
      browser_to_activate = browser;
    }
    browser->set_viv_ext_data((*i)->viv_ext_data);

    RestoreTabsToBrowser(*(*i), browser, initial_tab_count, selected_tab_index,
                         created_contents);
    NotifySessionServiceOfRestoredTabs(browser, initial_tab_count);
  }

  if (browser_to_activate && browser_to_activate->is_type_normal())
    last_browser = browser_to_activate;

  if (browser_to_activate)
    browser_to_activate->window()->Activate();

  // sessionStorages needed for the session restore have now been recreated
  // by RestoreTab. Now it's safe for the DOM storage system to start
  // deleting leftover data.
  profile_->GetDefaultStoragePartition()
      ->GetDOMStorageContext()
      ->StartScavengingUnusedSessionStorage();
  return last_browser;
}

// Same tests as in RestoreTab
bool VivaldiSessionService::HasTabs(const sessions::SessionWindow& window) {
  for (int i = 0; i < static_cast<int>(window.tabs.size()); ++i) {
    const sessions::SessionTab& tab = *(window.tabs[i]);
    if (sessions::IsTabQuarantined(&tab)) {
      continue;
    }
    if (!opts_.withWorkspace_ &&
      extensions::IsTabInAWorkspace(tab.viv_ext_data)) {
      continue;
    }

    const std::vector<int>& ids = opts_.tabs_to_include_;
    if (ids.size() > 0 &&
      std::find(ids.begin(), ids.end(), tab.tab_id.id()) == ids.end()) {
      continue;
    }

    if (tab.navigations.empty()) {
      continue;
    }
    return true;
  }
  return false;
}

// RemoveUnusedRestoreWindows from session_service.cc
void VivaldiSessionService::RemoveUnusedRestoreWindows(
    std::vector<std::unique_ptr<sessions::SessionWindow>>* window_list) {
  auto i = window_list->begin();
  while (i != window_list->end()) {
    sessions::SessionWindow* window = i->get();
    if (window->type != sessions::SessionWindow::TYPE_NORMAL) {
      delete window;
      i = window_list->erase(i);
    } else {
      ++i;
    }
  }
}

// Based on SessionFileReader::Read()
bool VivaldiSessionService::Read(
    base::File* file,
    std::vector<std::unique_ptr<sessions::SessionCommand>>* commands) {
  FileHeader header;
  int read_count;
  read_count =
      file->ReadAtCurrentPos(reinterpret_cast<char*>(&header), sizeof(header));
  if (read_count != sizeof(header) || header.signature != kFileSignature ||
      header.version != kFileCurrentVersion)
    return false;

  std::vector<std::unique_ptr<sessions::SessionCommand>> read_commands;
  for (std::unique_ptr<sessions::SessionCommand> command = ReadCommand(file);
       command && !errored_; command = ReadCommand(file)) {
    read_commands.push_back(std::move(command));
  }
  if (!errored_) {
    read_commands.swap(*commands);
  }
  return !errored_;
}

// Based on SessionFileReader::ReadCommand()
std::unique_ptr<sessions::SessionCommand> VivaldiSessionService::ReadCommand(
    base::File* file) {
  // Make sure there is enough in the buffer for the size of the next command.
  if (available_count_ < sizeof(size_type)) {
    if (!FillBuffer(file))
      return NULL;
    if (available_count_ < sizeof(size_type)) {
      VLOG(1) << "SessionFileReader::ReadCommand, file incomplete";
      // Still couldn't read a valid size for the command, assume write was
      // incomplete and return NULL.
      return NULL;
    }
  }
  // Get the size of the command.
  size_type command_size;
  memcpy(&command_size, &(buffer_[buffer_position_]), sizeof(command_size));
  buffer_position_ += sizeof(command_size);
  available_count_ -= sizeof(command_size);

  if (command_size == 0) {
    VLOG(1) << "SessionFileReader::ReadCommand, empty command";
    // Empty command. Shouldn't happen if write was successful, fail.
    return NULL;
  }

  // Make sure buffer has the complete contents of the command.
  if (command_size > available_count_) {
    if (command_size > buffer_.size())
      buffer_.resize((command_size / 1024 + 1) * 1024, 0);
    if (!FillBuffer(file) || command_size > available_count_) {
      // Again, assume the file was ok, and just the last chunk was lost.
      VLOG(1) << "SessionFileReader::ReadCommand, last chunk lost";
      return NULL;
    }
  }
  const id_type command_id = buffer_[buffer_position_];
  // NOTE: command_size includes the size of the id, which is not part of
  // the contents of the SessionCommand.
  std::unique_ptr<sessions::SessionCommand> command =
      std::make_unique<sessions::SessionCommand>(
          command_id, command_size - sizeof(id_type));
  if (command_size > sizeof(id_type)) {
    memcpy(command->contents(), &(buffer_[buffer_position_ + sizeof(id_type)]),
           command_size - sizeof(id_type));
  }
  buffer_position_ += command_size;
  available_count_ -= command_size;
  return command;
}

// Based on SessionFileReader::FillBuffer()
bool VivaldiSessionService::FillBuffer(base::File* file) {
  if (available_count_ > 0 && buffer_position_ > 0) {
    // Shift buffer to beginning.
    memmove(&(buffer_[0]), &(buffer_[buffer_position_]), available_count_);
  }
  buffer_position_ = 0;
  DCHECK(buffer_position_ + available_count_ < buffer_.size());
  int to_read = static_cast<int>(buffer_.size() - available_count_);
  int read_count =
      file->ReadAtCurrentPos(&(buffer_[available_count_]), to_read);
  if (read_count < 0) {
    errored_ = true;
    return false;
  }
  if (read_count == 0)
    return false;
  available_count_ += read_count;
  return true;
}

}  // namespace vivaldi
