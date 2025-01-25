//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//

#include "extensions/api/menubar/menubar_api.h"

#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "extensions/browser/app_window/app_window.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"

#include "app/vivaldi_command_controller.h"
#include "app/vivaldi_commands.h"
#include "browser/vivaldi_browser_finder.h"
#include "ui/vivaldi_browser_window.h"
#include "extensions/schema/menubar.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/vivaldi_main_menu.h"

#include <map>

namespace extensions {

namespace menubar = vivaldi::menubar;

struct ItemInfo {
  int id;
  bool enabled_with_no_window;
  bool enabled;
};

std::map<std::string, ItemInfo> tag_map;
int tag_counter = IDC_VIV_DYNAMIC_MENU_ID_START;

int getIdByAction(const menubar::MenuItem& item) {
  bool enabled = item.enabled ? *item.enabled : true;
  auto it = tag_map.find(item.action);
  if (it != tag_map.end()) {
    tag_map[item.action].enabled = enabled;
    return tag_map[item.action].id;
  }

  // We need hardcoded ids for some actions. These actions must be mapped as we
  // tests for those elsewhere and even let chromium handle some of them.
  if (item.action == "COMMAND_CLIPBOARD_UNDO")
    tag_map[item.action] = {IDC_CONTENT_CONTEXT_UNDO, false, enabled};
  else if (item.action == "COMMAND_CLIPBOARD_REDO")
    tag_map[item.action] = {IDC_CONTENT_CONTEXT_REDO, false, enabled};
  else if (item.action == "COMMAND_CLIPBOARD_CUT")
    tag_map[item.action] = {IDC_CONTENT_CONTEXT_CUT, false, enabled};
  else if (item.action == "COMMAND_CLIPBOARD_COPY")
    tag_map[item.action] = {IDC_CONTENT_CONTEXT_COPY, false, enabled};
  else if (item.action == "COMMAND_CLIPBOARD_PASTE")
    tag_map[item.action] = {IDC_CONTENT_CONTEXT_PASTE, false, enabled};
  else if (item.action == "COMMAND_DELETE")
    tag_map[item.action] = {IDC_CONTENT_CONTEXT_DELETE, false, enabled};
  else if (item.action == "COMMAND_CLIPBOARD_SELECT_ALL")
    tag_map[item.action] = {IDC_CONTENT_CONTEXT_SELECTALL, false, enabled};
  else if (item.action == "COMMAND_HIDE_VIVALDI")
    tag_map[item.action] = {IDC_HIDE_APP, false, enabled};
  else if (item.action == "COMMAND_HIDE_OTHERS")
    tag_map[item.action] = {IDC_VIV_HIDE_OTHERS, true, enabled};
  else if (item.action == "COMMAND_SHOW_ALL")
    tag_map[item.action] = {IDC_VIV_SHOW_ALL, true, enabled};
  // These are ids we test for in app_controller_mac.mm
  else if (item.action == "COMMAND_CHECK_FOR_UPDATES")
    tag_map[item.action] = {IDC_VIV_CHECK_FOR_UPDATES, item.with_no_window,
                            enabled};
  else if (item.action == "COMMAND_QUIT_MAC_MAYBE_WARN")
    tag_map[item.action] = {IDC_VIV_EXIT, item.with_no_window, enabled};
  else if (item.action == "COMMAND_NEW_WINDOW")
    tag_map[item.action] = {IDC_VIV_NEW_WINDOW, item.with_no_window, enabled};
  else if (item.action == "COMMAND_NEW_PRIVATE_WINDOW")
    tag_map[item.action] = {IDC_VIV_NEW_PRIVATE_WINDOW, item.with_no_window,
                            enabled};
  else if (item.action == "COMMAND_NEW_PRIVATE_WINDOW")
    tag_map[item.action] = {IDC_VIV_NEW_PRIVATE_WINDOW, item.with_no_window,
                            enabled};
  else if (item.action == "COMMAND_NEW_TAB")
    tag_map[item.action] = {IDC_VIV_NEW_TAB, item.with_no_window, enabled};
  else if (item.action == "COMMAND_CLOSE_WINDOW")
    tag_map[item.action] = {IDC_VIV_CLOSE_WINDOW, item.with_no_window, enabled};
  else if (item.action == "COMMAND_CLOSE_TAB")
    tag_map[item.action] = {IDC_VIV_CLOSE_TAB, item.with_no_window, enabled};
  else if (item.action == "COMMAND_WINDOW_MINIMIZE")
    tag_map[item.action] = {IDC_VIV_MAC_MINIMIZE, item.with_no_window, enabled};
  else if (item.action == "COMMAND_SHOW_HELP")
    tag_map[item.action] = {IDC_VIV_SHOW_HELP, item.with_no_window, enabled};
  // Some menus
  else if (item.action == "MENU_APPLE_APP")
    tag_map[item.action] = {IDC_CHROME_MENU, item.with_no_window, enabled};
  else if (item.action == "MENU_EDIT")
    tag_map[item.action] = {IDC_EDIT_MENU, item.with_no_window, enabled};
  else if (item.action == "MENU_BOOKMARKS")
    tag_map[item.action] = {IDC_BOOKMARKS_MENU, item.with_no_window, enabled};
  else if (item.action == "MENU_WINDOW")
    tag_map[item.action] = {IDC_WINDOW_MENU, item.with_no_window, enabled};
  else if (item.action == "MENU_HELP")
    tag_map[item.action] = {IDC_VIV_HELP_MENU, item.with_no_window, enabled};
  // And containers
  else if (item.action == "CONTAINER_MAC_SERVICES")
    tag_map[item.action] = {IDC_VIV_MAC_SERVICES, item.with_no_window, enabled};
  else if (item.action == "CONTAINER_SHARE_MENU")
    tag_map[item.action] = {IDC_VIV_SHARE_MENU_MAC, item.with_no_window, enabled};
  else if (item.action == "CONTAINER_BOOKMARK") {
    tag_map[item.action] = {IDC_VIV_BOOKMARK_CONTAINER, item.with_no_window,
                            enabled};
  }
  // And something we could do better
  else if (item.action == "JS_LOCAL_ADD_ACTIVE_TAB_TO_BOOKMARKS")
    tag_map[item.action] = {IDC_VIV_ADD_ACTIVE_TAB_TO_BOOKMARKS,
                            item.with_no_window, enabled};
  else
    tag_map[item.action] = {tag_counter++, item.with_no_window, enabled};

  return tag_map[item.action].id;
}

bool SetIds(std::vector<menubar::MenuItem>& items, bool addFixedActions) {
  size_t map_size = tag_map.size();
  for (menubar::MenuItem& item : items) {
    item.id = getIdByAction(item);
    if (item.items.has_value()) {
      SetIds(item.items.value(), false);
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
bool MenubarAPI::GetIsEnabled(int id, bool hasWindow, bool* enabled) {
  for (auto it = tag_map.begin(); it != tag_map.end(); ++it) {
    if (it->second.id == id) {
      if (it->second.enabled_with_no_window) {
        *enabled = !hasWindow || it->second.enabled;
      } else {
        *enabled = hasWindow && it->second.enabled;
      }
      return true;
    }
  }
  return false;
}

// static
// This collection should match commandSettingsAcceptlist in CommandStore
// TODO: Remove hardcoding here. Let menu spec pass info instead.
bool MenubarAPI::GetIsSupportedInSettings(int id) {
  switch (id) {
    case IDC_VIV_CLOSE_TAB: // COMMAND_CLOSE_TAB
    case IDC_VIV_CLOSE_WINDOW: // COMMAND_CLOSE_WINDOW
    case IDC_VIV_EXIT: // COMMAND_QUIT_MAC_MAYBE_WARN
    case IDC_VIV_NEW_WINDOW: // COMMAND_NEW_WINDOW
    case IDC_VIV_NEW_PRIVATE_WINDOW: // COMMAND_NEW_PRIVATE_WINDOW
    case IDC_VIV_SHOW_HELP: // COMMAND_SHOW_HELP
      return true;
    default:
      return false;
  }
}

// We have a problem in the Help Menu. The std inlined search function there
// takes focus when menu shows and the test we do in validateUserInterfaceItem()
// in app_controller_app.mm fails because there is no key window. So this
// function is then used to determine if we indeed have an active window to keep
// items in the Help menu enabled.
// static
bool MenubarAPI::HasActiveWindow() {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->window() && browser->window()->IsActive()) {
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

  VivaldiBrowserWindow* window = VivaldiBrowserWindow::FromId(window_id);
  if (window) {
    // VB-107552. Ping renderer code with a message telling
    // user input happens. A blocking blocking menu event loop will prevent
    // automatic updates. Some functions in blink/render require recent
    // input to run (to minimize risk of rouge page code executing those).
    // We may call such functions when selecting a menu item.
    // See user_activation_state.cc - an input notification remains valid
    // for 5 seconds.
    window->web_contents()->GetPrimaryMainFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kInteraction);
  } else {
    LOG(ERROR) << "Menu bar. Failed to look up window";
  }

  ::vivaldi::BroadcastEvent(
      menubar::OnActivated::kEventName,
      menubar::OnActivated::Create(window_id, action, parameter),
      browser_context);
  return true;
}

ExtensionFunction::ResponseAction MenubarGetHasWindowsFunction::Run() {
  return RespondNow(
    ArgumentList(vivaldi::menubar::GetHasWindows::Results::Create(
        BrowserList::GetInstance()->size() > 0)));
}

ExtensionFunction::ResponseAction MenubarSetupFunction::Run() {
  auto params = menubar::Setup::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // Set up map based on the incoming actions and update id to match this map.
  if (SetIds(params->items, true)) {
    for (Browser* browser : *BrowserList::GetInstance()) {
      chrome::BrowserCommandController* command_controller =
          browser->command_controller();
      command_controller->InitVivaldiCommandState();
    }
  }

#if BUILDFLAG(IS_MAC)
  // There may be no windows. Allow a nullptr profile.
  Profile* profile = Profile::FromBrowserContext(browser_context());
  ::vivaldi::CreateVivaldiMainMenu(profile, &params->items,
                                   IDC_VIV_DYNAMIC_MENU_ID_START, tag_counter);
  return RespondNow(NoArguments());
#else
  return RespondNow(Error("NOT IMPLEMENTED"));
#endif  // BUILDFLAG(IS_MAC)
}

}  // namespace extensions