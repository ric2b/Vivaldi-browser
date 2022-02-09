// Copyright (c) 2017-2021 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/prefs/prefs_api.h"

#include "apps/switches.h"
#include "base/memory/ptr_util.h"
#include "browser/translate/vivaldi_translate_client.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile_resetter/profile_resetter.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/schema/prefs.h"
#include "extensions/tools/vivaldi_tools.h"
#include "prefs/native_settings_observer.h"
#include "ui/vivaldi_ui_utils.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

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

  pref_properties_map_ = ::vivaldi::ExtractLastRegisteredPrefsProperties();
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
  const auto& item = pref_properties_map_.find(path);
  if (item == pref_properties_map_.end())
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

  local_prefs_registrar_.Add(
      path, base::BindRepeating(&VivaldiPrefsApiNotification::OnChanged,
                                weak_ptr_factory_.GetWeakPtr()));
}

void VivaldiPrefsApiNotification::RegisterProfilePref(const std::string& path) {
  if (prefs_registrar_.IsObserved(path))
    return;

  prefs_registrar_.Add(
      path, base::BindRepeating(&VivaldiPrefsApiNotification::OnChanged,
                                weak_ptr_factory_.GetWeakPtr()));
}

void VivaldiPrefsApiNotification::OnChanged(const std::string& path) {
  vivaldi::prefs::PreferenceNotification pref_notification;
  pref_notification.path = path;

  const ::vivaldi::PrefProperties* properties;
  PrefService* prefs = GetPrefService(profile_, path, &properties);
  DCHECK(prefs);
  pref_notification.value =
      std::make_unique<base::Value>(GetPrefValueForJS(prefs, path, properties));
  pref_notification.uses_default =
      prefs->FindPreference(path)->IsDefaultValue();

  ::vivaldi::BroadcastEvent(
      vivaldi::prefs::OnChanged::kEventName,
      vivaldi::prefs::OnChanged::Create(pref_notification), profile_);
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
    absl::optional<int> enum_value =
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

    const PrefService::Preference* pref = prefs->FindPreference(path);
    if (!pref) {
      // This is a Chromium property that was not registered on a particular
      // platform.
      DCHECK(!base::StartsWith(path, "vivaldi."));
      continue;
    }

    result.path = path;
    result.default_value = std::make_unique<base::Value>(
        GetPrefDefaultValueForJS(prefs, path, properties));
    result.value = std::make_unique<base::Value>(
        GetPrefValueForJS(prefs, path, properties));
    result.uses_default = pref->IsDefaultValue();
    VivaldiPrefsApiNotification::FromBrowserContext(browser_context())
        ->RegisterPref(path, properties->is_local());
    results.push_back(std::move(result));
  }

  return RespondNow(ArgumentList(Results::Create(results)));
}

namespace {
std::unique_ptr<translate::TranslateUIDelegate> GetTranslateUIDelegate(
    int tab_id,
    content::BrowserContext* context,
    std::string& original_language,
    std::string& target_language) {
  content::WebContents* web_contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(tab_id, context, nullptr);
  if (web_contents) {
    return std::make_unique<translate::TranslateUIDelegate>(
        VivaldiTranslateClient::GetManagerFromWebContents(web_contents)
            ->GetWeakPtr(),
        original_language, target_language);
  }
  return nullptr;
}
}  // namespace

ExtensionFunction::ResponseAction
PrefsSetLanguagePairToAlwaysTranslateFunction::Run() {
  using vivaldi::prefs::SetLanguagePairToAlwaysTranslate::Params;
  namespace Results = vivaldi::prefs::SetLanguagePairToAlwaysTranslate::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::unique_ptr<translate::TranslateUIDelegate> ui_delegate(
      GetTranslateUIDelegate(params->tab_id, browser_context(),
                             params->original_language,
                             params->target_language));
  if (!ui_delegate) {
    return RespondNow(ArgumentList(Results::Create(false)));
  }

  // Setting it when it's already set is a soft failure.
  bool success = ui_delegate->ShouldAlwaysTranslate() != params->enable;

  ui_delegate->SetAlwaysTranslate(params->enable);
  if (params->enable) {
    // Remove language from blocked list
    ui_delegate->SetLanguageBlocked(false);
    // Flip the setting so we get automatic translation.
    PrefService* pref_service =
        Profile::FromBrowserContext(browser_context())->GetPrefs();
    pref_service->SetBoolean(vivaldiprefs::kTranslateEnabled, true);
  }

  return RespondNow(ArgumentList(Results::Create(success)));
}

