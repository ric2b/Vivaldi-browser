// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/settings/settings_api.h"

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing_db/safe_browsing_prefs.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/schema/settings.h"
#include "prefs/vivaldi_browser_prefs.h"
#include "prefs/vivaldi_pref_names.h"

namespace vivaldi {

NativeSettingsObserverDelegate::~NativeSettingsObserverDelegate() {

}

} // namespace vivaldi

namespace extensions {

using vivaldi::settings::PreferenceItem;

struct PrefsMapping {
  const char* prefs_name;
  PrefType prefs_type;
};

struct PrefsMapping kPrefsValues[] {
  { prefs::kAlternateErrorPagesEnabled, booleanPreftype },
  { prefs::kSafeBrowsingExtendedReportingEnabled, booleanPreftype },
  { prefs::kSafeBrowsingEnabled, booleanPreftype },
  { prefs::kEnableDoNotTrack, booleanPreftype },
  { prefs::kSearchSuggestEnabled, booleanPreftype },
  { prefs::kWebKitMinimumFontSize, integerPreftype },
  { prefs::kDefaultCharset, stringPreftype },
  { password_manager::prefs::kPasswordManagerSavingEnabled, booleanPreftype },
  { password_manager::prefs::kCredentialsEnableService, booleanPreftype },

  // Download preferences
  { prefs::kDownloadDefaultDirectory, stringPreftype },

  // Privacy preferences
  { prefs::kWebRTCIPHandlingPolicy, stringPreftype },

  // vivaldi preferences
  { vivaldiprefs::kMousegesturesEnabled, booleanPreftype },
  { vivaldiprefs::kRockerGesturesEnabled, booleanPreftype },
  { vivaldiprefs::kSmoothScrollingEnabled, booleanPreftype },
  // Controls if zoom values are re-used in tab when navigating between hosts
  { vivaldiprefs::kVivaldiTabZoom, booleanPreftype },
  { vivaldiprefs::kVivaldiHomepage, stringPreftype },
  { vivaldiprefs::kVivaldiNumberOfDaysToKeepVisits, numberPreftype },

  // Plugin preferences
  { prefs::kPluginsAlwaysOpenPdfExternally, booleanPreftype },
  { vivaldiprefs::kPluginsWidevideEnabled, booleanPreftype },

  // Control if tab only cycles input fields and tab index fields or not.
  { vivaldiprefs::kVivaldiTabsToLinks, booleanPreftype },

  // Wait until a restored tab is activated before loading the page.
  { vivaldiprefs::kDeferredTabLoadingAfterRestore, booleanPreftype },
  // Always load restored pinned tabs.
  { vivaldiprefs::kAlwaysLoadPinnedTabAfterRestore, booleanPreftype },
  // Enable autoupdate.
  { vivaldiprefs::kAutoUpdateEnabled, booleanPreftype },

  // Startup preferences:
  // An integer pref. Holds one of several values:
  // 0: (deprecated) open the homepage on startup.
  // 1: restore the last session.
  // 2: this was used to indicate a specific session should be restored. It is
  //    no longer used, but saved to avoid conflict with old preferences.
  // 3: unused, previously indicated the user wants to restore a saved session.
  // 4: restore the URLs defined in kURLsToRestoreOnStartup.
  // 5: open the New Tab Page on startup.
  //
  // Mostly the same as chrome.importData.getStartupAction().
  { prefs::kRestoreOnStartup, numberPreftype },
  { prefs::kURLsToRestoreOnStartup, arrayPreftype },

