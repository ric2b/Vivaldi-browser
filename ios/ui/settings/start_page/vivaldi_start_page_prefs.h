// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_SETTINGS_START_PAGE_VIVALDI_START_PAGE_PREFS_H_
#define IOS_UI_SETTINGS_START_PAGE_VIVALDI_START_PAGE_PREFS_H_

#import <UIKit/UIKit.h>

#import "ios/ui/ntp/vivaldi_speed_dial_sorting_mode.h"
#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_column.h"
#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_style.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_start_item_type.h"

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs
class PrefRegistrySimple;

class PrefService;

// Stores and retrieves the prefs for the start page/new tab page.
@interface VivaldiStartPagePrefs : NSObject

/// Static variable declaration
+ (PrefService*)prefService;
+ (PrefService*)localPrefService;

/// Static method to set the PrefService
+ (void)setPrefService:(PrefService *)pref;
+ (void)setLocalPrefService:(PrefService*)pref;

/// Registers the feature preferences.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

+ (void)registerLocalStatePrefs:(PrefRegistrySimple*)registry;

#pragma mark - Getters
/// Returns the speed dial sorting mode from prefs.
+ (const SpeedDialSortingMode)getSDSortingMode;
/// Returns the speed dial sorting order from prefs.
+ (const SpeedDialSortingOrder)getSDSortingOrder;
/// Returns the start page layout setting
+ (const VivaldiStartPageLayoutStyle)getStartPageLayoutStyle;
/// Returns the start page speed dial maximum columns
+ (const VivaldiStartPageLayoutColumn)getStartPageSpeedDialMaximumColumns;
/// Returns whether frequently visited pages are visible on the start page.
+ (BOOL)showFrequentlyVisitedPages;
/// Returns whether speed dials are visible on the start page.
+ (BOOL)showSpeedDials;
/// Returns whether start page customize button is visible on the start page.
+ (BOOL)showStartPageCustomizeButton;

/// Returns the option to open start page with.
+ (const VivaldiStartPageStartItemType)getReopenStartPageWithItem;

/// Returns the last visited group index
+ (const NSInteger)getStartPageLastVisitedGroupIndex;

/// Returns the startup wallpaper
+ (NSString*)getWallpaperName;

/// Retrieves the UIImage from the stored Base64 encoded string
+ (UIImage *)getPortraitWallpaper;
+ (UIImage *)getLandscapeWallpaper;

#pragma mark - Setters
/// Sets the speed dial sorting mode to the prefs.
+ (void)setSDSortingMode:(const SpeedDialSortingMode)mode;
/// Sets the speed dial sorting order to the prefs.
+ (void)setSDSortingOrder:(const SpeedDialSortingOrder)order;
/// Sets the start page layout style.
+ (void)setStartPageLayoutStyle:(const VivaldiStartPageLayoutStyle)style;
/// Sets the start page speed dial maximum columns.
+ (void)setStartPageSpeedDialMaximumColumns:
    (VivaldiStartPageLayoutColumn)columns;
/// Sets whether frequently visited pages are visible on the start page.
+ (void)setShowFrequentlyVisitedPages:(BOOL)show;
/// Sets whether speed dials are visible on the start page.
+ (void)setShowSpeedDials:(BOOL)show;
/// Sets whether start page customize button is visible on the start page.
+ (void)setShowStartPageCustomizeButton:(BOOL)show;

/// Sets the option to open start page with.
+ (void)setReopenStartPageWithItem:(const VivaldiStartPageStartItemType)item;

/// Sets the last visited group index
+ (void)setStartPageLastVisitedGroupIndex:(const NSInteger)index;

/// Sets the wallpaper name for starup wallpaper
+ (void)setWallpaperName:(NSString*)name;

/// Stores the UIImage as a Base64 encoded string
+ (void)setPortraitWallpaper:(UIImage*)image;
+ (void)setLandscapeWallpaper:(UIImage*)image;

@end

#endif  // IOS_UI_SETTINGS_START_PAGE_VIVALDI_START_PAGE_PREFS_H_
