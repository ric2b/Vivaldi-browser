// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/settings/start_page/vivaldi_start_page_prefs_helper.h"

#import "ios/ui/settings/start_page/vivaldi_start_page_prefs.h"

@implementation VivaldiStartPagePrefsHelper

#pragma mark - Getters

+ (const SpeedDialSortingMode)getSDSortingMode {
  return [VivaldiStartPagePrefs getSDSortingMode];
}

+ (const VivaldiStartPageLayoutStyle)getStartPageLayoutStyle {
  return [VivaldiStartPagePrefs getStartPageLayoutStyle];
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

+ (void)setStartPageLayoutStyle:(const VivaldiStartPageLayoutStyle)style {
  [VivaldiStartPagePrefs setStartPageLayoutStyle:style];
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
