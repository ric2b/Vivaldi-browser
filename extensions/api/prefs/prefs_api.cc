// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/prefs/prefs_api.h"

#include "apps/switches.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/schema/prefs.h"
#include "prefs/native_settings_observer.h"

namespace extensions {

namespace {
std::unique_ptr<base::Value> GetPrefValue(PrefService* prefs,
                                          const std::string& path) {
  return std::make_unique<base::Value>(
      prefs->FindPreference(path)->GetValue()->Clone());
}

std::unique_ptr<base::Value> GetPrefDefaultValue(PrefService* prefs,
                                                 const std::string& path) {
  return std::make_unique<base::Value>(
      prefs->GetDefaultPrefValue(path)->Clone());
}

::vivaldi::PrefProperties* GetPrefProperties(const std::string& path,
                                             std::string* error) {
  ::vivaldi::PrefProperties* properties =
      VivaldiPrefsApiNotificationFactory::GetPrefProperties(path);
  // There might be security implications to letting the UI query any and all
  // prefs so, we want to only allow accessing prefs that we know the UI neeeds.
  if (!properties) {
    *error = "The pref api is not allowed to access " + path;
  }
  return properties;
}

PrefService* GetPrefService(content::BrowserContext* browser_context, ::vivaldi::PrefProperties* properties) {
  if (properties->local_pref)
    return g_browser_process->local_state();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return profile->GetOriginalProfile()->GetPrefs();
}

std::unique_ptr<base::Value> TranslateEnumValue(
    std::unique_ptr<base::Value> original,
    ::vivaldi::PrefProperties* pref_properties) {
  if (!pref_properties->enum_properties)
    return original;
  if (!original->is_int())
    return std::make_unique<base::Value>();
  const auto& result = pref_properties->enum_properties->value_to_string.find(
      original->GetInt());
  if (result == pref_properties->enum_properties->value_to_string.end())
    return std::make_unique<base::Value>();
  return std::make_unique<base::Value>(result->second);
}
}  // anonymous namespace

// static
VivaldiPrefsApiNotification* VivaldiPrefsApiNotification::FromBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<VivaldiPrefsApiNotification*>(
      VivaldiPrefsApiNotificationFactory::GetInstance()
          ->GetServiceForBrowserContext(browser_context, true));
}

