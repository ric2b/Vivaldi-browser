// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_SETTINGS_SETTINGS_API_H_
#define EXTENSIONS_API_SETTINGS_SETTINGS_API_H_

#include <memory>
#include <string>

#include "base/memory/singleton.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_ui.h"
#include "extensions/schema/settings.h"

namespace extensions {

class SettingsSetContentSettingFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("settings.setContentSetting",
                             SETTINGS_SET_CONTENTSETTING)
  SettingsSetContentSettingFunction();

 private:
  ~SettingsSetContentSettingFunction() override;
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(SettingsSetContentSettingFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_SETTINGS_SETTINGS_API_H_
