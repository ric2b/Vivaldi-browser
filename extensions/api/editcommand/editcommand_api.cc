// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
//
// The purpose with this api is to let the JS side execute the same commmands
// from menus (or elsewhere if needed) as shortcuts will do internally
// in chrome. It is string based so we can easily add more commands if needed.

#include "extensions/api/editcommand/editcommand_api.h"

#include <memory>

#include "app/vivaldi_constants.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"

using vivaldi::kVivaldiAppId;

namespace extensions {

using content::WebContents;

// Returns the active web content for the app window so that we can access
// any elemenent in the UI was well as the document area.
static WebContents* webContents() {
  Browser* browser = chrome::FindLastActive();
  WebContents* web_contents =
      browser ? browser->tab_strip_model()->GetActiveWebContents() : nullptr;
  AppWindow* appWindow =
      web_contents ? AppWindowRegistry::Get(web_contents->GetBrowserContext())
                         ->GetCurrentAppWindowForApp(kVivaldiAppId)
                   : nullptr;
  return appWindow ? appWindow->web_contents() : nullptr;
}

EditcommandExecuteFunction::EditcommandExecuteFunction() {}

EditcommandExecuteFunction::~EditcommandExecuteFunction() {}

bool EditcommandExecuteFunction::RunAsync() {
  WebContents* web_contents = webContents();
  if (web_contents) {
    std::unique_ptr<vivaldi::editcommand::Execute::Params> params(
        vivaldi::editcommand::Execute::Params::Create(*args_));

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
  }

  SendResponse(true);
  return true;
}

}  // namespace extensions