VivaldiPrefsApiNotification::VivaldiPrefsApiNotification(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {
  DCHECK(profile == profile->GetOriginalProfile());
  prefs_registrar_.Init(profile->GetPrefs());
  local_prefs_registrar_.Init(g_browser_process->local_state());

  // NOTE(andre@vivaldi.com) : Make sure the ExtensionPrefs has been created in
  // the ExtensionPrefsFactory-map in case another extension changes a setting
  // that we are observing. This could cause a race-condition and hitting a
  // DCHECK. See VB-27642.
  ExtensionPrefs::Get(profile);

  ::vivaldi::MigrateOldPrefs(profile_->GetPrefs());

  native_settings_observer_.reset(
      ::vivaldi::NativeSettingsObserver::Create(profile));
}

// static
::vivaldi::PrefProperties*
VivaldiPrefsApiNotificationFactory::GetPrefProperties(const std::string& path) {
  const auto& item = GetInstance()->prefs_properties_.find(path);
  if (item == GetInstance()->prefs_properties_.end())
    return nullptr;
  return &(item->second);
}

void VivaldiPrefsApiNotification::RegisterPref(const std::string& path,
                                               bool local_pref) {
  if (local_pref)
    RegisterLocalPref(path);
  else
    RegisterProfilePref(path);
}

void VivaldiPrefsApiNotification::RegisterLocalPref(const std::string& path) {
  if (local_prefs_registrar_.IsObserved(path))
    return;

  local_prefs_registrar_.Add(path,
                             base::Bind(&VivaldiPrefsApiNotification::OnChanged,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void VivaldiPrefsApiNotification::RegisterProfilePref(const std::string& path) {
  if (prefs_registrar_.IsObserved(path))
    return;

  prefs_registrar_.Add(path, base::Bind(&VivaldiPrefsApiNotification::OnChanged,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void VivaldiPrefsApiNotification::OnChanged(const std::string& path) {
  vivaldi::prefs::PreferenceValue pref_value;
  pref_value.path = path;

  ::vivaldi::PrefProperties* properties =
      VivaldiPrefsApiNotificationFactory::GetPrefProperties(path);
  DCHECK(properties);
  pref_value.value = TranslateEnumValue(
      GetPrefValue(GetPrefService(profile_, properties), path),
      properties);

  ::vivaldi::BroadcastEvent(vivaldi::prefs::OnChanged::kEventName,
                            vivaldi::prefs::OnChanged::Create(pref_value),
                            profile_);
}

VivaldiPrefsApiNotification::~VivaldiPrefsApiNotification() {}

VivaldiPrefsApiNotificationFactory*
VivaldiPrefsApiNotificationFactory::GetInstance() {
  return base::Singleton<VivaldiPrefsApiNotificationFactory>::get();
}

VivaldiPrefsApiNotificationFactory::VivaldiPrefsApiNotificationFactory()
    : BrowserContextKeyedServiceFactory(
          "VivaldiPrefsApiNotification",
          BrowserContextDependencyManager::GetInstance()) {}

VivaldiPrefsApiNotificationFactory::~VivaldiPrefsApiNotificationFactory() {}

KeyedService* VivaldiPrefsApiNotificationFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new VivaldiPrefsApiNotification(static_cast<Profile*>(profile));
}

bool VivaldiPrefsApiNotificationFactory::ServiceIsNULLWhileTesting() const {
  return false;
}

bool VivaldiPrefsApiNotificationFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

content::BrowserContext*
VivaldiPrefsApiNotificationFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Redirected in incognito.
  return extensions::ExtensionsBrowserClient::Get()->GetOriginalContext(
      context);
}

void VivaldiPrefsApiNotificationFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // We need to register the old version of prefs for which the name has changed
  // in order to be able to migrate them
  ::vivaldi::RegisterOldPrefs(registry);

  prefs_properties_ = ::vivaldi::RegisterBrowserPrefs(registry);
}

ExtensionFunction::ResponseAction PrefsGetFunction::Run() {
  using vivaldi::prefs::Get::Params;
  namespace Results = vivaldi::prefs::Get::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const std::string& path = params->path;

  std::string error;
  ::vivaldi::PrefProperties* properties = GetPrefProperties(path, &error);
  if (!properties)
    return RespondNow(Error(error));

  PrefService* prefs = GetPrefService(browser_context(), properties);
  std::unique_ptr<base::Value> value = GetPrefValue(prefs, path);
  value = TranslateEnumValue(std::move(value), properties);
  return RespondNow(ArgumentList(Results::Create(*value)));
}

ExtensionFunction::ResponseAction PrefsSetFunction::Run() {
  using vivaldi::prefs::Set::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const std::string& path = params->new_value.path;
  const auto& value = params->new_value.value;

  if (!value)
    return RespondNow(Error("Expected new value"));

  std::string error;
  ::vivaldi::PrefProperties* properties = GetPrefProperties(path, &error);
  if (!properties)
    return RespondNow(Error(error));

  DCHECK(!base::StartsWith(path, "vivaldi.system",
                           base::CompareCase::INSENSITIVE_ASCII));

  PrefService* prefs = GetPrefService(browser_context(), properties);
  if (!properties->enum_properties) {
    const PrefService::Preference* pref = prefs->FindPreference(path);
    if (pref->GetType() == value->type()) {
      prefs->Set(path, *value);
    } else if (pref->GetType() == base::Value::Type::DOUBLE &&
               value->type() == base::Value::Type::INTEGER) {
      // JS doesn't have an explicit distinction between integer and double
      // and will send us an integer even when explicitly using a decimal
      // point if the number has an empty decimal part.
      prefs->Set(path, base::Value(value->GetDouble()));
    } else {
      error = std::string("Cannot assign a ") +
              base::Value::GetTypeName(value->type()) + " value to a " +
              base::Value::GetTypeName(pref->GetType()) +
              " preference: " + path;
      return RespondNow(Error(error));
    }
  } else {
    if (!value->is_string()) {
      error = std::string("Cannot assign a ") +
              base::Value::GetTypeName(value->type()) +
              " value to an enumerated preference: " + path;
      return RespondNow(Error(error));
    }
    const auto& result =
        properties->enum_properties->string_to_value.find(value->GetString());
    if (result == properties->enum_properties->string_to_value.end()) {
      error = std::string("The value") + value->GetString() +
              "is not part of the accepted values for the enumerated "
              "preference:" +
              path;
      return RespondNow(Error(error));
    }
    prefs->Set(path, base::Value(result->second));
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction PrefsResetFunction::Run() {
  using vivaldi::prefs::Reset::Params;
  namespace Results = vivaldi::prefs::Reset::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const std::string& path = params->path;

  std::string error;
  ::vivaldi::PrefProperties* properties = GetPrefProperties(path, &error);
  if (!properties)
    return RespondNow(Error(error));

  PrefService* prefs = GetPrefService(browser_context(), properties);
  prefs->ClearPref(path);

  std::unique_ptr<base::Value> value = GetPrefValue(prefs, path);
  return RespondNow(ArgumentList(Results::Create(*value)));
}

ExtensionFunction::ResponseAction PrefsGetForCacheFunction::Run() {
  using vivaldi::prefs::GetForCache::Params;
  namespace Results = vivaldi::prefs::GetForCache::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::vector<vivaldi::prefs::PreferenceValueAndDefault> results;

  for (const auto& path : params->paths) {
    vivaldi::prefs::PreferenceValueAndDefault result;
    ::vivaldi::PrefProperties* properties =
        VivaldiPrefsApiNotificationFactory::GetPrefProperties(path);
    if (!properties)
      continue;

    PrefService* prefs = GetPrefService(browser_context(), properties);
    if (!prefs->FindPreference(path)) {
      // Trying to get a pref that's not registered. This can happen
      // when listing a chromium pref that's only registered for specific
      // OS'es
      continue;
    }

    result.path = path;
    result.default_value =
        TranslateEnumValue(GetPrefDefaultValue(prefs, path), properties);
    result.value = TranslateEnumValue(GetPrefValue(prefs, path), properties);
    VivaldiPrefsApiNotification::FromBrowserContext(browser_context())
        ->RegisterPref(path, properties->local_pref);
    results.push_back(std::move(result));
  }

  return RespondNow(ArgumentList(Results::Create(results)));
}

}  // namespace extensions
