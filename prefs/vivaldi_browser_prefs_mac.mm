// Copyright (c) 2015 Vivaldi Technologies

#include "prefs/vivaldi_browser_prefs.h"

#import <Cocoa/Cocoa.h>

#include "base/values.h"
#include "base/mac/mac_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "prefs/native_settings_helper_mac.h"
#include "prefs/vivaldi_pref_names.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace vivaldi {

void RegisterOldPlatformPrefs(user_prefs::PrefRegistrySyncable* registry) {
}

void MigrateOldPlatformPrefs(PrefService* prefs) {
}

base::Value GetPlatformComputedDefault(const std::string& path) {
  if (path == vivaldiprefs::kSystemMacActionOnDoubleClick) {
    return base::Value(getActionOnDoubleClick());
  }

  if (path == vivaldiprefs::kSystemMacAquaColorVariant) {
    return base::Value(getAquaColor());
  }

  if (path == vivaldiprefs::kSystemMacKeyboardUiMode) {
    return base::Value(getKeyboardUIMode());
  }

  if (path == vivaldiprefs::kSystemMacSwipeScrollDirection) {
    return base::Value(getSwipeDirection());
  }

  if (path == vivaldiprefs::kSystemDesktopThemeColor) {
    return base::Value(getSystemDarkMode());
  }

  if (path == vivaldiprefs::kSystemAccentColor) {
    return base::Value(getSystemAccentColor());
  }

  if (path == vivaldiprefs::kSystemHighlightColor) {
    return base::Value(getSystemHighlightColor());
  }

  if (path == vivaldiprefs::kSystemMacMenubarVisibleInFullscreen) {
    return base::Value(getMenubarVisibleInFullscreen());
  }

  if (path == vivaldiprefs::kSystemMacHideMenubar) {
    return base::Value(getHideMenubar());
  }

  return base::Value();
}

}  // namespace vivaldi
