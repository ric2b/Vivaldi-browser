//
// Copyright (c) 2014-2019 Vivaldi Technologies AS. All rights reserved.
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "extensions/api/context_menu/context_menu_api.h"

#include "content/public/browser/web_contents.h"
#include "extensions/api/menubar_menu/menubar_menu_api.h"
#include "ui/vivaldi_browser_window.h"
#include "browser/vivaldi_browser_finder.h"

namespace extensions {

namespace context_menu = vivaldi::context_menu;

ContextMenuShowFunction::ContextMenuShowFunction() {}
ContextMenuShowFunction::~ContextMenuShowFunction() {}

ExtensionFunction::ResponseAction ContextMenuShowFunction::Run() {
  auto params = context_menu::Show::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents)
    return RespondNow(Error("Missing WebContents"));

  // We need a web contents that can be used to determine menu position.
  // The sender web contents will with reusable react code be the same
  // for all windows. We have to locate the proper web contents to use.
  Browser* browser = ::vivaldi::FindBrowserByWindowId(
      params->properties.window_id);
  content::WebContents* window_web_contents = browser ?
      static_cast<VivaldiBrowserWindow*>(browser->window())->web_contents() :
      nullptr;
  if (!window_web_contents)
    return RespondNow(Error("Missing WebContents"));

  if (web_contents->IsShowingContextMenu()) {
    return RespondNow(
        Error("Attempt to show a Vivaldi context menu while Chromium "
              "context menu is running. Check that oncontextmenu is set "
              "and call preventDefault() to block the standard menu"));
  }

  controller_.reset(new ::vivaldi::ContextMenuController(this,
                                                         web_contents,
                                                         window_web_contents,
                                                         std::move(params)));
  controller_->Show();

  AddRef();

  return RespondLater();
}

void ContextMenuShowFunction::OnAction(int command, int event_state) {
  MenubarMenuAPI::SendAction(browser_context(), command, event_state);
}

void ContextMenuShowFunction::OnHover(const std::string& url) {
  MenubarMenuAPI::SendHover(browser_context(), url);
}

void ContextMenuShowFunction::OnOpened() {
  MenubarMenuAPI::SendOpen(browser_context(), 0);
}

void ContextMenuShowFunction::OnClosed() {
  MenubarMenuAPI::SendClose(browser_context());
  Respond(ArgumentList(context_menu::Show::Results::Create()));
  Release();
}

}  // namespace extensions
