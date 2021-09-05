//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//

#include "extensions/api/menubar/menubar_api.h"

#include "app/vivaldi_command_controller.h"
#include "app/vivaldi_commands.h"
#include "browser/vivaldi_browser_finder.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_list.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/schema/menubar.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/vivaldi_main_menu.h"

#include <map>

namespace extensions {

namespace menubar = vivaldi::menubar;

struct ItemInfo {
  int id;
  bool enabled_with_no_window;
};

std::map<std::string, ItemInfo> tag_map;
int tag_counter = IDC_VIV_DYNAMIC_MENU_ID_START;

int getIdByAction(const menubar::MenuItem& item) {
  auto it = tag_map.find(item.action);
  if (it != tag_map.end()) {
    return tag_map[item.action].id;
  }

  // We need hardcoded ids for some actions. These actions must be mapped as we
  // tests for those elsewhere and even let chromium handle some of them.
  if (item.action == "COMMAND_CLIPBOARD_UNDO")
    tag_map[item.action] = {IDC_CONTENT_CONTEXT_UNDO, false};
  else if (item.action == "COMMAND_CLIPBOARD_REDO")
    tag_map[item.action] = {IDC_CONTENT_CONTEXT_REDO, false};
  else if (item.action == "COMMAND_CLIPBOARD_CUT")
    tag_map[item.action] = {IDC_CONTENT_CONTEXT_CUT, false};
  else if (item.action == "COMMAND_CLIPBOARD_COPY")
    tag_map[item.action] = {IDC_CONTENT_CONTEXT_COPY, false};
  else if (item.action == "COMMAND_CLIPBOARD_PASTE")
    tag_map[item.action] = {IDC_CONTENT_CONTEXT_PASTE, false};
  else if (item.action == "COMMAND_DELETE")
    tag_map[item.action] = {IDC_CONTENT_CONTEXT_DELETE, false};
  else if (item.action == "COMMAND_CLIPBOARD_SELECT_ALL")
    tag_map[item.action] = {IDC_CONTENT_CONTEXT_SELECTALL, false};
  else if (item.action == "COMMAND_HIDE_VIVALDI")
    tag_map[item.action] = {IDC_HIDE_APP, false};
  else if (item.action == "COMMAND_HIDE_OTHERS")
    tag_map[item.action] = {IDC_VIV_HIDE_OTHERS, true};
  else if (item.action == "COMMAND_SHOW_ALL")
    tag_map[item.action] = {IDC_VIV_SHOW_ALL, true};
  // These are ids we test for in app_controller_mac.mm
  else if (item.action == "COMMAND_CHECK_FOR_UPDATES")
    tag_map[item.action] = {IDC_VIV_CHECK_FOR_UPDATES, item.with_no_window};
  else if (item.action == "COMMAND_QUIT_MAC_MAYBE_WARN")
    tag_map[item.action] = {IDC_VIV_EXIT, item.with_no_window};
  else if (item.action == "COMMAND_NEW_WINDOW")
    tag_map[item.action] = {IDC_VIV_NEW_WINDOW, item.with_no_window};
  else if (item.action == "COMMAND_NEW_PRIVATE_WINDOW")
    tag_map[item.action] = {IDC_VIV_NEW_PRIVATE_WINDOW, item.with_no_window};
  else if (item.action == "COMMAND_NEW_PRIVATE_WINDOW")
    tag_map[item.action] = {IDC_VIV_NEW_PRIVATE_WINDOW, item.with_no_window};
  else if (item.action == "COMMAND_NEW_TAB")
    tag_map[item.action] = {IDC_VIV_NEW_TAB, item.with_no_window};
  else if (item.action == "COMMAND_CLOSE_WINDOW")
    tag_map[item.action] = {IDC_VIV_CLOSE_WINDOW, item.with_no_window};
  else if (item.action == "COMMAND_CLOSE_TAB")
    tag_map[item.action] = {IDC_VIV_CLOSE_TAB, item.with_no_window};
  else if (item.action == "COMMAND_WINDOW_MINIMIZE")
    tag_map[item.action] = {IDC_VIV_MAC_MINIMIZE, item.with_no_window};
  // Some menus
  else if (item.action == "MENU_APPLE_APP")
    tag_map[item.action] = {IDC_CHROME_MENU, item.with_no_window};
  else if (item.action == "MENU_EDIT")
    tag_map[item.action] = {IDC_EDIT_MENU, item.with_no_window};
  else if (item.action == "MENU_BOOKMARKS")
    tag_map[item.action] = {IDC_BOOKMARKS_MENU, item.with_no_window};
  else if (item.action == "MENU_WINDOW")
    tag_map[item.action] = {IDC_WINDOW_MENU, item.with_no_window};
  // And containers
  else if (item.action == "CONTAINER_MAC_SERVICES")
    tag_map[item.action] = {IDC_VIV_MAC_SERVICES, item.with_no_window};
  else if (item.action == "CONTAINER_BOOKMARK") {
    tag_map[item.action] = {IDC_VIV_BOOKMARK_CONTAINER, item.with_no_window};
  }
  // And something we could do better
  else if (item.action == "JS_LOCAL_ADD_ACTIVE_TAB_TO_BOOKMARKS")
    tag_map[item.action] = {IDC_VIV_ADD_ACTIVE_TAB_TO_BOOKMARKS,
                            item.with_no_window};
  else
    tag_map[item.action] = {tag_counter++, item.with_no_window};

  return tag_map[item.action].id;
}

bool SetIds(std::vector<menubar::MenuItem>* items, bool addFixedActions) {
  size_t map_size = tag_map.size();
  for (menubar::MenuItem& item : *items) {
    item.id = getIdByAction(item);
    if (item.items) {
      SetIds(item.items.get(), false);
    }
  }
  if (addFixedActions) {
    // Special hardcoding for COMMAND_WINDOW_MINIMIZE due to problems with
    // random multiple minimize calls on some systems. Register it here to
    // ensure proper mapping.
    menubar::MenuItem item;
    item.action = "COMMAND_WINDOW_MINIMIZE";
    item.with_no_window = false;
    (void)getIdByAction(item);
    item.action = "JS_LOCAL_ADD_ACTIVE_TAB_TO_BOOKMARKS";
    item.with_no_window = false;
    (void)getIdByAction(item);
  }
  return map_size != tag_map.size();
}

std::string GetActionById(int id) {
  for (auto it = tag_map.begin(); it != tag_map.end(); ++it) {
    if (it->second.id == id) {
      return it->first;
    }
  }

  return "";
}

// static
void MenubarAPI::UpdateCommandEnabled(CommandUpdater* command_updater) {
  for (auto it = tag_map.begin(); it != tag_map.end(); ++it) {
    command_updater->UpdateCommandEnabled(it->second.id, true);
  }
}

// static
bool MenubarAPI::GetIsEnabledWithNoWindows(int id, bool* enabled) {
  for (auto it = tag_map.begin(); it != tag_map.end(); ++it) {
    if (it->second.id == id) {
      *enabled = it->second.enabled_with_no_window;
      return true;
    }
  }
  return false;
}

// static
bool MenubarAPI::HandleActionById(content::BrowserContext* browser_context,
                                  int window_id,
                                  int command_id,
                                  const std::string& parameter) {
  std::string action = GetActionById(command_id);
  if (action.empty()) {
    return false;
  }

  ::vivaldi::BroadcastEvent(
      menubar::OnActivated::kEventName,
      menubar::OnActivated::Create(window_id, action, parameter),
      browser_context);
  return true;
}

ExtensionFunction::ResponseAction MenubarSetupFunction::Run() {
  auto params = menubar::Setup::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Set up map based on the incoming actions and update id to match this map.
  if (SetIds(&params->items, true)) {
    for (auto* browser : *BrowserList::GetInstance()) {
      chrome::BrowserCommandController* command_controller =
        browser->command_controller();
      command_controller->InitVivaldiCommandState();
    }
  }

#if defined(OS_MAC)
  // There may be no windows. Allow a nullptr profile.
  Profile* profile = Profile::FromBrowserContext(browser_context());
  ::vivaldi::CreateVivaldiMainMenu(profile, &params->items, params->mode, IDC_VIV_DYNAMIC_MENU_ID_START, tag_counter);
  return RespondNow(NoArguments());
#else
  return RespondNow(Error("NOT IMPLEMENTED"));
#endif  // defined(OS_MAC)
}

}  // namespace extensions