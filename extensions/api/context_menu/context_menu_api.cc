//
// Copyright (c) 2014-2019 Vivaldi Technologies AS. All rights reserved.
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "extensions/api/context_menu/context_menu_api.h"

#include "browser/menus/vivaldi_context_menu_controller.h"
#include "browser/menus/vivaldi_render_view_context_menu.h"
#include "browser/vivaldi_browser_finder.h"
#include "content/public/browser/web_contents.h"
#include "extensions/api/menubar_menu/menubar_menu_api.h"
#include "extensions/schema/context_menu.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/vivaldi_browser_window.h"

namespace extensions {

namespace context_menu = vivaldi::context_menu;


// How it works
// * Menus from UI. The ContextMenuShowFunction() will create a
// ContextMenuController instance which will build the menu model based on the
// provided parameters from JS. The controller will then make a platform menu
// using the model so that chrome code can set it up.
// * Menus from a page. In chrome code we create a VivaldiRenderViewContextMenu
// instance. It examines the provided parameters from chrome to make a state
// object we pass to JS using  ContextMenuAPI::RequestMenu(). JS will then use
// that information to set up menu content and pass it to C++ again (here). From
// then on handling is for the most part tha same as with "Menus from UI" above
// (some exceptions where we test for the VivaldiRenderViewContextMenu instance).

// static
void ContextMenuAPI::RequestMenu(
    content::BrowserContext* browser_context,
    int window_id,
    int document_id,
    const vivaldi::context_menu::DocumentParams& request) {
  ::vivaldi::BroadcastEvent(
      context_menu::OnDocumentMenu::kEventName,
      context_menu::OnDocumentMenu::Create(window_id, document_id, request),
      browser_context);
}

namespace context_menu = vivaldi::context_menu;

ContextMenuShowFunction::ContextMenuShowFunction() = default;
ContextMenuShowFunction::~ContextMenuShowFunction() = default;

ExtensionFunction::ResponseAction ContextMenuShowFunction::Run() {
  auto params = context_menu::Show::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  VivaldiBrowserWindow* window =
      VivaldiBrowserWindow::FromId(params->properties.window_id);
  if (!window) {
    return RespondNow(Error("No such window"));
  }

  ::vivaldi::VivaldiRenderViewContextMenu* rv_context_menu = nullptr;
  if (params->properties.document_id >= 0) {
    // We are handling a document menu that has been requested from C++ by
    // VivaldiRenderViewContextMenu::InitMenu(). This is an async operation so
    // we use the document_id to match the active pending menu. If the owner of
    // this menu has removed it, or replaced it with a new (new id), we can not
    // use it.
    rv_context_menu = ::vivaldi::VivaldiRenderViewContextMenu::GetActive(
        params->properties.document_id);
    if (rv_context_menu) {
      // TODO (espen). Send these coordinates with the initial request to JS
      // once we have better options support in context-menus.js.
      params->properties.rect.x = rv_context_menu->params().x;
      params->properties.rect.y = rv_context_menu->params().y;
    } else {
      return RespondNow(Error("Missing document controller"));
    }
  }

  if (window->web_contents()->IsShowingContextMenu()) {
    return RespondNow(
        Error("Attempt to show a Vivaldi context menu while Chromium "
              "context menu is running. Check that oncontextmenu is set "
              "and call preventDefault() to block the standard menu"));
  }

  ::vivaldi::ContextMenuController::Create(window,
                                           rv_context_menu,
                                           std::move(params))->Show();

  return RespondNow(ArgumentList(context_menu::Show::Results::Create()));
}

ExtensionFunction::ResponseAction ContextMenuUpdateFunction::Run() {
  auto params = context_menu::Update::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  ::vivaldi::ContextMenuController* controller =
    ::vivaldi::ContextMenuController::GetActive();
  if (controller) {
    controller->Update(params->properties);
  }
  return RespondNow(ArgumentList(context_menu::Update::Results::Create()));
}


}  // namespace extensions