ExtensionFunction::ResponseAction
PrefsSetLanguageToNeverTranslateFunction::Run() {
  using vivaldi::prefs::SetLanguageToNeverTranslate::Params;
  namespace Results = vivaldi::prefs::SetLanguageToNeverTranslate::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::unique_ptr<translate::TranslateUIDelegate> ui_delegate(
      GetTranslateUIDelegate(params->tab_id, browser_context(),
                             params->original_language,
                             params->target_language));

  if (!ui_delegate) {
    return RespondNow(ArgumentList(Results::Create(false)));
  }
  // Setting it when it's already set is a soft failure.
  bool success = params->block != ui_delegate->IsLanguageBlocked();

  ui_delegate->SetLanguageBlocked(params->block);

  if (params->block) {
    // Disable always translate if we're blocking the language
    ui_delegate->SetAlwaysTranslate(false);
  }

  return RespondNow(ArgumentList(Results::Create(success)));
}

ExtensionFunction::ResponseAction PrefsGetTranslateSettingsFunction::Run() {
  using vivaldi::prefs::GetTranslateSettings::Params;
  namespace Results = vivaldi::prefs::GetTranslateSettings::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::unique_ptr<translate::TranslateUIDelegate> ui_delegate(
      GetTranslateUIDelegate(params->tab_id, browser_context(),
                             params->original_language,
                             params->target_language));

  extensions::vivaldi::prefs::TranslateLanguageSettings settings;
  if (!ui_delegate) {
    return RespondNow(ArgumentList(Results::Create(settings)));
  }

  settings.is_language_pair_on_always_translate_list =
      ui_delegate->ShouldAlwaysTranslate();
  settings.is_language_in_never_translate_list =
      ui_delegate->IsLanguageBlocked();
  settings.is_site_on_never_translate_list =
      ui_delegate->IsSiteOnNeverPromptList();
  settings.should_show_always_translate_shortcut =
      ui_delegate->ShouldShowAlwaysTranslateShortcut();
  settings.should_show_never_translate_shortcut =
      ui_delegate->ShouldShowNeverTranslateShortcut();

  return RespondNow(ArgumentList(Results::Create(std::move(settings))));
}

ExtensionFunction::ResponseAction PrefsSetSiteToNeverTranslateFunction::Run() {
  using vivaldi::prefs::SetSiteToNeverTranslate::Params;
  namespace Results = vivaldi::prefs::SetSiteToNeverTranslate::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::unique_ptr<translate::TranslateUIDelegate> ui_delegate(
      GetTranslateUIDelegate(params->tab_id, browser_context(),
                             params->original_language,
                             params->target_language));

  if (!ui_delegate) {
    return RespondNow(ArgumentList(Results::Create(false)));
  }
  // Setting it when it's already set is a soft failure.
  bool success = params->block != ui_delegate->IsSiteOnNeverPromptList();

  if (!ui_delegate->CanAddSiteToNeverPromptList()) {
    success = false;
  } else {
    ui_delegate->SetNeverPromptSite(params->block);
  }
  return RespondNow(ArgumentList(Results::Create(success)));
}

ExtensionFunction::ResponseAction PrefsSetTranslationDeclinedFunction::Run() {
  using vivaldi::prefs::SetTranslationDeclined::Params;
  namespace Results = vivaldi::prefs::SetTranslationDeclined::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::unique_ptr<translate::TranslateUIDelegate> ui_delegate(
      GetTranslateUIDelegate(params->tab_id, browser_context(),
                             params->original_language,
                             params->target_language));

  if (!ui_delegate) {
    return RespondNow(ArgumentList(Results::Create(false)));
  }
  ui_delegate->TranslationDeclined(params->explicitly_closed);

  // We don't really have a fail condition...
  return RespondNow(ArgumentList(Results::Create(true)));
}

ExtensionFunction::ResponseAction PrefsResetTranslationPrefsFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  PrefService* prefs = profile->GetPrefs();
  DCHECK(prefs);

  auto translate_prefs = VivaldiTranslateClient::CreateTranslatePrefs(prefs);
  translate_prefs->ResetToDefaults();

  prefs->ClearPref(language::prefs::kSelectedLanguages);

  return RespondNow(NoArguments());
}

}  // namespace extensions
