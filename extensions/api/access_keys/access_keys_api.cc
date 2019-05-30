// Copyright (c) 2016-2018 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/access_keys/access_keys_api.h"

#include <vector>

#include "app/vivaldi_apptools.h"
#include "extensions/api/tabs/tabs_private_api.h"
#include "extensions/schema/access_keys.h"
#include "renderer/vivaldi_render_messages.h"
#include "ui/vivaldi_ui_utils.h"

namespace extensions {

AccessKeysGetAccessKeysForPageFunction::
    AccessKeysGetAccessKeysForPageFunction() {}

AccessKeysGetAccessKeysForPageFunction::
    ~AccessKeysGetAccessKeysForPageFunction() {}

bool AccessKeysGetAccessKeysForPageFunction::RunAsync() {
  std::unique_ptr<vivaldi::access_keys::GetAccessKeysForPage::Params> params(
      vivaldi::access_keys::GetAccessKeysForPage::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int tab_id = params->tab_id;

  content::WebContents* tabstrip_contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(tab_id,
                                                      browser_context());
  if (tabstrip_contents) {
    VivaldiPrivateTabObserver* tab_api =
        VivaldiPrivateTabObserver::FromWebContents(tabstrip_contents);
    tab_api->GetAccessKeys(tabstrip_contents, base::BindOnce(
        &AccessKeysGetAccessKeysForPageFunction::AccessKeysReceived, this));
    return true;
  }
  SendResponse(false);
  return false;
}

void AccessKeysGetAccessKeysForPageFunction::AccessKeysReceived(
    std::vector<VivaldiViewMsg_AccessKeyDefinition> access_keys) {

  std::vector<vivaldi::access_keys::AccessKeyDefinition> access_key_list;

  for (auto& key: access_keys) {
    vivaldi::access_keys::AccessKeyDefinition entry;

    // The key elements will always be defined, but empty if the elements
    // doesn't contain that attribute
    entry.access_key = key.access_key;
    entry.tagname = key.tagname;
    entry.title = key.title;
    entry.href = key.href;
    entry.value = key.value;
    entry.id = key.id;
    entry.text_content = key.textContent;
    access_key_list.push_back(std::move(entry));
  }

  results_ = vivaldi::access_keys::GetAccessKeysForPage::Results::Create(
      access_key_list);
  SendResponse(true);
}

AccessKeysActionFunction::
    AccessKeysActionFunction() {}

AccessKeysActionFunction::
    ~AccessKeysActionFunction() {}

bool AccessKeysActionFunction::RunAsync() {
  std::unique_ptr<vivaldi::access_keys::Action::Params> params(
      vivaldi::access_keys::Action::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int tab_id = params->tab_id;
  std::string id = params->id;

  content::WebContents* tabstrip_contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(tab_id,
                                                      browser_context());
  if (tabstrip_contents) {
    VivaldiPrivateTabObserver* tab_api =
        VivaldiPrivateTabObserver::FromWebContents(tabstrip_contents);
    tab_api->AccessKeyAction(tabstrip_contents, id);
    SendResponse(true);
    return true;
  }
  SendResponse(false);
  return false;
}

} //extensions
