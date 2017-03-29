//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#include "chrome/browser/extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"

#include <string>

#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

namespace extensions {

bool UtilitiesIsTabInLastSessionFunction::RunAsync() {
  scoped_ptr<api::utilities::IsTabInLastSession::Params> params(
      api::utilities::IsTabInLastSession::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int tabId;
  if(!base::StringToInt(params->tab_id, &tabId))
    return false;

  bool is_in_session = false;
  content::WebContents* contents;

  if (!ExtensionTabUtil::GetTabById(tabId, GetProfile(), true, nullptr, nullptr,
                                    &contents, nullptr)) {
    error_ = "TabId not found.";
    return false;
  }
  // Both the profile and navigation entries are marked if they are
  // loaded from a session, so check both.
  if (GetProfile()->restored_last_session()) {
    content::NavigationEntry* entry =
        contents->GetController().GetVisibleEntry();
    is_in_session = entry && entry->IsRestored();
  }
  results_ =
      api::utilities::IsTabInLastSession::Results::Create(is_in_session);

  SendResponse(true);
  return true;
}

UtilitiesIsTabInLastSessionFunction::UtilitiesIsTabInLastSessionFunction() {
}

UtilitiesIsTabInLastSessionFunction::~UtilitiesIsTabInLastSessionFunction() {
}

}  // namespace extensions
