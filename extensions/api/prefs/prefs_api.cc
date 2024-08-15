// Copyright (c) 2017-2021 Vivaldi Technologies AS. All rights reserved

#include <string>
#include <utility>
#include <vector>

#include "extensions/api/prefs/prefs_api.h"

#include "apps/switches.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile_resetter/profile_resetter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
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

#include "browser/translate/vivaldi_translate_client.h"
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

PrefService* GetPrefService(
    content::BrowserContext* browser_context,
    const std::string& pref_path,
    const ::vivaldi::VivaldiPrefsDefinitions::PrefProperties** properties) {
  *properties = VivaldiPrefsApiNotification::FromBrowserContext(browser_context)
                    ->GetPrefProperties(pref_path);
  if (!*properties)
    return nullptr;

  if ((*properties)->local_pref)
    return g_browser_process->local_state();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return profile->GetOriginalProfile()->GetPrefs();
}

base::Value TranslateEnumValue(
    const base::Value& original,
    const ::vivaldi::VivaldiPrefsDefinitions::PrefProperties* pref_properties) {
  if (!pref_properties->definition || !pref_properties->definition->enum_values)
    return original.Clone();
  if (!original.is_int())
    return base::Value();
  const std::string* name =
      pref_properties->definition->enum_values->FindName(original.GetInt());
  if (!name)
    return base::Value();
  return base::Value(*name);
}

base::Value GetPrefValueForJS(
    PrefService* prefs,
    const std::string& path,
    const ::vivaldi::VivaldiPrefsDefinitions::PrefProperties* properties) {
  return TranslateEnumValue(prefs->GetValue(path), properties);
}

