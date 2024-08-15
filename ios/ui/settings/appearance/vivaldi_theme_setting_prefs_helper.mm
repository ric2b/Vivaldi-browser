// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/settings/appearance/vivaldi_theme_setting_prefs_helper.h"

#import "ios/ui/settings/appearance/vivaldi_theme_setting_prefs.h"

@implementation VivaldiThemeSettingPrefsHelper

/// Returns the startup wallpaper
+ (NSString*)getWallpaperName {
  return [VivaldiThemeSettingPrefs getWallpaperName];
}

/// Returns the apperance for settings
+ (NSString*)getAppearanceMode {
  return [VivaldiThemeSettingPrefs getAppearanceMode];
}

/// Sets the wallpaper name for starup wallpaper
+ (void)setWallpaperName:(NSString*)name {
  [VivaldiThemeSettingPrefs setWallpaperName: name];
}

/// Sets the appearance mode for theme
+ (void)setAppearanceMode:(NSString*)mode {
  [VivaldiThemeSettingPrefs setAppearanceMode: mode];
}

@end