  // Used to store active feature flags (experimental features).
  { vivaldiprefs::kVivaldiExperiments, arrayPreftype },

#if defined(USE_AURA)
  { vivaldiprefs::kHideMouseCursorInFullscreen, booleanPreftype },
#endif //USE_AURA

#if defined(OS_MACOSX)
  { vivaldiprefs::kAppleKeyboardUIMode, numberPreftype },
  { vivaldiprefs::kAppleMiniaturizeOnDoubleClick, booleanPreftype },
  { vivaldiprefs::kAppleAquaColorVariant, numberPreftype },
  { vivaldiprefs::kAppleInterfaceStyle, stringPreftype },
  { vivaldiprefs::kAppleActionOnDoubleClick, stringPreftype },
  { vivaldiprefs::kSwipeScrollDirection, booleanPreftype },
#endif
};

namespace {
std::unique_ptr<vivaldi::settings::PreferenceItem> GetPref(Profile* profile,
                                                  const std::string prefName,
                                                  PrefType type) {
  std::unique_ptr<vivaldi::settings::PreferenceItem> prefItem(
      new vivaldi::settings::PreferenceItem);

  switch (type) {
    case booleanPreftype: {
      prefItem->preference_key = prefName;
      prefItem->preference_value.boolean.reset(
          new bool(profile->GetPrefs()->GetBoolean(prefName)));
      prefItem->preference_type =
          vivaldi::settings::PREFERENCE_TYPE_ENUM_BOOLEAN;
    } break;
    case stringPreftype: {
      prefItem->preference_key = prefName;
      prefItem->preference_value.string.reset(
          new std::string(profile->GetPrefs()->GetString(prefName)));
      prefItem->preference_type =
          vivaldi::settings::PREFERENCE_TYPE_ENUM_STRING;
    } break;
    case numberPreftype: {
      prefItem->preference_key = prefName;
      prefItem->preference_value.number.reset(
          new double(profile->GetPrefs()->GetDouble(prefName)));
      prefItem->preference_type =
          vivaldi::settings::PREFERENCE_TYPE_ENUM_NUMBER;
    } break;
    case integerPreftype: {
      prefItem->preference_key = prefName;
      prefItem->preference_value.integer.reset(
          new int(profile->GetPrefs()->GetInteger(prefName)));
      prefItem->preference_type =
          vivaldi::settings::PREFERENCE_TYPE_ENUM_INTEGER;
    } break;
    case arrayPreftype: {
      const base::ListValue* url_list = profile->GetPrefs()->GetList(prefName);
      prefItem->preference_key = prefName;
      prefItem->preference_value.array.reset(new std::vector<std::string>());
      for (size_t i = 0; i < url_list->GetSize(); ++i) {
        std::string url_text;
        if (url_list->GetString(i, &url_text)) {
          prefItem->preference_value.array->push_back(url_text);
        }
      }
      prefItem->preference_type = vivaldi::settings::PREFERENCE_TYPE_ENUM_ARRAY;
    } break;
    default:
    case unknownPreftype: {
      // Should never happen.
      NOTREACHED();
    } break;
  }
  return prefItem;
}

PrefType FindTypeForPrefsName(const std::string& name) {
  for (size_t i = 0; i < arraysize(kPrefsValues); i++) {
    if (name == kPrefsValues[i].prefs_name) {
      return kPrefsValues[i].prefs_type;
    }
  }
  return unknownPreftype;
}

}  // namespace

/*static*/
void VivaldiSettingsApiNotification::BroadcastEvent(
    const std::string& eventname,
    std::unique_ptr<base::ListValue>& args,
    content::BrowserContext* context) {
  std::unique_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::VIVALDI_EXTENSION_EVENT, eventname, std::move(args)));
  event->restrict_to_browser_context = context;
  EventRouter* event_router = EventRouter::Get(context);
  if (event_router) {
    event_router->BroadcastEvent(std::move(event));
  }
}

void VivaldiSettingsApiNotification::OnChanged(
    const std::string& pref_changed) {
  PrefType type = FindTypeForPrefsName(pref_changed);
  if (type != unknownPreftype) {
    std::unique_ptr<vivaldi::settings::PreferenceItem> item(
        GetPref(profile_, pref_changed.c_str(), type));

    std::unique_ptr<base::ListValue> args =
        vivaldi::settings::OnChanged::Create(*item.release());
    BroadcastEvent(vivaldi::settings::OnChanged::kEventName, args,
                   profile_);
  }
}

VivaldiSettingsApiNotification::VivaldiSettingsApiNotification(Profile* profile)
    : profile_(profile),
      weak_ptr_factory_(this) {
  prefs_registrar_.Init(profile->GetPrefs());
  for (size_t i = 0; i < arraysize(kPrefsValues); i++) {
    prefs_registrar_.Add(
        kPrefsValues[i].prefs_name,
        base::Bind(&VivaldiSettingsApiNotification::OnChanged,
                   base::Unretained(this)));
  }
  native_settings_observer_.reset(
      ::vivaldi::NativeSettingsObserver::Create(this));
}

