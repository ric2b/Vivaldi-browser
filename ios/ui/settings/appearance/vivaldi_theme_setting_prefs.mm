// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/settings/appearance/vivaldi_theme_setting_prefs.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "prefs/vivaldi_pref_names.h"

@implementation VivaldiThemeSettingPrefs

static PrefService *_prefService = nil;

/// Static variable to store prefService
+ (PrefService*)prefService {
    return _prefService;
}

/// Static method to set the PrefService, don't forget to set it
+ (void)setPrefService:(PrefService *)pref {
    _prefService = pref;
}

/// Making an an entry in registry for prefs
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterStringPref(vivaldiprefs::kVivaldiStartupWallpaper, "");
  registry->RegisterStringPref(vivaldiprefs::kVivaldiAppearanceMode, "");
}

/// Returns the startup wallpaper
+ (NSString*)getWallpaperName {
  PrefService *prefs = [VivaldiThemeSettingPrefs prefService];
  NSString *name = base::SysUTF8ToNSString(
      prefs->GetString(vivaldiprefs::kVivaldiStartupWallpaper));
  return name;
}

/// Returns the apperance for settings
+ (NSString*)getAppearanceMode {
  PrefService *prefs = [VivaldiThemeSettingPrefs prefService];
  NSString *mode = base::SysUTF8ToNSString(
      prefs->GetString(vivaldiprefs::kVivaldiAppearanceMode));
  return mode;
}

/// Sets the wallpaper name for starup wallpaper
+ (void)setWallpaperName:(NSString *)name {
  PrefService *prefService = [VivaldiThemeSettingPrefs prefService];
  prefService->SetString(
      vivaldiprefs::kVivaldiStartupWallpaper, base::SysNSStringToUTF8(name));
}

/// Sets the appearance mode for theme
+ (void)setAppearanceMode:(NSString *)mode {
  PrefService *prefService = [VivaldiThemeSettingPrefs prefService];
  prefService->SetString(
      vivaldiprefs::kVivaldiAppearanceMode, base::SysNSStringToUTF8(mode));
}

@end
