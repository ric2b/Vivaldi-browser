// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_SETTINGS_VIVALDI_THEME_SETTING_PREFS_H_
#define IOS_UI_SETTINGS_VIVALDI_THEME_SETTING_PREFS_H_

#import <Foundation/Foundation.h>

namespace user_prefs {
  class PrefRegistrySyncable;
}  // namespace user_prefs

class PrefService;

@interface VivaldiThemeSettingPrefs : NSObject

/// Static variable declaration
+ (PrefService*) prefService;

/// Static method to set the PrefService
+ (void)setPrefService:(PrefService *)pref;

/// Making an an entry in registry for prefs
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

/// Returns the startup wallpaper
+ (NSString*)getWallpaperName;

/// Returns the apperance for settings
+ (NSString*)getAppearanceMode;

/// Sets the wallpaper name for starup wallpaper
+ (void)setWallpaperName:(NSString*)name;

/// Sets the appearance mode for theme
+ (void)setAppearanceMode:(NSString*)mode;

@end

#endif  // IOS_UI_SETTINGS_VIVALDI_THEME_SETTING_PREFS_H_
