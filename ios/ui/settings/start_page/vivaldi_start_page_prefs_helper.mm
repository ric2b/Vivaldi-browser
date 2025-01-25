// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/settings/start_page/vivaldi_start_page_prefs_helper.h"

#import "ios/ui/settings/start_page/vivaldi_start_page_prefs.h"

@implementation VivaldiStartPagePrefsHelper

#pragma mark - Getters

+ (const SpeedDialSortingMode)getSDSortingMode {
  return [VivaldiStartPagePrefs getSDSortingMode];
}

+ (const SpeedDialSortingOrder)getSDSortingOrder {
  return [VivaldiStartPagePrefs getSDSortingOrder];
}

+ (const VivaldiStartPageLayoutStyle)getStartPageLayoutStyle {
  return [VivaldiStartPagePrefs getStartPageLayoutStyle];
}

+ (const VivaldiStartPageLayoutColumn)getStartPageSpeedDialMaximumColumns {
  return [VivaldiStartPagePrefs getStartPageSpeedDialMaximumColumns];
}

+ (BOOL)showFrequentlyVisitedPages {
  return [VivaldiStartPagePrefs showFrequentlyVisitedPages];
}

+ (BOOL)showSpeedDials {
  return [VivaldiStartPagePrefs showSpeedDials];
}

+ (BOOL)showStartPageCustomizeButton {
  return [VivaldiStartPagePrefs showStartPageCustomizeButton];
}

+ (const VivaldiStartPageStartItemType)getReopenStartPageWithItem {
  return [VivaldiStartPagePrefs getReopenStartPageWithItem];
}

+ (const NSInteger)getStartPageLastVisitedGroupIndex {
  return [VivaldiStartPagePrefs getStartPageLastVisitedGroupIndex];
}

+ (NSString*)getWallpaperName {
  return [VivaldiStartPagePrefs getWallpaperName];
}

+ (UIImage*)getPortraitWallpaper {
  return [VivaldiStartPagePrefs getPortraitWallpaper];
}

+ (UIImage*)getLandscapeWallpaper {
  return [VivaldiStartPagePrefs getLandscapeWallpaper];
}

#pragma mark - Setters

+ (void)setSDSortingMode:(const SpeedDialSortingMode)mode {
  [VivaldiStartPagePrefs setSDSortingMode:mode];
}

+ (void)setSDSortingOrder:(const SpeedDialSortingOrder)order {
  [VivaldiStartPagePrefs setSDSortingOrder:order];
}

+ (void)setStartPageLayoutStyle:(const VivaldiStartPageLayoutStyle)style {
  [VivaldiStartPagePrefs setStartPageLayoutStyle:style];
}

+ (void)setStartPageSpeedDialMaximumColumns:
    (VivaldiStartPageLayoutColumn)columns {
  [VivaldiStartPagePrefs setStartPageSpeedDialMaximumColumns:columns];
}

+ (void)setShowSpeedDials:(BOOL)show {
  [VivaldiStartPagePrefs setShowSpeedDials:show];
}

+ (void)setShowFrequentlyVisitedPages:(BOOL)show {
  [VivaldiStartPagePrefs setShowFrequentlyVisitedPages:show];
}

+ (void)setShowStartPageCustomizeButton:(BOOL)show {
  [VivaldiStartPagePrefs setShowStartPageCustomizeButton:show];
}

+ (void)setReopenStartPageWithItem:(const VivaldiStartPageStartItemType)item {
  [VivaldiStartPagePrefs setReopenStartPageWithItem:item];
}

+ (void)setStartPageLastVisitedGroupIndex:(const NSInteger)index {
  [VivaldiStartPagePrefs setStartPageLastVisitedGroupIndex:index];
}

+ (void)setWallpaperName:(NSString*)name {
  [VivaldiStartPagePrefs setWallpaperName:name];
}

+ (void)setPortraitWallpaper:(UIImage*)image {
  [VivaldiStartPagePrefs setPortraitWallpaper:image];
}

+ (void)setLandscapeWallpaper:(UIImage*)image {
  [VivaldiStartPagePrefs setLandscapeWallpaper:image];
}

@end
