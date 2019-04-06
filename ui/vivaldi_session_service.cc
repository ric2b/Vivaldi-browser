// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//

#include "ui/vivaldi_session_service.h"

#include <algorithm>
#include <memory>
#include <string>

#include "app/vivaldi_constants.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/memory/linked_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabrestore.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/core/session_service_commands.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/buildflags/buildflags.h"
//#include "extensions/browser/extension_function_dispatcher.h"

#include "components/sessions/vivaldi_session_service_commands.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/tab_helper.h"
#endif

using sessions::ContentSerializedNavigationBuilder;
using sessions::SerializedNavigationEntry;
using content::NavigationEntry;

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

}  // namespace

namespace vivaldi {

VivaldiSessionService::~VivaldiSessionService() {}

VivaldiSessionService::VivaldiSessionService()
    : errored_(false),
      buffer_(kFileReadBufferSize, 0),
      buffer_position_(0),
      available_count_(0),
      browser_(nullptr),
      profile_(nullptr) {}

VivaldiSessionService::VivaldiSessionService(Profile* profile)
    : errored_(false),
      buffer_(kFileReadBufferSize, 0),
      buffer_position_(0),
      available_count_(0),
      browser_(nullptr),
      profile_(profile) {}

base::File* VivaldiSessionService::OpenAndWriteHeader(
    const base::FilePath& path) {
  DCHECK(!path.empty());
  std::unique_ptr<base::File> file(new base::File(
      path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE |
                base::File::FLAG_EXCLUSIVE_WRITE |
                base::File::FLAG_EXCLUSIVE_READ));
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
      return false;
    }
    id_type command_id = i->id();
    wrote = file->WriteAtCurrentPos(reinterpret_cast<char*>(&command_id),
                                    sizeof(command_id));
    if (wrote != sizeof(command_id)) {
      NOTREACHED() << "error writing";
      return false;
    }
    if (content_size > 0) {
      wrote = file->WriteAtCurrentPos(reinterpret_cast<char*>(i->contents()),
                                      content_size);
      if (wrote != content_size) {
        NOTREACHED() << "error writing";
        return false;
      }
    }
  }
  return true;
}

bool VivaldiSessionService::Save(const base::FilePath& file_name) {
  base::ThreadRestrictions::ScopedAllowIO allow_io;

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

bool VivaldiSessionService::ShouldTrackWindow(Browser* browser,
                                              Profile* profile) {
  // Skip windows not opened with the same profile.
  if (browser->profile() != profile)
    return false;
  if (browser->is_app() && browser->is_type_popup() &&
      !browser->is_trusted_source()) {
    return false;
  }
  if (!SessionService::ShouldTrackVivaldiBrowser(browser)) {
    return false;
  }
  return SessionService::ShouldTrackBrowserOfType(browser->type());
}

void VivaldiSessionService::ScheduleCommand(
    std::unique_ptr<sessions::SessionCommand> command) {
  DCHECK(command);
  pending_commands_.push_back(std::move(command));
}

void VivaldiSessionService::BuildCommandsForTab(const SessionID& window_id,
                                                content::WebContents* tab,
                                                int index_in_window,
                                                bool is_pinned) {
  DCHECK(tab && window_id.id());
  SessionTabHelper* session_tab_helper = SessionTabHelper::FromWebContents(tab);
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
  if (extensions_tab_helper->extension_app()) {
    ScheduleCommand(sessions::CreateSetTabExtensionAppIDCommand(
        session_id, extensions_tab_helper->extension_app()->id()));
  }
#endif
  if (!tab->GetExtData().empty()) {
    ScheduleCommand(
        sessions::CreateSetExtDataCommand(session_id, tab->GetExtData()));
  }
  const std::string& ua_override = tab->GetUserAgentOverride();
  if (!ua_override.empty()) {
    ScheduleCommand(sessions::CreateSetTabUserAgentOverrideCommand(
        session_id, ua_override));
  }
  for (int i = min_index; i < max_index; ++i) {
    const NavigationEntry* entry =
        (i == pending_index) ? tab->GetController().GetPendingEntry()
                             : tab->GetController().GetEntryAtIndex(i);
    DCHECK(entry);
    if (ShouldTrackURLForRestore(entry->GetVirtualURL())) {
      const SerializedNavigationEntry navigation =
          ContentSerializedNavigationBuilder::FromNavigationEntry(i, *entry);
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
      session_tab_helper->session_id(),
      session_storage_namespace->id()));
}

void VivaldiSessionService::BuildCommandsForBrowser(Browser* browser) {
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
  if (!browser->ext_data().empty()) {
    ScheduleCommand(sessions::CreateSetWindowExtDataCommand(
        browser->session_id(), browser->ext_data()));
  }
  TabStripModel* tab_strip = browser->tab_strip_model();
  for (int i = 0; i < tab_strip->count(); ++i) {
    content::WebContents* tab = tab_strip->GetWebContentsAt(i);
    DCHECK(tab);
    BuildCommandsForTab(browser->session_id(), tab, i,
                        tab_strip->IsTabPinned(i));
  }
  ScheduleCommand(sessions::CreateSetSelectedTabInWindowCommand(
      browser->session_id(), browser->tab_strip_model()->active_index()));
}

bool VivaldiSessionService::Load(const base::FilePath& path,
                                 Browser* browser,
                                 const SessionOptions& opts) {
  browser_ = browser;
  opts_ = opts;
  current_session_file_.reset(
      new base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ));
  if (!current_session_file_->IsValid())
    return false;

  std::vector<std::unique_ptr<sessions::SessionCommand>> commands;
  std::vector<std::unique_ptr<sessions::SessionWindow>> valid_windows;
  SessionID active_window_id = SessionID::InvalidValue();

  if (Read(&commands)) {
    sessions::RestoreSessionFromCommands(commands, &valid_windows,
                                         &active_window_id);
    RemoveUnusedRestoreWindows(&valid_windows);
    std::vector<SessionRestoreDelegate::RestoredTab> created_contents;
    ProcessSessionWindows(&valid_windows, active_window_id, &created_contents);
    return true;
  }
  return false;
}

