// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_SETTINGS_API_H_
#define EXTENSIONS_API_SETTINGS_API_H_

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_ui.h"

#include "extensions/schema/settings.h"
#include "prefs/native_settings_observer.h"
#include "prefs/native_settings_observer_delegate.h"

using vivaldi::NativeSettingsObserverDelegate;

namespace extensions {

class VivaldiSettingsApiNotification;

class VivaldiSettingsApiNotificationFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static VivaldiSettingsApiNotification* GetForProfile(Profile* profile);

  static VivaldiSettingsApiNotificationFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<VivaldiSettingsApiNotificationFactory>;

  VivaldiSettingsApiNotificationFactory();
  ~VivaldiSettingsApiNotificationFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

// A class receiving the callback notification when a registered
// prefs value has changed.
class VivaldiSettingsApiNotification : public KeyedService,
  public NativeSettingsObserverDelegate {
 public:
  explicit VivaldiSettingsApiNotification(Profile*);
  VivaldiSettingsApiNotification();
  ~VivaldiSettingsApiNotification() override;

  static void BroadcastEvent(const std::string& eventname,
                             std::unique_ptr<base::ListValue>& args,
                             content::BrowserContext* context);

  void OnChanged(const std::string& prefs_changed);

  // NativeSettingsObserverDelegate
  void SetPref(const char* name, const int value) override;
  void SetPref(const char* name, const std::string& value) override;
  void SetPref(const char* name, const bool value) override;

 private:
  Profile* profile_;
  PrefChangeRegistrar prefs_registrar_;

  std::unique_ptr<::vivaldi::NativeSettingsObserver> native_settings_observer_;

  base::WeakPtrFactory<VivaldiSettingsApiNotification> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiSettingsApiNotification);
};

enum PrefType {
  booleanPreftype,
  stringPreftype,
  integerPreftype,  // integer in js
  numberPreftype,   // double in js
  arrayPreftype,
  unknownPreftype
};

class SettingsTogglePreferenceFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("settings.togglePreference",
    SETTINGS_SET_PREFERENCE)
  SettingsTogglePreferenceFunction();

 protected:
  ~SettingsTogglePreferenceFunction() override;
  bool RunAsync() override;
  DISALLOW_COPY_AND_ASSIGN(SettingsTogglePreferenceFunction);
};

class SettingsGetPreferenceFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("settings.getPreference",
    SETTINGS_GET_PREFERENCES)
  SettingsGetPreferenceFunction();

 protected:
   ~SettingsGetPreferenceFunction() override;
   bool RunAsync() override;
   DISALLOW_COPY_AND_ASSIGN(SettingsGetPreferenceFunction);
};

class SettingsSetPreferenceFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("settings.setPreference",
    SETTINGS_GET_PREFERENCES)
  SettingsSetPreferenceFunction();
  bool SetPref(const vivaldi::settings::PreferenceItem* item);

 protected:
   ~SettingsSetPreferenceFunction() override;
   bool RunAsync() override;
   DISALLOW_COPY_AND_ASSIGN(SettingsSetPreferenceFunction);
};

class SettingsGetAllPreferencesFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("settings.getAllPreferences",
    SETTINGS_GET_PREFERENCES)
  SettingsGetAllPreferencesFunction();

 protected:
   ~SettingsGetAllPreferencesFunction() override;
   bool RunAsync() override;

   DISALLOW_COPY_AND_ASSIGN(SettingsGetAllPreferencesFunction);
};

}

#endif  // EXTENSIONS_API_SETTINGS_API_H_
