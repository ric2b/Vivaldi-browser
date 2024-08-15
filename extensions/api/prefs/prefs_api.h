// Copyright (c) 2017-2021 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_PREFS_PREFS_API_H_
#define EXTENSIONS_API_PREFS_PREFS_API_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/extension_function.h"
#include "prefs/vivaldi_prefs_definitions.h"

class Profile;

namespace vivaldi {
class NativeSettingsObserver;
}

namespace extensions {

class VivaldiPrefsApiNotification;

class VivaldiPrefsApiNotificationFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static VivaldiPrefsApiNotificationFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      VivaldiPrefsApiNotificationFactory>;
  friend class VivaldiPrefsApiNotification;

  VivaldiPrefsApiNotificationFactory();
  ~VivaldiPrefsApiNotificationFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

// A class receiving the callback notification when a registered
// prefs value has changed.
class VivaldiPrefsApiNotification : public KeyedService {
 public:
  static VivaldiPrefsApiNotification* FromBrowserContext(
      content::BrowserContext* browser_context);

  explicit VivaldiPrefsApiNotification(Profile* profile);
  ~VivaldiPrefsApiNotification() override;
  VivaldiPrefsApiNotification(const VivaldiPrefsApiNotification&) = delete;
  VivaldiPrefsApiNotification& operator=(const VivaldiPrefsApiNotification&) =
      delete;

  const ::vivaldi::VivaldiPrefsDefinitions::PrefProperties* GetPrefProperties(
      const std::string& path);

  void RegisterPref(const std::string& path, bool local_pref);

 private:
  void OnChanged(const std::string& path);

  const raw_ptr<Profile> profile_;
  PrefChangeRegistrar prefs_registrar_;
  PrefChangeRegistrar local_prefs_registrar_;
  const raw_ref<const ::vivaldi::VivaldiPrefsDefinitions::PrefPropertiesMap>
      pref_properties_;
  PrefChangeRegistrar::NamedChangeCallback pref_change_callback_;

  std::unique_ptr<::vivaldi::NativeSettingsObserver> native_settings_observer_;
  base::WeakPtrFactory<VivaldiPrefsApiNotification> weak_ptr_factory_{this};
};

class PrefsGetFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("prefs.get", PREFS_GET)
  PrefsGetFunction() = default;

 private:
  ~PrefsGetFunction() override = default;
  ResponseAction Run() override;
};

class PrefsSetFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("prefs.set", PREFS_SET)
  PrefsSetFunction() = default;

 private:
  ~PrefsSetFunction() override = default;
  ResponseAction Run() override;
};

class PrefsGetForCacheFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("prefs.getForCache", PREFS_GET_FOR_CACHE)
  PrefsGetForCacheFunction() = default;

 private:
  ~PrefsGetForCacheFunction() override = default;
  ResponseAction Run() override;
};

class PrefsSetLanguagePairToAlwaysTranslateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("prefs.setLanguagePairToAlwaysTranslate",
                             PREFS_SETLANGUAGEPAIRTOALWAYSTRANSLATE)
  PrefsSetLanguagePairToAlwaysTranslateFunction() = default;

 private:
  ~PrefsSetLanguagePairToAlwaysTranslateFunction() override = default;
  ResponseAction Run() override;
};

class PrefsSetLanguageToNeverTranslateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("prefs.setLanguageToNeverTranslate",
                             PREFS_SETLANGUAGETONEVERTRANSLATE)
  PrefsSetLanguageToNeverTranslateFunction() = default;

 private:
  ~PrefsSetLanguageToNeverTranslateFunction() override = default;
  ResponseAction Run() override;
};

class PrefsGetTranslateSettingsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("prefs.getTranslateSettings",
                             PREFS_GETTRANSLATESETTINGS)
  PrefsGetTranslateSettingsFunction() = default;

 private:
  ~PrefsGetTranslateSettingsFunction() override = default;
  ResponseAction Run() override;
};

class PrefsSetSiteToNeverTranslateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("prefs.setSiteToNeverTranslate",
                             PREFS_SETLANGUAGETONEVERTRANSLATE)
  PrefsSetSiteToNeverTranslateFunction() = default;

 private:
  ~PrefsSetSiteToNeverTranslateFunction() override = default;
  ResponseAction Run() override;
};

class PrefsSetTranslationDeclinedFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("prefs.setTranslationDeclined",
                             PREFS_SETTRANSLATIONDECLINED)
  PrefsSetTranslationDeclinedFunction() = default;

 private:
  ~PrefsSetTranslationDeclinedFunction() override = default;
  ResponseAction Run() override;
};

class PrefsResetTranslationPrefsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("prefs.resetTranslationPrefs",
                             PREFS_RESETTRANSLATIONPREFS)
  PrefsResetTranslationPrefsFunction() = default;

 private:
  ~PrefsResetTranslationPrefsFunction() override = default;
  ResponseAction Run() override;
};

class PrefsResetAllToDefaultFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("prefs.resetAllToDefault", PREFS_RESETALLTODEFAULT)
  PrefsResetAllToDefaultFunction();

 private:
  ~PrefsResetAllToDefaultFunction() override;

  void HandlePrefValue(const std::string& key, const base::Value& value);

  ResponseAction Run() override;

  std::vector<std::string> keys_to_reset_;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_PREFS_PREFS_API_H_
