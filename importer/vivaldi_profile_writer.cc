// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "base/json/json_writer.h"
#include "browser/sessions/vivaldi_session_service.h"
#include "chrome/browser/importer/profile_writer.h"

#include "app/vivaldi_resources.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/token.h"
#include "base/values.h"
#include "browser/sessions/vivaldi_session_service.h"
#include "browser/sessions/vivaldi_session_utils.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_common_utils.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabrestore.h"
#include "chrome/browser/ui/startup/startup_tab.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/vivaldi_bookmark_kit.h"
#include "components/notes/note_node.h"
#include "components/notes/notes_factory.h"
#include "components/notes/notes_model.h"
#include "importer/imported_notes_entry.h"
#include "importer/imported_speeddial_entry.h"
#include "importer/imported_tab_entry.h"
#include "ui/base/l10n/l10n_util.h"

#include "importer/chromium_extension_importer.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using vivaldi::NoteNode;
using vivaldi::NotesModel;
using vivaldi::NotesModelFactory;

void ChromiumExtensionsImporterDeleter::operator()(
    extension_importer::ChromiumExtensionsImporter* ptr) {
  delete ptr;
}

namespace {

// Generates a unique folder name. If |folder_name| is not unique, then this
// repeatedly tests for '|folder_name| + (i)' until a unique name is found.
std::u16string VivaldiGenerateUniqueFolderName(
    BookmarkModel* model,
    const std::u16string& folder_name) {
  // Build a set containing the bookmark bar folder names.
  std::set<std::u16string> existing_folder_names;
  const BookmarkNode* bookmark_bar = model->bookmark_bar_node();
  for (const auto& node : bookmark_bar->children()) {
    if (node->is_folder())
      existing_folder_names.insert(node->GetTitle());
  }

  // If the given name is unique, use it.
  if (existing_folder_names.find(folder_name) == existing_folder_names.end())
    return folder_name;

  // Otherwise iterate until we find a unique name.
  for (size_t i = 1; i <= existing_folder_names.size(); ++i) {
    std::u16string name =
        folder_name + u" (" + base::NumberToString16(i) + u")";
    if (existing_folder_names.find(name) == existing_folder_names.end())
      return name;
  }

  NOTREACHED();
  // return folder_name;
}

void RestoreTabsToBrowser(
    std::vector<std::unique_ptr<sessions::SessionTab>>& tabs,
    Browser* browser) {
  TabStripModel* tab_strip_model = browser->tab_strip_model();

  DCHECK(tab_strip_model);

  int initial_tab_count = tab_strip_model->count();
  int active_tab_index = tab_strip_model->active_index();
  auto active_tab_handle = tab_strip_model->GetTabHandleAt(active_tab_index);
  auto prev_active_tab = tab_strip_model->GetActiveTab();

  const base::Time epoch_time = base::Time::UnixEpoch();
  const base::TimeTicks epoch_time_ticks = base::TimeTicks::UnixEpoch();

  int tab_index = initial_tab_count;

  for (const auto& tab : tabs) {
    // Convert the last active time because WebContents needs a TimeTicks.
    const base::TimeDelta delta = tab->last_active_time - epoch_time;
    const base::TimeTicks last_active_time_ticks = epoch_time_ticks + delta;

    // Skip tabs with empty navigations.
    if (tab->navigations.empty())
      continue;

    // We don't use tab groups.
    DCHECK(!tab->group);

    // Tab selected index in navigations - i.e. the current position in history
    // of browsing in the tab.
    int selected_index = GetNavigationIndexToSelect(*tab);

    chrome::AddRestoredTab(browser, tab->navigations, tab_index, selected_index,
                           tab->extension_app_id, std::nullopt, false,
                           tab->pinned, last_active_time_ticks, tab->last_active_time,
                           nullptr,
                           tab->user_agent_override, tab->extra_data, false,
                           true,
                           // Vivaldi
                           tab->viv_page_action_overrides, tab->viv_ext_data);
    ++tab_index;
  }

  // Focus the original tab back.
  active_tab_index = tab_strip_model->GetIndexOfTab(active_tab_handle);
  DCHECK(active_tab_index != TabStripModel::kNoTab);
  tab_strip_model->ActivateTabAt(active_tab_index);
  DCHECK(tab_strip_model->GetActiveTab() == prev_active_tab);
}

}  // namespace

