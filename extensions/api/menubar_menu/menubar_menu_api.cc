//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#include "extensions/api/menubar_menu/menubar_menu_api.h"

#include "base/base64.h"
#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/menus/simple_menu_model.h"

#include "browser/menus/vivaldi_menu_enums.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/vivaldi_browser_window.h"
#include "browser/menus/vivaldi_menubar_controller.h"

namespace extensions {

// helper
static bool IsBackgroundCommand(int command) {
  switch (command) {
    case IDC_VIV_BOOKMARK_BAR_OPEN_BACKGROUND_TAB:
      return true;
    default:
      return false;
  }
}

// Helper
static vivaldi::menubar_menu::Disposition CommandToDisposition(int command) {
  switch (command) {
    case IDC_VIV_BOOKMARK_BAR_OPEN_CURRENT_TAB:
      return vivaldi::menubar_menu::Disposition::kCurrent;
    case IDC_VIV_BOOKMARK_BAR_OPEN_NEW_TAB:
    case IDC_VIV_BOOKMARK_BAR_OPEN_BACKGROUND_TAB:
      return vivaldi::menubar_menu::Disposition::kNewTab;
    case IDC_VIV_BOOKMARK_BAR_OPEN_NEW_WINDOW:
      return vivaldi::menubar_menu::Disposition::kNewWindow;
    case IDC_VIV_BOOKMARK_BAR_OPEN_NEW_PRIVATE_WINDOW:
      return vivaldi::menubar_menu::Disposition::kNewPrivateWindow;
    default:
      return vivaldi::menubar_menu::Disposition::kNone;
  }
}

// Helper
static vivaldi::menubar_menu::BookmarkCommand CommandToAction(int command) {
  switch (command) {
    case IDC_VIV_BOOKMARK_BAR_ADD_ACTIVE_TAB:
      return vivaldi::menubar_menu::BookmarkCommand::kAddactivetab;
    case IDC_BOOKMARK_BAR_ADD_NEW_BOOKMARK:
      return vivaldi::menubar_menu::BookmarkCommand::kAddbookmark;
    case IDC_BOOKMARK_BAR_NEW_FOLDER:
      return vivaldi::menubar_menu::BookmarkCommand::kAddfolder;
    case IDC_VIV_BOOKMARK_BAR_NEW_SEPARATOR:
      return vivaldi::menubar_menu::BookmarkCommand::kAddseparator;
    case IDC_BOOKMARK_BAR_EDIT:
      return vivaldi::menubar_menu::BookmarkCommand::kEdit;
    case IDC_CUT:
      return vivaldi::menubar_menu::BookmarkCommand::kCut;
    case IDC_COPY:
      return vivaldi::menubar_menu::BookmarkCommand::kCopy;
    case IDC_PASTE:
      return vivaldi::menubar_menu::BookmarkCommand::kPaste;
    default:
      return vivaldi::menubar_menu::BookmarkCommand::kNone;
  }
}

// Helper
static vivaldi::menubar_menu::EventState FlagToEventState(int flag) {
  vivaldi::menubar_menu::EventState event_state;
  event_state.ctrl = flag & ui::EF_CONTROL_DOWN ? true : false;
  event_state.shift = flag & ui::EF_SHIFT_DOWN ? true : false;
  event_state.alt = flag & ui::EF_ALT_DOWN ? true : false;
  event_state.command = flag & ui::EF_COMMAND_DOWN ? true : false;
  event_state.left = flag & ui::EF_LEFT_MOUSE_BUTTON ? true : false;
  event_state.right = flag & ui::EF_RIGHT_MOUSE_BUTTON ? true : false;
  event_state.center = flag & ui::EF_MIDDLE_MOUSE_BUTTON ? true : false;
  return event_state;
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<MenubarMenuAPI>>::
    DestructorAtExit g_menubar_menu = LAZY_INSTANCE_INITIALIZER;

MenubarMenuAPI::MenubarMenuAPI(content::BrowserContext* context) {}

MenubarMenuAPI::~MenubarMenuAPI() {}

// static
BrowserContextKeyedAPIFactory<MenubarMenuAPI>*
MenubarMenuAPI::GetFactoryInstance() {
  return g_menubar_menu.Pointer();
}

// static
void MenubarMenuAPI::SendAction(content::BrowserContext* browser_context,
                                int command,
                                int event_state,
                                bool persistent) {
  vivaldi::menubar_menu::Action action;
  // Convert to api id before sending to JS.
  action.id = command - IDC_VIV_MENU_FIRST;
  action.state = FlagToEventState(event_state);
  action.persistent = persistent;
  ::vivaldi::BroadcastEvent(vivaldi::menubar_menu::OnAction::kEventName,
                            vivaldi::menubar_menu::OnAction::Create(action),
                            browser_context);
}

// static
void MenubarMenuAPI::SendOpenBookmark(content::BrowserContext* browser_context,
                                      int window_id,
                                      int64_t bookmark_id,
                                      int event_state) {
  vivaldi::menubar_menu::BookmarkAction action;
  action.id = std::to_string(bookmark_id);
  action.disposition = vivaldi::menubar_menu::Disposition::kSetting;
  action.background = false;
  action.state = FlagToEventState(event_state);
  ::vivaldi::BroadcastEvent(
      vivaldi::menubar_menu::OnOpenBookmark::kEventName,
      vivaldi::menubar_menu::OnOpenBookmark::Create(window_id, action),
      browser_context);
}

// static
void MenubarMenuAPI::SendBookmarkAction(
    content::BrowserContext* browser_context,
    int window_id,
    int64_t bookmark_id,
    int command) {
  // Some commands will open a bookmark while the rest are managing actions. If
  // we have a disposition the bookmark should be opened.
  vivaldi::menubar_menu::Disposition disposition =
      CommandToDisposition(command);
  if (disposition != vivaldi::menubar_menu::Disposition::kNone) {
    vivaldi::menubar_menu::BookmarkAction action;
    action.id = std::to_string(bookmark_id);
    action.disposition = disposition;
    action.background = IsBackgroundCommand(command);
    ::vivaldi::BroadcastEvent(
        vivaldi::menubar_menu::OnOpenBookmark::kEventName,
        vivaldi::menubar_menu::OnOpenBookmark::Create(window_id, action),
        browser_context);
  } else {
    vivaldi::menubar_menu::BookmarkAction action;
    action.id = std::to_string(bookmark_id);
    action.command = CommandToAction(command);
    ::vivaldi::BroadcastEvent(
        vivaldi::menubar_menu::OnBookmarkAction::kEventName,
        vivaldi::menubar_menu::OnBookmarkAction::Create(window_id, action),
        browser_context);
  }
}

// static
void MenubarMenuAPI::SendOpen(content::BrowserContext* browser_context,
                              int menu_id) {
  ::vivaldi::BroadcastEvent(vivaldi::menubar_menu::OnOpen::kEventName,
                            vivaldi::menubar_menu::OnOpen::Create(menu_id),
                            browser_context);
}

// static
void MenubarMenuAPI::SendClose(content::BrowserContext* browser_context) {
  ::vivaldi::BroadcastEvent(vivaldi::menubar_menu::OnClose::kEventName,
                            vivaldi::menubar_menu::OnClose::Create(),
                            browser_context);
}

// static
void MenubarMenuAPI::SendHover(content::BrowserContext* browser_context,
                               int window_id,
                               const std::string& url) {
  MenubarMenuAPI* api = GetFactoryInstance()->Get(browser_context);
  DCHECK(api);
  if (!api)
    return;
  if (api->hover_url_ != url) {
    api->hover_url_ = url;
    ::vivaldi::BroadcastEvent(vivaldi::menubar_menu::OnHover::kEventName,
                              vivaldi::menubar_menu::OnHover::Create(window_id,
                                                                     url),
                              browser_context);
  }
}

// static
void MenubarMenuAPI::SendError(content::BrowserContext* browser_context,
                               const std::string& text) {
  ::vivaldi::BroadcastEvent(vivaldi::menubar_menu::OnError::kEventName,
                            vivaldi::menubar_menu::OnError::Create(text),
                            browser_context);
}

MenubarMenuShowFunction::MenubarMenuShowFunction() {}

MenubarMenuShowFunction::~MenubarMenuShowFunction() = default;

ExtensionFunction::ResponseAction MenubarMenuShowFunction::Run() {
#if BUILDFLAG(IS_MAC)
  return RespondNow(Error("Not implemented on Mac"));
#else
  using vivaldi::menubar_menu::Show::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  VivaldiBrowserWindow* window =
    VivaldiBrowserWindow::FromId(params->properties.window_id);
  if (!window) {
    return RespondNow(Error("No such window"));
  }

  // Validate requested menu.
  bool valid_id = false;
  for (const vivaldi::menubar_menu::Menu& m : params->properties.siblings) {
    if (m.id == params->properties.id) {
      valid_id = true;
      break;
    }
  }
  if (!valid_id) {
    return RespondNow(Error("Id out of range"));
  }

  // Controller owns itself.
  ::vivaldi::MenubarController* controller =
    ::vivaldi::MenubarController::Create(window, std::move(params));
  if (!controller->browser()) {
    return RespondNow(Error("Can not show menu"));
  }
  controller->Show();
  return RespondNow(ArgumentList(
      vivaldi::menubar_menu::Show::Results::Create()));
#endif  // IS_MAC
}

ExtensionFunction::ResponseAction MenubarMenuGetMaxIdFunction::Run() {
  return RespondNow(ArgumentList(
      vivaldi::menubar_menu::GetMaxId::Results::Create(
          ::vivaldi::MenubarController::GetMaximumId())));
}

}  // namespace extensions
