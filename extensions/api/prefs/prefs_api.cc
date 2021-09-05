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
#include "extensions/tools/vivaldi_tools.h"
#include "prefs/native_settings_observer.h"

namespace extensions {

namespace {

// There might be security implications to letting the UI query any and all
// prefs so, we want to only allow accessing prefs that we know the UI neeeds
// and report this error otherwise.
std::string UnknownPrefError(const std::string& pref_path) {
  return "The pref api is not allowed to access " + pref_path;
}

PrefService* GetPrefService(content::BrowserContext* browser_context,
                            const std::string& pref_path,
                            const ::vivaldi::PrefProperties** properties) {
  *properties = VivaldiPrefsApiNotification::FromBrowserContext(browser_context)
                    ->GetPrefProperties(pref_path);
  if (!*properties)
    return nullptr;

  if ((*properties)->is_local())
    return g_browser_process->local_state();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return profile->GetOriginalProfile()->GetPrefs();
}

base::Value TranslateEnumValue(
    const base::Value* original,
    const ::vivaldi::PrefProperties* pref_properties) {
  if (!pref_properties->enum_properties())
    return original->Clone();
  if (!original->is_int())
    return base::Value();
  const std::string* name =
      pref_properties->enum_properties()->FindName(original->GetInt());
  if (!name)
    return base::Value();
  return base::Value(*name);
}

base::Value GetPrefValueForJS(PrefService* prefs,
                              const std::string& path,
                              const ::vivaldi::PrefProperties* properties) {
  return TranslateEnumValue(prefs->Get(path), properties);
}

base::Value GetPrefDefaultValueForJS(
    PrefService* prefs,
    const std::string& path,
    const ::vivaldi::PrefProperties* properties) {
  return TranslateEnumValue(prefs->GetDefaultPrefValue(path), properties);
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

  prefs_properties_ = ::vivaldi::ExtractLastRegisteredPrefsPropertes();
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

const ::vivaldi::PrefProperties*
VivaldiPrefsApiNotification::GetPrefProperties(const std::string& path) {
  const auto& item = prefs_properties_.find(path);
  if (item == prefs_properties_.end())
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

  const ::vivaldi::PrefProperties* properties;
  PrefService* prefs = GetPrefService(profile_, path, &properties);
  DCHECK(prefs);
  pref_value.value =
      std::make_unique<base::Value>(GetPrefValueForJS(prefs, path, properties));

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

ExtensionFunction::ResponseAction PrefsGetFunction::Run() {
  using vivaldi::prefs::Get::Params;
  namespace Results = vivaldi::prefs::Get::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const std::string& path = params->path;

  const ::vivaldi::PrefProperties* properties;
  PrefService* prefs = GetPrefService(browser_context(), path, &properties);
  if (!prefs)
    return RespondNow(Error(UnknownPrefError(path)));

  base::Value value = GetPrefValueForJS(prefs, path, properties);
  return RespondNow(ArgumentList(Results::Create(value)));
}

ExtensionFunction::ResponseAction PrefsSetFunction::Run() {
  using vivaldi::prefs::Set::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const std::string& path = params->new_value.path;
  const auto& value = params->new_value.value;

  if (!value)
    return RespondNow(Error("Expected new value"));

  const ::vivaldi::PrefProperties* properties;
  PrefService* prefs = GetPrefService(browser_context(), path, &properties);
  if (!prefs)
    return RespondNow(Error(UnknownPrefError(path)));

  DCHECK(!base::StartsWith(path, "vivaldi.system",
                           base::CompareCase::INSENSITIVE_ASCII));

  if (!properties->enum_properties()) {
    const base::Value* pref = prefs->Get(path);
    if (pref->type() == value->type()) {
      prefs->Set(path, *value);
    } else if (pref->type() == base::Value::Type::DOUBLE &&
               value->type() == base::Value::Type::INTEGER) {
      // JS doesn't have an explicit distinction between integer and double
      // and will send us an integer even when explicitly using a decimal
      // point if the number has an empty decimal part.
      prefs->Set(path, base::Value(value->GetDouble()));
    } else {
      return RespondNow(Error(
          std::string("Cannot assign a ") +
          base::Value::GetTypeName(value->type()) + " value to a " +
          base::Value::GetTypeName(pref->type()) + " preference: " + path));
    }
  } else {
    if (!value->is_string()) {
      return RespondNow(Error(std::string("Cannot assign a ") +
                              base::Value::GetTypeName(value->type()) +
                              " value to an enumerated preference: " + path));
    }
    base::Optional<int> enum_value =
        properties->enum_properties()->FindValue(value->GetString());
    if (!enum_value) {
      return RespondNow(
          Error(std::string("The value") + value->GetString() +
                "is not part of the accepted values for the enumerated "
                "preference:" +
                path));
    }
    prefs->Set(path, base::Value(*enum_value));
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction PrefsResetFunction::Run() {
  using vivaldi::prefs::Reset::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const std::string& path = params->path;

  const ::vivaldi::PrefProperties* properties;
  PrefService* prefs = GetPrefService(browser_context(), path, &properties);
  if (!prefs)
    return RespondNow(Error(UnknownPrefError(path)));

  prefs->ClearPref(path);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction PrefsGetForCacheFunction::Run() {
  using vivaldi::prefs::GetForCache::Params;
  namespace Results = vivaldi::prefs::GetForCache::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::vector<vivaldi::prefs::PreferenceValueAndDefault> results;

  for (const auto& path : params->paths) {
    vivaldi::prefs::PreferenceValueAndDefault result;

    const ::vivaldi::PrefProperties* properties;
    PrefService* prefs = GetPrefService(browser_context(), path, &properties);
    if (!prefs)
      continue;

    if (!prefs->FindPreference(path)) {
      // This is a Chromium property that was not registered on a particular
      // platform.
      DCHECK(!base::StringPiece(path).starts_with("vivaldi."));
      continue;
    }

    result.path = path;
    result.default_value = std::make_unique<base::Value>(
        GetPrefDefaultValueForJS(prefs, path, properties));
    result.value = std::make_unique<base::Value>(
        GetPrefValueForJS(prefs, path, properties));
    VivaldiPrefsApiNotification::FromBrowserContext(browser_context())
        ->RegisterPref(path, properties->is_local());
    results.push_back(std::move(result));
  }

  return RespondNow(ArgumentList(Results::Create(results)));
}

}  // namespace extensions