void ProfileWriter::AddSpeedDial(
    const std::vector<ImportedSpeedDialEntry>& speeddial) {
  if (speeddial.empty())
    return;

  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile_);
  DCHECK(model->loaded());
  const BookmarkNode* bookmark_bar = model->bookmark_bar_node();
  const std::u16string& first_folder_name =
      l10n_util::GetStringUTF16(IDS_SPEEDDIAL_GROUP_FROM_OPERA);

  model->BeginExtensiveChanges();

  const BookmarkNode* top_level_folder = NULL;

  std::u16string name =
      VivaldiGenerateUniqueFolderName(model, first_folder_name);

  vivaldi_bookmark_kit::CustomMetaInfo vivaldi_meta;
  vivaldi_meta.SetSpeeddial(true);
  top_level_folder = model->AddFolder(
      bookmark_bar, bookmark_bar->children().size(), name, vivaldi_meta.map());

  for (auto& item : speeddial) {
    if (!model->AddURL(top_level_folder, top_level_folder->children().size(),
                       item.title, item.url))
      break;
  }

  model->EndExtensiveChanges();
}

void ProfileWriter::AddNotes(const std::vector<ImportedNotesEntry>& notes,
                             const std::u16string& top_level_folder_name) {
  NotesModel* model = NotesModelFactory::GetForBrowserContext(profile_);

  model->BeginExtensiveChanges();

  std::set<const NoteNode*> folders_added_to;
  const NoteNode* top_level_folder = NULL;
  for (std::vector<ImportedNotesEntry>::const_iterator note = notes.begin();
       note != notes.end(); ++note) {
    const NoteNode* parent = NULL;
    // Add to a folder that will contain all the imported notes.
    // The first time we do so, create the folder.
    if (!top_level_folder) {
      std::u16string name;
      name = l10n_util::GetStringUTF16(IDS_NOTES_GROUP_FROM_OPERA);
      top_level_folder = model->AddFolder(
          model->main_node(), model->main_node()->children().size(), name);
    }
    parent = top_level_folder;

    // Ensure any enclosing folders are present in the model.  The note's
    // enclosing folder structure should be
    //   path[0] > path[1] > ... > path[size() - 1]
    for (std::vector<std::u16string>::const_iterator folder_name =
             note->path.begin();
         folder_name != note->path.end(); ++folder_name) {
      const NoteNode* child = NULL;
      for (size_t index = 0; index < parent->children().size(); ++index) {
        const NoteNode* node = parent->children()[index].get();
        if (node->is_folder() && node->GetTitle() == *folder_name) {
          child = node;
          break;
        }
      }
      if (!child) {
        child =
            model->AddFolder(parent, parent->children().size(), *folder_name);
      }
      parent = child;
    }

    folders_added_to.insert(parent);
    model->ImportNote(parent, parent->children().size(), *note);
  }
  model->EndExtensiveChanges();
}

void ProfileWriter::AddExtensions(
    const std::vector<std::string>& extensions,
    base::WeakPtr<ExternalProcessImporterHost> host) {
  vivaldi_extensions_importer_ =
      std::unique_ptr<extension_importer::ChromiumExtensionsImporter,
                      ChromiumExtensionsImporterDeleter>(
          new extension_importer::ChromiumExtensionsImporter(profile_, host));
  vivaldi_extensions_importer_->AddExtensions(extensions);
}

