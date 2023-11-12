// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_NTP_VIVALDI_START_PAGE_PREFS_H_
#define IOS_UI_NTP_VIVALDI_START_PAGE_PREFS_H_

#import <UIKit/UIKit.h>

#import "ios/ui/ntp/vivaldi_speed_dial_sorting_mode.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_layout_style.h"

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class PrefService;

// Stores and retrieves the prefs for the start page/new tab page.
@interface VivaldiStartPagePrefs : NSObject

/// Registers the feature preferences.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

/// Returns the speed dial sorting mode from prefs.
+ (const SpeedDialSortingMode)getSDSortingModeWithPrefService:
    (PrefService*)prefService;
/// Returns the start page layout setting
+ (const VivaldiStartPageLayoutStyle)getStartPageLayoutStyleWithPrefService:
    (PrefService*)prefService;

/// Sets the speed dial sorting mode to the prefs.
+ (void)setSDSortingMode:(const SpeedDialSortingMode)mode
          inPrefServices:(PrefService*)prefService;
/// Sets the start page layout style.
+ (void)setStartPageLayoutStyle:(const VivaldiStartPageLayoutStyle)style
                 inPrefServices:(PrefService*)prefService;

@end

#endif  // IOS_UI_NTP_VIVALDI_START_PAGE_PREFS_H_
