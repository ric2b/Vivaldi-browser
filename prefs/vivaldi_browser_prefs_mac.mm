// Copyright (c) 2015 Vivaldi Technologies

#include "prefs/vivaldi_browser_prefs.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "prefs/vivaldi_pref_names.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace vivaldi {

void RegisterOldPlatformPrefs(user_prefs::PrefRegistrySyncable* registry) {
}

void MigrateOldPlatformPrefs(PrefService* prefs) {
}

std::unique_ptr<base::Value> GetPlatformComputedDefault(
    const std::string& path) {
  NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];

  if (path == vivaldiprefs::kSystemMacActionOnDoubleClick) {
    NSString* appleActionOnDoubleClick =
        [userDefaults stringForKey:@"AppleActionOnDoubleClick"];
    std::string val = "";
    if (appleActionOnDoubleClick) {
      val = [appleActionOnDoubleClick UTF8String];
    }
    return std::make_unique<base::Value>(val);
  }

  if (path == vivaldiprefs::kSystemMacAquaColorVariant) {
    NSNumber* appleAquaColorVariant =
        [userDefaults objectForKey:@"AppleAquaColorVariant"];
    return std::make_unique<base::Value>([appleAquaColorVariant intValue]);
  }

  if (path == vivaldiprefs::kSystemMacKeyboardUiMode) {
    NSNumber* appleKeyboardUIMode =
        [userDefaults objectForKey:@"AppleKeyboardUIMode"];
    return std::make_unique<base::Value>([appleKeyboardUIMode intValue]);
  }

  if (path == vivaldiprefs::kSystemMacSwipeScrollDirection) {
    bool appleSwipeScrollDirection =
        [[userDefaults objectForKey:@"com.apple.swipescrolldirection"] boolValue];
    return std::make_unique<base::Value>(appleSwipeScrollDirection);
  }

  return std::make_unique<base::Value>();
}

std::string GetPlatformDefaultKey() {
  return "default_mac";
}

}  // namespace vivaldi
