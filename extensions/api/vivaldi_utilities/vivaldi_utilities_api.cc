//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "prefs/vivaldi_pref_names.h"
#include "ui/vivaldi_ui_utils.h"
#include "url/url_constants.h"

namespace {
bool IsValidUserId(const std::string& user_id) {
  uint64_t value;
  return !user_id.empty() && base::HexStringToUInt64(user_id, &value) &&
         value > 0;
}
}  // anonymous namespace

namespace extensions {

VivaldiUtilitiesEventRouter::VivaldiUtilitiesEventRouter(Profile* profile)
    : browser_context_(profile) {
}

VivaldiUtilitiesEventRouter::~VivaldiUtilitiesEventRouter() {
}

void VivaldiUtilitiesEventRouter::DispatchEvent(const std::string& event_name,
                                 std::unique_ptr<base::ListValue> event_args) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    event_router->BroadcastEvent(base::WrapUnique(
        new extensions::Event(extensions::events::VIVALDI_EXTENSION_EVENT,
                              event_name, std::move(event_args))));
  }
}

VivaldiUtilitiesAPI::VivaldiUtilitiesAPI(content::BrowserContext *context)
    : browser_context_(context) {
  extensions::AppWindowRegistry::Get(context)->AddObserver(this);

  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(
      this, vivaldi::utilities::OnScroll::kEventName);
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
  for (auto* browser : *BrowserList::GetInstance()) {
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

void VivaldiUtilitiesAPI::OnAppWindowRemoved(
    extensions::AppWindow* app_window) {
  appwindow_id_to_window_id_.erase(app_window->window_key());
}

void VivaldiUtilitiesAPI::ScrollType(int scrollType) {
  if (event_router_) {
    event_router_->DispatchEvent(
        vivaldi::utilities::OnScroll::kEventName,
        vivaldi::utilities::OnScroll::Create(scrollType));
  }
}

void VivaldiUtilitiesAPI::OnListenerAdded(const EventListenerInfo& details) {
  event_router_.reset(new VivaldiUtilitiesEventRouter(
      Profile::FromBrowserContext(browser_context_)));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

namespace ClearAllRecentlyClosedSessions =
    vivaldi::utilities::ClearAllRecentlyClosedSessions;

UtilitiesBasicPrintFunction::UtilitiesBasicPrintFunction() {}
UtilitiesBasicPrintFunction::~UtilitiesBasicPrintFunction() {}

bool UtilitiesBasicPrintFunction::RunAsync() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  Browser *browser = chrome::FindAnyBrowser(profile, true);
  chrome::BasicPrint(browser);

  SendResponse(true);
  return true;
}

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
  SendResponse(result);
  return result;
}

UtilitiesGetUniqueUserIdFunction::~UtilitiesGetUniqueUserIdFunction() {
}

bool UtilitiesGetUniqueUserIdFunction::RunAsync() {
  std::unique_ptr<vivaldi::utilities::GetUniqueUserId::Params> params(
      vivaldi::utilities::GetUniqueUserId::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string user_id = g_browser_process->local_state()->GetString(
      vivaldiprefs::kVivaldiUniqueUserId);

  if (!IsValidUserId(user_id)) {
    content::BrowserThread::PostTask(
        content::BrowserThread::FILE, FROM_HERE,
        base::Bind(
            &UtilitiesGetUniqueUserIdFunction::GetUniqueUserIdOnFileThread,
            this, params->legacy_user_id));
  } else {
    RespondOnUIThread(user_id, false);
  }

  return true;
}

void UtilitiesGetUniqueUserIdFunction::GetUniqueUserIdOnFileThread(
    const std::string& legacy_user_id) {
  // Note: We do not refresh the copy of the user id stored in the OS profile
  // if it is missing because we do not want standalone copies of vivaldi on USB
  // to spread their user id to all the computers they are run on.

  std::string user_id;
  bool is_new_user = false;
  if (!ReadUserIdFromOSProfile(&user_id) || !IsValidUserId(user_id)) {
    if (IsValidUserId(legacy_user_id)) {
      user_id = legacy_user_id;
    } else {
      uint64_t random = base::RandUint64();
      user_id = base::StringPrintf("%016" PRIX64, random);
      is_new_user = true;
    }
    WriteUserIdToOSProfile(user_id);
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&UtilitiesGetUniqueUserIdFunction::RespondOnUIThread, this,
                 user_id, is_new_user));
}

void UtilitiesGetUniqueUserIdFunction::RespondOnUIThread(
    const std::string& user_id,
    bool is_new_user) {
  g_browser_process->local_state()->SetString(
      vivaldiprefs::kVivaldiUniqueUserId, user_id);

  results_ = vivaldi::utilities::GetUniqueUserId::Results::Create(user_id,
                                                                  is_new_user);
  SendResponse(true);
}

bool UtilitiesIsTabInLastSessionFunction::RunAsync() {
  std::unique_ptr<vivaldi::utilities::IsTabInLastSession::Params> params(
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

bool UtilitiesIsUrlValidFunction::RunAsync() {
  std::unique_ptr<vivaldi::utilities::IsUrlValid::Params> params(
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

  SendResponse(true);
  return true;
}

UtilitiesGetSelectedTextFunction::UtilitiesGetSelectedTextFunction() {}

UtilitiesGetSelectedTextFunction::~UtilitiesGetSelectedTextFunction() {}

bool UtilitiesGetSelectedTextFunction::RunAsync() {
  std::unique_ptr<vivaldi::utilities::GetSelectedText::Params> params(
      vivaldi::utilities::GetSelectedText::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int tabId;
  if (!base::StringToInt(params->tab_id, &tabId))
    return false;

  std::string text;
  content::WebContents* web_contents =
    ::vivaldi::ui_tools::GetWebContentsFromTabStrip(tabId, GetProfile());
  if (web_contents) {
    content::RenderWidgetHostView* rwhv =
      web_contents->GetRenderWidgetHostView();
    if (rwhv) {
      text = base::UTF16ToUTF8(rwhv->GetSelectedText());
    }
  }

  results_ = vivaldi::utilities::GetSelectedText::Results::Create(text);

  SendResponse(true);
  return true;
}

UtilitiesIsUrlValidFunction::UtilitiesIsUrlValidFunction() {}

UtilitiesIsUrlValidFunction::~UtilitiesIsUrlValidFunction() {}

UtilitiesMapFocusAppWindowToWindowIdFunction::
    UtilitiesMapFocusAppWindowToWindowIdFunction() {}

UtilitiesMapFocusAppWindowToWindowIdFunction::
    ~UtilitiesMapFocusAppWindowToWindowIdFunction() {}

bool UtilitiesMapFocusAppWindowToWindowIdFunction::RunAsync() {
  std::unique_ptr<vivaldi::utilities::MapFocusAppWindowToWindowId::Params>
      params(vivaldi::utilities::MapFocusAppWindowToWindowId::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  VivaldiUtilitiesAPI *api =
      VivaldiUtilitiesAPI::GetFactoryInstance()->Get(GetProfile());

  api->MapAppWindowIdToWindowId(params->app_window_id, params->window_id);

  SendResponse(true);
  return true;
}

}  // namespace extensions
