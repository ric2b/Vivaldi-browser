// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/settings/appearance/vivaldi_appearance_settings_prefs_helper.h"

#import "ios/ui/settings/appearance/vivaldi_appearance_settings_prefs.h"

namespace {
NSString* darkThemeKey = @"dark";
}

@implementation VivaldiAppearanceSettingsPrefsHelper

#pragma mark - Getters
+ (NSString*)getBrowserTheme {
  return [VivaldiAppearanceSettingPrefs getBrowserTheme];
}

+ (BOOL)isBrowserThemeDark {
  return [[self getBrowserTheme] isEqualToString:darkThemeKey];
}

+ (int)getWebsiteAppearanceStyle {
  return [VivaldiAppearanceSettingPrefs getWebsiteAppearanceStyle];
}

+ (BOOL)forceWebsiteDarkThemeEnabled {
  return [VivaldiAppearanceSettingPrefs forceWebsiteDarkThemeEnabled];
}

+ (NSString*)getCustomAccentColor {
  return [VivaldiAppearanceSettingPrefs getCustomAccentColor];
}

+ (BOOL)dynamicAccentColorEnabled {
  return [VivaldiAppearanceSettingPrefs dynamicAccentColorEnabled];
}

#pragma mark - Setters
+ (void)setBrowserTheme:(NSString*)theme {
  [VivaldiAppearanceSettingPrefs setBrowserTheme:theme];
}

+ (void)setWebsiteAppearanceStyle:(int)style {
  [VivaldiAppearanceSettingPrefs setWebsiteAppearanceStyle:style];
}

+ (void)setForceWebsiteDarkThemeEnabled:(BOOL)enabled {
  [VivaldiAppearanceSettingPrefs setForceWebsiteDarkThemeEnabled:enabled];
}

+ (void)setCustomAccentColor:(NSString*)accentColor {
  [VivaldiAppearanceSettingPrefs setCustomAccentColor:accentColor];
}

+ (void)setDynamicAccentColorEnabled:(BOOL)enabled {
  [VivaldiAppearanceSettingPrefs setDynamicAccentColorEnabled:enabled];
}

@end