VivaldiSettingsApiNotification::VivaldiSettingsApiNotification()
    : profile_(nullptr), weak_ptr_factory_(this) {
}

VivaldiSettingsApiNotification::~VivaldiSettingsApiNotification() {
}

// NativeSettingsObserverDelegate
void VivaldiSettingsApiNotification::SetPref(const char* name,
                                             const int value) {
  profile_->GetPrefs()->SetInteger(name, value);
}
void VivaldiSettingsApiNotification::SetPref(const char* name,
                                             const std::string& value) {
  profile_->GetPrefs()->SetString(name, value);
}
void VivaldiSettingsApiNotification::SetPref(const char* name,
                                             const bool value) {
  profile_->GetPrefs()->SetBoolean(name, value);
}

VivaldiSettingsApiNotification*
VivaldiSettingsApiNotificationFactory::GetForProfile(Profile* profile) {
  return static_cast<VivaldiSettingsApiNotification*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

VivaldiSettingsApiNotificationFactory*
VivaldiSettingsApiNotificationFactory::GetInstance() {
  return base::Singleton<VivaldiSettingsApiNotificationFactory>::get();
}

VivaldiSettingsApiNotificationFactory::VivaldiSettingsApiNotificationFactory()
    : BrowserContextKeyedServiceFactory(
          "VivaldiSettingsApiNotification",
          BrowserContextDependencyManager::GetInstance()) {
}

VivaldiSettingsApiNotificationFactory::
    ~VivaldiSettingsApiNotificationFactory() {
}

KeyedService* VivaldiSettingsApiNotificationFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new VivaldiSettingsApiNotification(static_cast<Profile*>(profile));
}

bool VivaldiSettingsApiNotificationFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

bool VivaldiSettingsApiNotificationFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

content::BrowserContext*
VivaldiSettingsApiNotificationFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Redirected in incognito.
  return extensions::ExtensionsBrowserClient::Get()->GetOriginalContext(
      context);
}

