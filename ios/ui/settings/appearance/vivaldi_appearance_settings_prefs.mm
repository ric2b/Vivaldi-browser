// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/settings/appearance/vivaldi_appearance_settings_prefs.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/ui/settings/appearance/vivaldi_appearance_settings_swift.h"
#import "prefs/vivaldi_pref_names.h"

@implementation VivaldiAppearanceSettingPrefs

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
  registry->RegisterStringPref(vivaldiprefs::kVivaldiAppearanceMode, "");
  registry->RegisterIntegerPref(
      vivaldiprefs::kVivaldiWebsiteAppearanceStyle,
          VivaldiWebsiteAppearanceStyleAuto);
  registry->RegisterBooleanPref(
      vivaldiprefs::kVivaldiWebsiteAppearanceForceDarkTheme, NO);
  // Set the first color from preloaded list as custom accent color, which
  // works as the selected color when dynamic accent color toggle is disabled
  // after fresh install.
  registry->RegisterStringPref(
      vivaldiprefs::kVivaldiCustomAccentColor, "#6D6D6D");
  registry->RegisterBooleanPref(
      vivaldiprefs::kVivaldiDynamicAccentColorEnabled, YES);
}

#pragma mark - Getters
+ (NSString*)getBrowserTheme {
  PrefService *prefs = [VivaldiAppearanceSettingPrefs prefService];
  NSString *theme = base::SysUTF8ToNSString(
      prefs->GetString(vivaldiprefs::kVivaldiAppearanceMode));
  return theme;
}

+ (int)getWebsiteAppearanceStyle {
  PrefService *prefService = [VivaldiAppearanceSettingPrefs prefService];
  return prefService->GetInteger(vivaldiprefs::kVivaldiWebsiteAppearanceStyle);
}

+ (BOOL)forceWebsiteDarkThemeEnabled {
  PrefService *prefService = [VivaldiAppearanceSettingPrefs prefService];
  return prefService->GetBoolean(
      vivaldiprefs::kVivaldiWebsiteAppearanceForceDarkTheme);
}

+ (NSString*)getCustomAccentColor {
  PrefService *prefs = [VivaldiAppearanceSettingPrefs prefService];
  NSString *accentColor = base::SysUTF8ToNSString(
      prefs->GetString(vivaldiprefs::kVivaldiCustomAccentColor));
  return accentColor;
}

+ (BOOL)dynamicAccentColorEnabled {
  PrefService *prefService = [VivaldiAppearanceSettingPrefs prefService];
  return prefService->GetBoolean(
      vivaldiprefs::kVivaldiDynamicAccentColorEnabled);
}

#pragma mark - Setters
+ (void)setBrowserTheme:(NSString*)theme {
  PrefService *prefService = [VivaldiAppearanceSettingPrefs prefService];
  prefService->SetString(
      vivaldiprefs::kVivaldiAppearanceMode, base::SysNSStringToUTF8(theme));
}

+ (void)setWebsiteAppearanceStyle:(int)style {
  PrefService *prefService = [VivaldiAppearanceSettingPrefs prefService];
  prefService->SetInteger(vivaldiprefs::kVivaldiWebsiteAppearanceStyle, style);
}

+ (void)setForceWebsiteDarkThemeEnabled:(BOOL)enabled {
  PrefService *prefService = [VivaldiAppearanceSettingPrefs prefService];
  prefService->SetBoolean(
      vivaldiprefs::kVivaldiWebsiteAppearanceForceDarkTheme, enabled);
}

+ (void)setCustomAccentColor:(NSString*)accentColor {
  PrefService *prefService = [VivaldiAppearanceSettingPrefs prefService];
  prefService->SetString(
      vivaldiprefs::kVivaldiCustomAccentColor,
          base::SysNSStringToUTF8(accentColor));
}

+ (void)setDynamicAccentColorEnabled:(BOOL)enabled {
  PrefService *prefService = [VivaldiAppearanceSettingPrefs prefService];
  prefService->SetBoolean(
      vivaldiprefs::kVivaldiDynamicAccentColorEnabled, enabled);
}

@end