void ProfileWriter::AddOpenTabs(const std::vector<ImportedTabEntry>& tabs) {
  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile_);

  std::vector<sessions::SessionWindow*> session_windows;

  std::unique_ptr<sessions::SessionWindow> session_window =
      std::make_unique<sessions::SessionWindow>();
  session_window->window_id = SessionID::NewUnique();

  // Save the window and tab into the session service.
  session_service->SetWindowType(session_window->window_id,
                                 Browser::Type::TYPE_NORMAL);
  session_service->SetWindowVisibleOnAllWorkspaces(session_window->window_id,
                                                   false);
  int tab_idx = 0;

  std::map<base::Token, base::Uuid> group_to_uuid;
  std::map<base::Uuid, std::vector<sessions::SessionTab*>> stacks;

  for (const auto& imported_tab : tabs) {
    std::unique_ptr<sessions::SessionTab> session_tab =
        std::make_unique<sessions::SessionTab>();

    // Set up mandatory information
    auto tab_id = SessionID::NewUnique();

    int current_navigation_index = imported_tab.current_navigation_index;
    int idx = -1;
    for (const auto& imported_navigation : imported_tab.navigations) {
      ++idx;
      if (imported_navigation.url.SchemeIs("chrome") ||
          !imported_navigation.url.IsStandard()) {
        // If the navigation index is above us, we lower it, as we removed page
        // from history below current page.
        if (current_navigation_index > idx) {
          --current_navigation_index;
        }
        continue;
      }

      sessions::SerializedNavigationEntry navigation;
      navigation.set_virtual_url(imported_navigation.url);
      navigation.set_favicon_url(imported_navigation.favicon_url);
      navigation.set_title(imported_navigation.title);

      session_tab->navigations.push_back(std::move(navigation));
    }

    // if there are no navigations at all, skip this tab import.
    if (session_tab->navigations.empty())
      continue;

    session_tab->tab_id = tab_id;  // Unique tab ID
    session_tab->pinned = imported_tab.pinned;
    session_tab->current_navigation_index = current_navigation_index;
    session_tab->timestamp = imported_tab.timestamp;
    session_tab->viv_ext_data = imported_tab.viv_ext_data;

    if (!imported_tab.group.empty()) {
      // Map the group to existing/new UUID for stack ID
      const auto converted_group = base::Token::FromString(imported_tab.group);
      if (converted_group) {
        auto it = group_to_uuid.find(*converted_group);

        base::Uuid stack_id;
        if (it == group_to_uuid.end()) {
          stack_id = base::Uuid::GenerateRandomV4();
          group_to_uuid.emplace(*converted_group, stack_id);
        } else {
          stack_id = it->second;
        }

        stacks[stack_id].push_back(session_tab.get());
      }
    }

    session_window->tabs.push_back(std::move(session_tab));

    session_service->SetTabWindow(tab_id, session_window->window_id);
    // We're ignoring the previous tab index because we're merging multiple
    // window tabs in some cases.
    session_service->SetTabIndexInWindow(session_window->window_id, tab_id,
                                         ++tab_idx);
  }

  // As a post-processing step, set tab group for any group with at least 2
  // tabs...
  for (auto& [uuid, stack_tabs] : stacks) {
    if (stack_tabs.size() <= 1) {
      continue;
    }
    for (auto* tab : stack_tabs) {
      sessions::SetTabStackForImportedTab(uuid, tab);
    }
  }

  session_windows.push_back(session_window.get());

  if (session_window->tabs.empty()) {
    // VB-113094 No need to restore if no tabs in imported browser.
    return;
  }
  Browser* browser = chrome::FindBrowserWithActiveWindow();
  if (browser) {
    // If we find an active browser instance, we restore into that (same window
    // restore).
    RestoreTabsToBrowser(session_window->tabs, browser);
  } else {
    LOG(WARNING) << "Couldn't find an active window browser. Restoring into a "
                    "new window.";
    // Restore the tabs by opening a new browser instance with the window we
    // prepared.
    SessionRestore::RestoreForeignSessionWindows(
        profile_, session_windows.begin(), session_windows.end());
  }
}