base::Value GetPrefDefaultValueForJS(
    PrefService* prefs,
    const std::string& path,
    const ::vivaldi::VivaldiPrefsDefinitions::PrefProperties* properties) {
  return TranslateEnumValue(*prefs->GetDefaultPrefValue(path), properties);
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
    : profile_(profile),
      pref_properties_(::vivaldi::VivaldiPrefsDefinitions::GetInstance()
                           ->get_pref_properties()) {
  DCHECK(profile == profile->GetOriginalProfile());

  prefs_registrar_.Init(profile->GetPrefs());
  local_prefs_registrar_.Init(g_browser_process->local_state());

  // Create and cache the listener callback now that will listen for all
  // properties.
  pref_change_callback_ = base::BindRepeating(
      &VivaldiPrefsApiNotification::OnChanged, weak_ptr_factory_.GetWeakPtr());

  // NOTE(andre@vivaldi.com) : Make sure the ExtensionPrefs has been created in
  // the ExtensionPrefsFactory-map in case another extension changes a setting
  // that we are observing. This could cause a race-condition and hitting a
  // DCHECK. See VB-27642.
  ExtensionPrefs::Get(profile);

  native_settings_observer_.reset(
      ::vivaldi::NativeSettingsObserver::Create(profile));
}

const ::vivaldi::VivaldiPrefsDefinitions::PrefProperties*
VivaldiPrefsApiNotification::GetPrefProperties(const std::string& path) {
  const auto& item = pref_properties_->find(path);
  if (item == pref_properties_->end())
    return nullptr;
  return &(item->second);
}

void VivaldiPrefsApiNotification::RegisterPref(const std::string& path,
                                               bool local_pref) {
  if (local_pref) {
    if (local_prefs_registrar_.IsObserved(path))
      return;
    local_prefs_registrar_.Add(path, pref_change_callback_);
  } else {
    if (prefs_registrar_.IsObserved(path))
      return;
    prefs_registrar_.Add(path, pref_change_callback_);
  }
}

void VivaldiPrefsApiNotification::OnChanged(const std::string& path) {
  const ::vivaldi::VivaldiPrefsDefinitions::PrefProperties* properties =
      nullptr;
  PrefService* prefs = GetPrefService(profile_, path, &properties);
  DCHECK(prefs);
  vivaldi::prefs::PreferenceValue pref_value;
  pref_value.path = path;
  if (!prefs->FindPreference(path)->IsDefaultValue()) {
    pref_value.value = GetPrefValueForJS(prefs, path, properties);
  }

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

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::string& path = params->path;

  const ::vivaldi::VivaldiPrefsDefinitions::PrefProperties* properties;
  PrefService* prefs = GetPrefService(browser_context(), path, &properties);
  if (!prefs)
    return RespondNow(Error(UnknownPrefError(path)));

  base::Value value = GetPrefValueForJS(prefs, path, properties);
  return RespondNow(ArgumentList(Results::Create(value)));
}

ExtensionFunction::ResponseAction PrefsSetFunction::Run() {
  using vivaldi::prefs::Set::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::string& path = params->new_value.path;
  const auto& value = params->new_value.value;

  const ::vivaldi::VivaldiPrefsDefinitions::PrefProperties* properties;
  PrefService* prefs = GetPrefService(browser_context(), path, &properties);
  if (!prefs)
    return RespondNow(Error(UnknownPrefError(path)));

  DCHECK(!base::StartsWith(path, "vivaldi.system",
                           base::CompareCase::INSENSITIVE_ASCII));

  if (!value) {
    prefs->ClearPref(path);
    return RespondNow(NoArguments());
  }

  if (!properties->definition || !properties->definition->enum_values) {
    const base::Value& pref = prefs->GetValue(path);
    if (pref.type() == value->type()) {
      prefs->Set(path, *value);
    } else if (pref.type() == base::Value::Type::DOUBLE &&
               value->type() == base::Value::Type::INTEGER) {
      // JS doesn't have an explicit distinction between integer and double
      // and will send us an integer even when explicitly using a decimal
      // point if the number has an empty decimal part.
      prefs->Set(path, base::Value(value->GetDouble()));
    } else {
      return RespondNow(Error(
          std::string("Cannot assign a ") +
          base::Value::GetTypeName(value->type()) + " value to a " +
          base::Value::GetTypeName(pref.type()) + " preference: " + path));
    }
  } else {
    if (!value->is_string()) {
      return RespondNow(Error(std::string("Cannot assign a ") +
                              base::Value::GetTypeName(value->type()) +
                              " value to an enumerated preference: " + path));
    }
    std::optional<int> enum_value =
        properties->definition->enum_values->FindValue(value->GetString());
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

ExtensionFunction::ResponseAction PrefsGetForCacheFunction::Run() {
  // Parse arguments and assemble results manually instead of using generated
  // params and results types to avoid extra copies of big structures as we have
  // over 450 preferences.
  if (args().size() != 1)
    return RespondNow(Error("bad argument list"));
  const base::Value& params = args()[0];
  if (!params.is_list())
    return RespondNow(Error("bad params argument"));

  Profile* profile = Profile::FromBrowserContext(browser_context());
  PrefService* profilePrefs = profile->GetOriginalProfile()->GetPrefs();
  PrefService* localPrefs = g_browser_process->local_state();
  VivaldiPrefsApiNotification* api =
      VivaldiPrefsApiNotification::FromBrowserContext(profile);

  base::Value::List array;
  array.reserve(params.GetList().size() * 2);
  for (const base::Value& path_value : params.GetList()) {
    if (!path_value.is_string())
      return RespondNow(Error("params element is not a string"));
    // We must not skip as we must fill the values for all paths. So just
    // keep default_value as none even on errors.
    const std::string& path = path_value.GetString();
    base::Value value;
    base::Value default_value;
    do {
      const ::vivaldi::VivaldiPrefsDefinitions::PrefProperties* properties =
          api->GetPrefProperties(path);
      if (!properties) {
        // Barring bugs this is a platform-specific property not available on
        // the current platform.
        break;
      }
      bool local = properties->local_pref;
      PrefService* prefs = local ? localPrefs : profilePrefs;
      const PrefService::Preference* pref = prefs->FindPreference(path);
      if (!pref) {
        // This must be a Chromium property that was not registered on a
        // particular platform.
        DCHECK(!base::StartsWith(path, "vivaldi."));
        break;
      }
      default_value = GetPrefDefaultValueForJS(prefs, path, properties);
      if (pref->IsDefaultValue()) {
        value = std::move(default_value);
        default_value = base::Value();
      } else {
        value = GetPrefValueForJS(prefs, path, properties);
      }
      api->RegisterPref(path, local);
    } while (false);
    array.Append(std::move(value));
    array.Append(std::move(default_value));
  }

  return RespondNow(WithArguments(base::Value(std::move(array))));
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

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

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

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

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

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

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

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

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

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

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

PrefsResetAllToDefaultFunction::PrefsResetAllToDefaultFunction() {}
PrefsResetAllToDefaultFunction::~PrefsResetAllToDefaultFunction() {}

void PrefsResetAllToDefaultFunction::HandlePrefValue(const std::string& key,
                                                     const base::Value& value) {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  PrefService* prefs = profile->GetPrefs();
  const PrefService::Preference* pref = prefs->FindPreference(key);
  if (!pref->IsDefaultValue()) {
    // Some prefs will crash the browser unless we restart, so filter them out:
    if (key != prefs::kProfileAvatarIndex) {
      keys_to_reset_.push_back(key);
    }
  }
}

ExtensionFunction::ResponseAction PrefsResetAllToDefaultFunction::Run() {
  using vivaldi::prefs::ResetAllToDefault::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  Profile* profile = Profile::FromBrowserContext(browser_context());
  PrefService* prefs = profile->GetPrefs();
  DCHECK(prefs);

  // No other way (I know of) to get all prefs in a flat structure.
  prefs->IteratePreferenceValues(base::BindRepeating(
      &PrefsResetAllToDefaultFunction::HandlePrefValue, this));

  // Don't mutate while we're iterating.
  for (std::string& reset_key : keys_to_reset_) {
    if (params->paths &&
        std::find(std::begin(*params->paths), std::end(*params->paths),
                  reset_key) != std::end(*params->paths)) {
      // Skip it if it's in the blacklist.
      continue;
    }
    prefs->ClearPref(reset_key);
  }
  return RespondNow(NoArguments());
}

}  // namespace extensions
