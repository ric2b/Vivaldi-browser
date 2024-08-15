// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
//
// The purpose with this api is to let the JS side execute the same commmands
// from menus (or elsewhere if needed) as shortcuts will do internally
// in chrome. It is string based so we can easily add more commands if needed.

#include "extensions/api/editcommand/editcommand_api.h"

#include "content/browser/web_contents/web_contents_impl.h"  // nogncheck

#include "app/vivaldi_constants.h"
#include "ui/vivaldi_browser_window.h"

namespace extensions {

ExtensionFunction::ResponseAction EditcommandExecuteFunction::Run() {
  using vivaldi::editcommand::Execute::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  VivaldiBrowserWindow* window =
      VivaldiBrowserWindow::FromId(params->window_id);
  if (!window) {
    return RespondNow(Error("No such window"));
  }
  content::WebContents* web_contents =
      static_cast<content::WebContentsImpl*>(window->web_contents())
          ->GetFocusedWebContents();

  if (params->command == "undo")
    web_contents->Undo();
  else if (params->command == "redo")
    web_contents->Redo();
  else if (params->command == "cut")
    web_contents->Cut();
  else if (params->command == "copy")
    web_contents->Copy();
  else if (params->command == "paste")
    web_contents->Paste();
  else if (params->command == "selectAll")
    web_contents->SelectAll();
  else if (params->command == "pasteAndMatchStyle")
    web_contents->PasteAndMatchStyle();

  return RespondNow(
      ArgumentList(vivaldi::editcommand::Execute::Results::Create(true)));
}

}  // namespace extensions
