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

#include "content/browser/web_contents/web_contents_impl.h"

using vivaldi::kVivaldiAppId;
using content::WebContents;

namespace extensions {

EditcommandExecuteFunction::EditcommandExecuteFunction() {}

EditcommandExecuteFunction::~EditcommandExecuteFunction() {}

bool EditcommandExecuteFunction::RunAsync() {
  WebContents* web_contents = dispatcher()->GetAssociatedWebContents();
  if (web_contents) {
    // NOTE(espen@vivaldi.com): Added with ch67 when we started to use separate
    // documents for ui and page (cross process). The api happens in the ui
    // document while actions can take place in ui or page. So look up the
    // focused WebContents object and use that.
    std::vector<content::WebContentsImpl*> impls =
      content::WebContentsImpl::GetAllWebContents();
    for (std::vector<content::WebContentsImpl*>::iterator it = impls.begin();
         it != impls.end(); ++it) {
      content::WebContentsImpl* impl = *it;
      if (impl->GetAsWebContents() == web_contents) {
        impl = impl->GetFocusedWebContents();
        if (impl) {
          web_contents = impl->GetAsWebContents();
        }
        break;
      }
    }
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