Browser* VivaldiSessionService::CreateRestoredBrowser(
    Browser::Type type,
    gfx::Rect bounds,
    ui::WindowShowState show_state,
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
  params.is_session_restore = true;
  params.is_vivaldi = true;
  return new Browser(params);
}

void VivaldiSessionService::ShowBrowser(Browser* browser,
                                        int selected_tab_index) {
  DCHECK(browser);
  DCHECK(browser->tab_strip_model()->count());
  browser->tab_strip_model()->ActivateTabAt(selected_tab_index, true);

  if (browser_ == browser)
    return;

  browser->window()->Show();
  browser->set_is_session_restore(false);

  // TODO(jcampan): http://crbug.com/8123 we should not need to set the
  //                initial focus explicitly.
  browser->tab_strip_model()->GetActiveWebContents()->SetInitialFocus();
}

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

      SessionRestoreDelegate::RestoredTab restored_tab(
          contents, is_selected_tab, tab.extension_app_id.empty(), tab.pinned);
      created_contents->push_back(restored_tab);

      // If this isn't the selected tab, there's nothing else to do.
      if (!is_selected_tab)
        continue;

      ShowBrowser(browser,
                  browser->tab_strip_model()->GetIndexOfWebContents(contents));
    }
  } else {
    // If the browser already has tabs, we want to restore the new ones after
    // the existing ones.
    int tab_index_offset = initial_tab_count;
    for (int i = 0; i < static_cast<int>(window.tabs.size()); ++i) {
      const sessions::SessionTab& tab = *(window.tabs[i]);
      // Always schedule loads as we will not be calling ShowBrowser().
      content::WebContents* contents =
          RestoreTab(tab, tab_index_offset + i, browser, false);
      if (contents) {
        // Sanitize the last active time.
        SessionRestoreDelegate::RestoredTab restored_tab(
            contents, false, tab.extension_app_id.empty(), tab.pinned);
        created_contents->push_back(restored_tab);
      }
    }
  }
}

// |tab_index| is ignored for pinned tabs which will always be pushed behind
// the last existing pinned tab.
// |tab_loader_| will schedule this tab for loading if |is_selected_tab| is
// false.
content::WebContents* VivaldiSessionService::RestoreTab(
    const sessions::SessionTab& tab,
    const int tab_index,
    Browser* browser,
    bool is_selected_tab) {
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
        content::BrowserContext::GetDefaultStoragePartition(profile_)
            ->GetDOMStorageContext()
            ->RecreateSessionStorage(tab.session_storage_persistent_id);
  }
  content::WebContents* web_contents = chrome::AddRestoredTab(
      browser, tab.navigations, tab_index, selected_index, tab.extension_app_id,
      false,  // select
      tab.pinned, true, base::TimeTicks(), session_storage_namespace.get(),
      tab.user_agent_override, true /* from_session_restore */, tab.ext_data);
  // Regression check: check that the tab didn't start loading right away. The
  // focused tab will be loaded by Browser, and TabLoader will load the rest.
  DCHECK(web_contents->GetController().NeedsReload());

  return web_contents;
}

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
  for (int i = initial_count; i < tab_strip->count(); ++i)
    session_service->TabRestored(tab_strip->GetWebContentsAt(i),
                                 tab_strip->IsTabPinned(i));
}

