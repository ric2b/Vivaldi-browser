// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_SETTINGS_SETTINGS_API_H_
#define EXTENSIONS_API_SETTINGS_SETTINGS_API_H_

#include <memory>
#include <string>

#include "extensions/browser/extension_function.h"

#include "extensions/schema/settings.h"

namespace extensions {

class SettingsSetContentSettingFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("settings.setContentSetting",
                             SETTINGS_SET_CONTENTSETTING)
  SettingsSetContentSettingFunction();

 private:
  ~SettingsSetContentSettingFunction() override;
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_SETTINGS_SETTINGS_API_H_
