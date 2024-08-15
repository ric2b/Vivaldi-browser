// Copyright (c) 2015 Vivaldi Technologies

#include "prefs/vivaldi_prefs_definitions.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "prefs/native_settings_helper_mac.h"
#include "prefs/vivaldi_pref_names.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace vivaldi {

base::Value VivaldiPrefsDefinitions::GetPlatformComputedDefault(
    const std::string& path) {
  if (path == vivaldiprefs::kSystemMacActionOnDoubleClick) {
    return base::Value(getActionOnDoubleClick());
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

  return base::Value();
}

}  // namespace vivaldi
