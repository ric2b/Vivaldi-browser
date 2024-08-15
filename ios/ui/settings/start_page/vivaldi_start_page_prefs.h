// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_SETTINGS_START_PAGE_VIVALDI_START_PAGE_PREFS_H_
#define IOS_UI_SETTINGS_START_PAGE_VIVALDI_START_PAGE_PREFS_H_

#import <UIKit/UIKit.h>

#import "ios/ui/ntp/vivaldi_speed_dial_sorting_mode.h"
#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_style.h"

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class PrefService;

// Stores and retrieves the prefs for the start page/new tab page.
@interface VivaldiStartPagePrefs : NSObject

/// Static variable declaration
+ (PrefService*)prefService;

/// Static method to set the PrefService
+ (void)setPrefService:(PrefService *)pref;

/// Registers the feature preferences.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

#pragma mark - Getters
/// Returns the speed dial sorting mode from prefs.
+ (const SpeedDialSortingMode)getSDSortingMode;
/// Returns the start page layout setting
+ (const VivaldiStartPageLayoutStyle)getStartPageLayoutStyle;
/// Returns the startup wallpaper
+ (NSString*)getWallpaperName;

/// Retrieves the UIImage from the stored Base64 encoded string
+ (UIImage *)getPortraitWallpaper;
+ (UIImage *)getLandscapeWallpaper;

#pragma mark - Setters
/// Sets the speed dial sorting mode to the prefs.
+ (void)setSDSortingMode:(const SpeedDialSortingMode)mode;
/// Sets the start page layout style.
+ (void)setStartPageLayoutStyle:(const VivaldiStartPageLayoutStyle)style;
/// Sets the wallpaper name for starup wallpaper
+ (void)setWallpaperName:(NSString*)name;

/// Stores the UIImage as a Base64 encoded string
+ (void)setPortraitWallpaper:(UIImage*)image;
+ (void)setLandscapeWallpaper:(UIImage*)image;

@end

#endif  // IOS_UI_SETTINGS_START_PAGE_VIVALDI_START_PAGE_PREFS_H_
