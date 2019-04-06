// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "app/vivaldi_command_controller.h"

#include "app/vivaldi_commands.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/api/show_menu/show_menu_api.h"
#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"
#endif

namespace vivaldi {

void SetVivaldiScrollType(int scrollType) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  Browser* browser = chrome::FindLastActive();
  if (browser) {
    extensions::VivaldiUtilitiesAPI::GetFactoryInstance()
        ->Get(browser->profile())
        ->ScrollType(scrollType);
  }
#endif
}

void UpdateCommandsForVivaldi(CommandUpdater* command_updater_) {
  command_updater_->UpdateCommandEnabled(IDC_TASK_MANAGER, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_NEW_TAB, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_NEW_WINDOW, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_IMPORT_DATA, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_PRINT, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_EXIT, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_FIND_IN_PAGE, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_TOGGLE_MENU_POSITION, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_TOGGLE_PANEL, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_TOGGLE_STATUS_BAR, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_BOOKMARKS_PANEL, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_MAIL_PANEL, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_CONTACTS_PANEL, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_DOWNLOADS_PANEL, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_SETTINGS, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_SHOW_QUICK_COMMANDS, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_TOGGLE_FULLSCREEN, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_ABOUT, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_COMMUNITY, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_SAVE_AS, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_NOTES_PANEL, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_CHECK_FOR_UPDATES, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_TASK_MANAGER, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_DEVELOPER_TOOLS, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_PLUGINS_PAGE, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_TOGGLE_BOOKMARKS_BAR, true);
  command_updater_->UpdateCommandEnabled(
      IDC_VIV_PASTE_AS_PLAIN_TEXT_OR_PASTE_AND_GO, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_SHOW_KEYBOARDSHORTCUTS, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_EXTENSIONS_PAGE, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_CLEAR_PRIVATE_DATA_PAGE, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_NEW_PRIVATE_WINDOW, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_PASTE_AS_PLAIN_TEXT, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_OPEN_PAGE, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_CLOSE_TAB, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_COMMAND_MAIN_ZOOM_IN, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_COMMAND_MAIN_ZOOM_OUT, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_COMMAND_MAIN_ZOOM_RESET, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_COMMAND_EXPORT_DATA, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_MANAGE_BOOKMARKS, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_MANAGE_HISTORY, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_CLOSE_WINDOW, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_SHOW_HELP, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_ACTIVATE_TAB, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_COMMAND_OPEN_SESSION, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_COMMAND_SAVE_SESSION, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_COMMAND_SHOW_WELCOME, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_FOCUS_ADDRESSFIELD, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_CAPTURE_PAGE_TO_DISK, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_CAPTURE_PAGE_TO_CLIPBOARD,
                                         true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_CAPTURE_AREA_TO_DISK, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_CAPTURE_AREA_TO_CLIPBOARD,
                                         true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_HISTORY_PANEL, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_TOGGLE_IMAGES, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_WINDOW_PANEL, true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_ADD_ACTIVE_TAB_TO_BOOKMARKS,
                                         true);
  command_updater_->UpdateCommandEnabled(IDC_VIV_WINDOW_MINIMIZE, true);
}

bool ExecuteVivaldiCommands(Browser* browser, int id) {
  switch (id) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    case IDC_VIV_NEW_TAB:
    case IDC_VIV_NEW_WINDOW:
    case IDC_VIV_IMPORT_DATA:
    case IDC_VIV_PRINT:
    case IDC_VIV_EXIT:
    case IDC_VIV_FIND_IN_PAGE:
    case IDC_VIV_TOGGLE_MENU_POSITION:
    case IDC_VIV_TOGGLE_PANEL:
    case IDC_VIV_TOGGLE_STATUS_BAR:
    case IDC_VIV_BOOKMARKS_PANEL:
    case IDC_VIV_MAIL_PANEL:
    case IDC_VIV_CONTACTS_PANEL:
    case IDC_VIV_DOWNLOADS_PANEL:
    case IDC_VIV_SETTINGS:
    case IDC_VIV_SHOW_QUICK_COMMANDS:
    case IDC_VIV_TOGGLE_FULLSCREEN:
    case IDC_VIV_ABOUT:
    case IDC_VIV_COMMUNITY:
    case IDC_VIV_SAVE_AS:
    case IDC_VIV_NOTES_PANEL:
    case IDC_VIV_CHECK_FOR_UPDATES:
    case IDC_VIV_TASK_MANAGER:
    case IDC_VIV_DEVELOPER_TOOLS:
    case IDC_VIV_PLUGINS_PAGE:
    case IDC_VIV_TOGGLE_BOOKMARKS_BAR:
    case IDC_VIV_PASTE_AS_PLAIN_TEXT_OR_PASTE_AND_GO:
    case IDC_VIV_SHOW_KEYBOARDSHORTCUTS:
    case IDC_VIV_EXTENSIONS_PAGE:
    case IDC_VIV_CLEAR_PRIVATE_DATA_PAGE:
    case IDC_VIV_NEW_PRIVATE_WINDOW:
    case IDC_VIV_PASTE_AS_PLAIN_TEXT:
    case IDC_VIV_OPEN_PAGE:
    case IDC_VIV_CLOSE_TAB:
    case IDC_VIV_COMMAND_MAIN_ZOOM_IN:
    case IDC_VIV_COMMAND_MAIN_ZOOM_OUT:
    case IDC_VIV_COMMAND_MAIN_ZOOM_RESET:
    case IDC_VIV_COMMAND_EXPORT_DATA:
    case IDC_VIV_MANAGE_BOOKMARKS:
    case IDC_VIV_MANAGE_HISTORY:
    case IDC_VIV_CLOSE_WINDOW:
    case IDC_VIV_SHOW_HELP:
    case IDC_VIV_ACTIVATE_TAB:
    case IDC_VIV_COMMAND_OPEN_SESSION:
    case IDC_VIV_COMMAND_SAVE_SESSION:
    case IDC_VIV_COMMAND_SHOW_WELCOME:
    case IDC_VIV_FOCUS_ADDRESSFIELD:
    case IDC_VIV_CAPTURE_PAGE_TO_DISK:
    case IDC_VIV_CAPTURE_PAGE_TO_CLIPBOARD:
    case IDC_VIV_CAPTURE_AREA_TO_DISK:
    case IDC_VIV_CAPTURE_AREA_TO_CLIPBOARD:
    case IDC_VIV_HISTORY_PANEL:
    case IDC_VIV_TOGGLE_IMAGES:
    case IDC_VIV_WINDOW_PANEL:
    case IDC_VIV_WINDOW_MINIMIZE:
      // The API is registered with a regular profile.
      extensions::ShowMenuAPI::GetFactoryInstance()
          ->Get(browser->profile()->GetOriginalProfile())
          ->CommandExecuted(id);
      break;
#endif

    default:
      return false;
  }
  return true;
}

}  // namespace vivaldi
