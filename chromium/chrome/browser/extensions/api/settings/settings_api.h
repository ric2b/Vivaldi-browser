// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/web_ui.h"

#include "chrome/common/extensions/api/settings.h"

namespace extensions {

enum PrefType {booleanPreftype, stringPreftype, numberPreftype, arrayPreftype};

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
  linked_ptr<api::settings::PreferenceItem> GetPref(const char* prefName,
                                                    PrefType type);

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
  bool SetPref(const api::settings::PreferenceItem* item);

 protected:
   ~SettingsSetPreferenceFunction() override;
   bool RunAsync() override;
   DISALLOW_COPY_AND_ASSIGN(SettingsSetPreferenceFunction);
};

}

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_API_H_
