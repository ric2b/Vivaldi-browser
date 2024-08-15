// Copyright (c) 2016-2018 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/access_keys/access_keys_api.h"

#include <vector>

#include "app/vivaldi_apptools.h"
#include "extensions/api/tabs/tabs_private_api.h"
#include "extensions/schema/access_keys.h"
#include "ui/vivaldi_ui_utils.h"

namespace extensions {

ExtensionFunction::ResponseAction
AccessKeysGetAccessKeysForPageFunction::Run() {
  using vivaldi::access_keys::GetAccessKeysForPage::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;
  VivaldiPrivateTabObserver* tab_api = VivaldiPrivateTabObserver::FromTabId(
      browser_context(), params->tab_id, &error);
  if (!tab_api)
    return RespondNow(Error(error));

  tab_api->GetAccessKeys(base::BindOnce(
      &AccessKeysGetAccessKeysForPageFunction::AccessKeysReceived, this));

  return RespondLater();
}

void AccessKeysGetAccessKeysForPageFunction::AccessKeysReceived(
    std::vector<::vivaldi::mojom::AccessKeyPtr> access_keys) {
  namespace Results = vivaldi::access_keys::GetAccessKeysForPage::Results;

  std::vector<vivaldi::access_keys::AccessKeyDefinition> access_key_list;

  for (auto& key : access_keys) {
    vivaldi::access_keys::AccessKeyDefinition entry;

    // The key elements will always be defined, but empty if the elements
    // doesn't contain that attribute
    entry.access_key = key->access_key;
    entry.tagname = key->tagname;
    entry.title = key->title;
    entry.href = key->href;
    entry.value = key->value;
    entry.id = key->id;
    entry.text_content = key->text_content;
    access_key_list.push_back(std::move(entry));
  }

  Respond(ArgumentList(Results::Create(access_key_list)));
}

ExtensionFunction::ResponseAction AccessKeysActionFunction::Run() {
  using vivaldi::access_keys::Action::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string id = params->id;

  std::string error;
  VivaldiPrivateTabObserver* tab_api = VivaldiPrivateTabObserver::FromTabId(
      browser_context(), params->tab_id, &error);
  if (!tab_api)
    return RespondNow(Error(error));

  tab_api->AccessKeyAction(id);

  return RespondNow(NoArguments());
}

}  // namespace extensions
