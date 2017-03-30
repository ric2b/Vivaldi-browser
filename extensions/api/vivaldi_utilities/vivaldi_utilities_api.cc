//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"

#include <string>
#include <vector>
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "url/url_constants.h"

#include <iostream>

namespace extensions {

VivaldiUtilitiesAPI::VivaldiUtilitiesAPI(content::BrowserContext *context)
    : browser_context_(context) {
  extensions::AppWindowRegistry::Get(context)->AddObserver(this);
}

VivaldiUtilitiesAPI::~VivaldiUtilitiesAPI() {
}

void VivaldiUtilitiesAPI::Shutdown() {
  extensions::AppWindowRegistry::Get(browser_context_)->RemoveObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<VivaldiUtilitiesAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<VivaldiUtilitiesAPI> *
VivaldiUtilitiesAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

Browser *VivaldiUtilitiesAPI::FindBrowserFromAppWindowId(
    const std::string &appwindow_id) {
  WindowIdToAppWindowId::const_iterator iter =
      appwindow_id_to_window_id_.find(appwindow_id);
  if (iter == appwindow_id_to_window_id_.end())
    return nullptr;  // not a window

  int window_id = iter->second;
  for (auto* browser: *BrowserList::GetInstance()) {
    if (ExtensionTabUtil::GetWindowId(browser) == window_id &&
        browser->window()) {
      return browser;
    }
  }
  return nullptr;
}

void VivaldiUtilitiesAPI::MapAppWindowIdToWindowId(
    const std::string &appwindow_id, int window_id) {
  appwindow_id_to_window_id_.insert(std::make_pair(appwindow_id, window_id));
}

void VivaldiUtilitiesAPI::OnAppWindowActivated(
    extensions::AppWindow *app_window) {
  Browser* browser = FindBrowserFromAppWindowId(app_window->window_key());
  if (browser) {
    // Activate the found browser but don't call Activate as that will call back
    // to the app window again.
    BrowserList::SetLastActive(browser);
  }
}

void VivaldiUtilitiesAPI::OnAppWindowRemoved(extensions::AppWindow* app_window) {
  appwindow_id_to_window_id_.erase(app_window->window_key());
}


namespace ClearAllRecentlyClosedSessions =
    vivaldi::utilities::ClearAllRecentlyClosedSessions;

bool UtilitiesIsTabInLastSessionFunction::RunAsync() {
  scoped_ptr<vivaldi::utilities::IsTabInLastSession::Params> params(
      vivaldi::utilities::IsTabInLastSession::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int tabId;
  if (!base::StringToInt(params->tab_id, &tabId))
    return false;

  bool is_in_session = false;
  content::WebContents *contents;

  if (!ExtensionTabUtil::GetTabById(tabId, GetProfile(), true, nullptr, nullptr,
                                    &contents, nullptr)) {
    error_ = "TabId not found.";
    return false;
  }
  // Both the profile and navigation entries are marked if they are
  // loaded from a session, so check both.
  if (GetProfile()->restored_last_session()) {
    content::NavigationEntry *entry =
        contents->GetController().GetVisibleEntry();
    is_in_session = entry && entry->IsRestored();
  }
  results_ =
      vivaldi::utilities::IsTabInLastSession::Results::Create(is_in_session);

  SendResponse(true);
  return true;
}

UtilitiesIsTabInLastSessionFunction::UtilitiesIsTabInLastSessionFunction() {}

UtilitiesIsTabInLastSessionFunction::~UtilitiesIsTabInLastSessionFunction() {}

bool UtilitiesIsUrlValidFunction::RunSync() {
  scoped_ptr<vivaldi::utilities::IsUrlValid::Params> params(
      vivaldi::utilities::IsUrlValid::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL url(params->url);

  vivaldi::utilities::UrlValidResults result;
  result.url_valid = !params->url.empty() && url.is_valid();
  result.scheme_valid = URLPattern::IsValidSchemeForExtensions(url.scheme()) ||
                        url.SchemeIs(url::kJavaScriptScheme) ||
                        url.SchemeIs(url::kDataScheme) ||
                        url.SchemeIs(url::kMailToScheme) ||
                        url.spec() == url::kAboutBlankURL;

  results_ = vivaldi::utilities::IsUrlValid::Results::Create(result);

  return true;
}

UtilitiesIsUrlValidFunction::UtilitiesIsUrlValidFunction() {}

UtilitiesIsUrlValidFunction::~UtilitiesIsUrlValidFunction() {}

UtilitiesClearAllRecentlyClosedSessionsFunction::
    ~UtilitiesClearAllRecentlyClosedSessionsFunction() {
}

bool UtilitiesClearAllRecentlyClosedSessionsFunction::RunAsync() {
  sessions::TabRestoreService *tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(GetProfile());
  bool result = false;
  if (tab_restore_service) {
    result = true;
    tab_restore_service->ClearEntries();
  }
  results_ = ClearAllRecentlyClosedSessions::Results::Create(result);
  return result;
}

UtilitiesGetAvailablePageEncodingsFunction::
    ~UtilitiesGetAvailablePageEncodingsFunction() {
}

bool UtilitiesGetAvailablePageEncodingsFunction::RunSync() {
  const std::vector<CharacterEncoding::EncodingInfo> *encodings;
  PrefService *pref_service = GetProfile()->GetPrefs();
  encodings = CharacterEncoding::GetCurrentDisplayEncodings(
      g_browser_process->GetApplicationLocale(),
      pref_service->GetString(prefs::kStaticEncodings),
      pref_service->GetString(prefs::kRecentlySelectedEncoding));
  DCHECK(encodings);
  DCHECK(!encodings->empty());

  std::vector<vivaldi::utilities::EncodingItem> encodingItems;

  std::vector<CharacterEncoding::EncodingInfo>::const_iterator it;
  for (it = encodings->begin(); it != encodings->end(); ++it) {
    if (it->encoding_id) {
      vivaldi::utilities::EncodingItem *encodingItem =
          new vivaldi::utilities::EncodingItem;
      int cmd_id = it->encoding_id;
      std::string encoding =
          CharacterEncoding::GetCanonicalEncodingNameByCommandId(cmd_id);
      encodingItem->name = base::UTF16ToUTF8(it->encoding_display_name);
      encodingItem->encoding = encoding;
      encodingItems.push_back(std::move(*encodingItem));
    }
  }

  results_ = vivaldi::utilities::GetAvailablePageEncodings::Results::Create(
      encodingItems);
  return true;
}

UtilitiesMapFocusAppWindowToWindowIdFunction::UtilitiesMapFocusAppWindowToWindowIdFunction() {

}

UtilitiesMapFocusAppWindowToWindowIdFunction::~UtilitiesMapFocusAppWindowToWindowIdFunction() {
}

bool UtilitiesMapFocusAppWindowToWindowIdFunction::RunSync() {
  scoped_ptr<vivaldi::utilities::MapFocusAppWindowToWindowId::Params> params(
      vivaldi::utilities::MapFocusAppWindowToWindowId::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  VivaldiUtilitiesAPI *api =
      VivaldiUtilitiesAPI::GetFactoryInstance()->Get(GetProfile());

  api->MapAppWindowIdToWindowId(params->app_window_id, params->window_id);

  return true;
}

}  // namespace extensions
