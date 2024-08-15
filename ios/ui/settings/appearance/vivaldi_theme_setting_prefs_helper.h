// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_SETTINGS_VIVALDI_THEME_SETTING_PREFS_HELPER_H_
#define IOS_UI_SETTINGS_VIVALDI_THEME_SETTING_PREFS_HELPER_H_

#import <Foundation/Foundation.h>

@interface VivaldiThemeSettingPrefsHelper : NSObject

/// Returns the startup wallpaper
+ (NSString*)getWallpaperName;

/// Returns the apperance for settings
+ (NSString*)getAppearanceMode;

/// Sets the wallpaper name for starup wallpaper
+ (void)setWallpaperName:(NSString*)name;

/// Sets the appearance mode for theme
+ (void)setAppearanceMode:(NSString*)mode;

@end

#endif  // IOS_UI_SETTINGS_VIVALDI_THEME_SETTING_PREFS_HELPER_H_