void VivaldiSettingsApiNotificationFactory::RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry)
{
  registry->RegisterBooleanPref(vivaldiprefs::kMousegesturesEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterBooleanPref(vivaldiprefs::kRockerGesturesEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterBooleanPref(vivaldiprefs::kVivaldiTabsToLinks, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterBooleanPref(vivaldiprefs::kVivaldiTabZoom, true,
    user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterStringPref(vivaldiprefs::kVivaldiHomepage,
                               "https://vivaldi.com",
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterInt64Pref(vivaldiprefs::kVivaldiLastTopSitesVacuumDate, 0);

  registry->RegisterBooleanPref(vivaldiprefs::kDeferredTabLoadingAfterRestore,
                                true);
  registry->RegisterBooleanPref(vivaldiprefs::kAlwaysLoadPinnedTabAfterRestore,
                                true);

  registry->RegisterBooleanPref(vivaldiprefs::kAutoUpdateEnabled, true);
  registry->RegisterIntegerPref(
      vivaldiprefs::kVivaldiNumberOfDaysToKeepVisits, 90,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterBooleanPref(vivaldiprefs::kPluginsWidevideEnabled, true);

  registry->RegisterListPref(vivaldiprefs::kVivaldiExperiments);

  ::vivaldi::RegisterPlatformPrefs(registry);
}


SettingsTogglePreferenceFunction::~SettingsTogglePreferenceFunction() {
}

SettingsTogglePreferenceFunction::SettingsTogglePreferenceFunction() {
}

bool SettingsTogglePreferenceFunction::RunAsync() {
  std::unique_ptr<vivaldi::settings::TogglePreference::Params> params(
    vivaldi::settings::TogglePreference::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Profile* profile = GetProfile();
  std::string pref = params->value;
  bool currentValue = profile->GetPrefs()->GetBoolean(pref.c_str() );

  profile->GetPrefs()->SetBoolean(pref.c_str(), !currentValue);
  results_ =
      vivaldi::settings::TogglePreference::Results::Create(!currentValue);
  SendResponse(true);
  return true;
}

SettingsGetPreferenceFunction::~SettingsGetPreferenceFunction() {
}

SettingsGetPreferenceFunction::SettingsGetPreferenceFunction() {
}

bool SettingsGetPreferenceFunction::RunAsync() {
  std::unique_ptr<vivaldi::settings::GetPreference::Params> params(
    vivaldi::settings::GetPreference::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  Profile* profile = GetProfile();
  PrefType type = FindTypeForPrefsName(params->pref_name);
  if (type == unknownPreftype) {
    error_ = "Preferences name not found: " + params->pref_name;
    return false;
  }
  std::unique_ptr<vivaldi::settings::PreferenceItem> item(
      GetPref(profile, params->pref_name, type));
  results_ = vivaldi::settings::GetPreference::Results::Create(*item.release());
  SendResponse(true);
  return true;
}


SettingsSetPreferenceFunction::SettingsSetPreferenceFunction() {}

SettingsSetPreferenceFunction::~SettingsSetPreferenceFunction() {}

bool SettingsSetPreferenceFunction::RunAsync() {
  std::unique_ptr<vivaldi::settings::SetPreference::Params> params(
      vivaldi::settings::SetPreference::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool setResult = SetPref(&(params->preference));
  SendResponse(setResult);
  return setResult;
}

bool SettingsSetPreferenceFunction::SetPref(
    const vivaldi::settings::PreferenceItem* item) {
  Profile* profile = GetProfile();
  PrefService* prefs = profile->GetPrefs();
  bool status = false;
  switch (item->preference_type) {
    case vivaldi::settings::PREFERENCE_TYPE_ENUM_BOOLEAN: {
      if (item->preference_value.boolean) {
        prefs->SetBoolean(item->preference_key.c_str(),
                          *item->preference_value.boolean);
        status = true;
      } else {
        status = false;
      }
    } break;
    case vivaldi::settings::PREFERENCE_TYPE_ENUM_STRING: {
      if (item->preference_value.string) {
        prefs->SetString(item->preference_key.c_str(),
                         *item->preference_value.string);
        status = true;
      } else {
        status = false;
      }
    } break;
    case vivaldi::settings::PREFERENCE_TYPE_ENUM_NUMBER: {
      if (item->preference_value.number) {
        prefs->SetDouble(item->preference_key.c_str(),
                         *item->preference_value.number);
        status = true;
      } else {
        status = false;
      }
    } break;
    case vivaldi::settings::PREFERENCE_TYPE_ENUM_INTEGER: {
      if (item->preference_value.integer) {
        prefs->SetInteger(item->preference_key.c_str(),
                         *item->preference_value.integer);
        status = true;
      } else {
        status = false;
      }
    } break;
    case vivaldi::settings::PREFERENCE_TYPE_ENUM_ARRAY: {
      if (item->preference_value.array) {
        base::ListValue new_url_pref_list;
        new_url_pref_list.AppendStrings(*item->preference_value.array);
        prefs->Set(prefs::kURLsToRestoreOnStartup, new_url_pref_list);
        status = true;
      } else {
        status = false;
      }
    } break;
    case vivaldi::settings::PREFERENCE_TYPE_ENUM_NONE: {
      status = false;
    } break;
    default: {
      NOTREACHED();
      break;
    }
  }

  return status;
}

SettingsGetAllPreferencesFunction::~SettingsGetAllPreferencesFunction() {
}

SettingsGetAllPreferencesFunction::SettingsGetAllPreferencesFunction() {
}

bool SettingsGetAllPreferencesFunction::RunAsync() {
  std::vector<vivaldi::settings::PreferenceItem> nodes;
  Profile* profile = GetProfile();

  for (size_t i = 0; i < arraysize(kPrefsValues); i++) {
    std::unique_ptr<vivaldi::settings::PreferenceItem> item(GetPref(
        profile, kPrefsValues[i].prefs_name, kPrefsValues[i].prefs_type));
    nodes.push_back(std::move(*item));
  }
  results_ = vivaldi::settings::GetAllPreferences::Results::Create(nodes);
  SendResponse(true);
  return true;
}


}  // namespace extensions