Browser* VivaldiSessionService::ProcessSessionWindows(
    std::vector<std::unique_ptr<sessions::SessionWindow>>* windows,
    const SessionID& active_window_id,
    std::vector<SessionRestoreDelegate::RestoredTab>* created_contents) {
  DVLOG(1) << "ProcessSessionWindows " << windows->size();

  if (windows->empty()) {
    // Restore was unsuccessful. The DOM storage system can also delete its
    // data, since no session restore will happen at a later point in time.
    content::BrowserContext::GetDefaultStoragePartition(profile_)
        ->GetDOMStorageContext()
        ->StartScavengingUnusedSessionStorage();
    NOTREACHED();
    return nullptr;
  }
  // After the for loop this contains the last TABBED_BROWSER. Is null if no
  // tabbed browsers exist.
  Browser* last_browser = nullptr;
  bool has_tabbed_browser = false;

  // After the for loop, this contains the browser to activate, if one of the
  // windows has the same id as specified in active_window_id.
  Browser* browser_to_activate = nullptr;

  // Determine if there is a visible window, or if the active window exists.
  // Even if all windows are ui::SHOW_STATE_MINIMIZED, if one of them is the
  // active window it will be made visible by the call to
  // browser_to_activate->window()->Activate() later on in this method.
  bool has_visible_browser = false;
  for (auto i = windows->begin(); i != windows->end(); ++i) {
    if ((*i)->show_state != ui::SHOW_STATE_MINIMIZED ||
        (*i)->window_id == active_window_id)
      has_visible_browser = true;
  }

  for (auto i = windows->begin(); i != windows->end(); ++i) {
    Browser* browser = nullptr;
    if (!has_tabbed_browser &&
        (*i)->type == sessions::SessionWindow::TYPE_TABBED) {
      has_tabbed_browser = true;
    }
    if (i == windows->begin() && !opts_.openInNewWindow_ &&
        (*i)->type == sessions::SessionWindow::TYPE_TABBED && browser_ &&
        browser_->is_type_tabbed() && !browser_->profile()->IsOffTheRecord()) {
      // The first set of tabs is added to the existing browser.
      browser = browser_;
    } else {
      // Show the first window if none are visible.
      ui::WindowShowState show_state = (*i)->show_state;
      if (!has_visible_browser) {
        show_state = ui::SHOW_STATE_NORMAL;
        has_visible_browser = true;
      }
      browser = CreateRestoredBrowser(BrowserTypeForWindowType((*i)->type),
                                      (*i)->bounds, show_state, (*i)->app_name);
    }
    if ((*i)->type == sessions::SessionWindow::TYPE_TABBED) {
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
    browser->set_ext_data((*i)->ext_data);

    RestoreTabsToBrowser(*(*i), browser, initial_tab_count, selected_tab_index,
                         created_contents);
    NotifySessionServiceOfRestoredTabs(browser, initial_tab_count);
  }

  if (browser_to_activate && browser_to_activate->is_type_tabbed())
    last_browser = browser_to_activate;

  if (browser_to_activate)
    browser_to_activate->window()->Activate();

  // sessionStorages needed for the session restore have now been recreated
  // by RestoreTab. Now it's safe for the DOM storage system to start
  // deleting leftover data.
  content::BrowserContext::GetDefaultStoragePartition(profile_)
      ->GetDOMStorageContext()
      ->StartScavengingUnusedSessionStorage();
  return last_browser;
}

void VivaldiSessionService::RemoveUnusedRestoreWindows(
    std::vector<std::unique_ptr<sessions::SessionWindow>>* window_list) {
  auto i = window_list->begin();
  while (i != window_list->end()) {
    sessions::SessionWindow* window = i->get();
    if (window->type != sessions::SessionWindow::TYPE_TABBED) {
      delete window;
      i = window_list->erase(i);
    } else {
      ++i;
    }
  }
}

bool VivaldiSessionService::Read(
    std::vector<std::unique_ptr<sessions::SessionCommand>>* commands) {
  FileHeader header;
  int read_count;
  read_count = current_session_file_->ReadAtCurrentPos(
      reinterpret_cast<char*>(&header), sizeof(header));
  if (read_count != sizeof(header) || header.signature != kFileSignature ||
      header.version != kFileCurrentVersion)
    return false;

  std::vector<std::unique_ptr<sessions::SessionCommand>> read_commands;
  for (std::unique_ptr<sessions::SessionCommand> command = ReadCommand();
       command && !errored_; command = ReadCommand()) {
    read_commands.push_back(std::move(command));
  }
  if (!errored_) {
    read_commands.swap(*commands);
  }
  return !errored_;
}

std::unique_ptr<sessions::SessionCommand> VivaldiSessionService::ReadCommand() {
  // Make sure there is enough in the buffer for the size of the next command.
  if (available_count_ < sizeof(size_type)) {
    if (!FillBuffer())
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
    if (!FillBuffer() || command_size > available_count_) {
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

bool VivaldiSessionService::FillBuffer() {
  if (available_count_ > 0 && buffer_position_ > 0) {
    // Shift buffer to beginning.
    memmove(&(buffer_[0]), &(buffer_[buffer_position_]), available_count_);
  }
  buffer_position_ = 0;
  DCHECK(buffer_position_ + available_count_ < buffer_.size());
  int to_read = static_cast<int>(buffer_.size() - available_count_);
  int read_count = current_session_file_->ReadAtCurrentPos(
      &(buffer_[available_count_]), to_read);
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
