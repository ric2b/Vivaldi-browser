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

// static
void ContextMenuAPI::RequestMenu(content::BrowserContext* browser_context,
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
  auto params = context_menu::Show::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents)
    return RespondNow(Error("Missing WebContents"));

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

  // We need a web contents that can be used to determine menu position.
  // The sender web contents will with reusable react code be the same
  // for all windows. We have to locate the proper web contents to use.
  Browser* browser = ::vivaldi::FindBrowserByWindowId(
      params->properties.window_id);
  VivaldiBrowserWindow* window = VivaldiBrowserWindow::FromBrowser(browser);
  if (!window)
    return RespondNow(Error("Missing browser window"));
  content::WebContents* window_web_contents = window->web_contents();
  if (!window_web_contents)
    return RespondNow(Error("Missing WebContents"));

  if (web_contents->IsShowingContextMenu()) {
    return RespondNow(
        Error("Attempt to show a Vivaldi context menu while Chromium "
              "context menu is running. Check that oncontextmenu is set "
              "and call preventDefault() to block the standard menu"));
  }

  // The controller deletes itself
  ::vivaldi::ContextMenuController* controller = new
      ::vivaldi::ContextMenuController(
          browser_context(),
          web_contents,
          window_web_contents,
          rv_context_menu,
          std::move(params));
  controller->Show();
  return RespondNow(ArgumentList(context_menu::Show::Results::Create()));
}

}  // namespace extensions
