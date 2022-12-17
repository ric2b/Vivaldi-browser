// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_VIVALDI_SPEED_DIAL_PREFS_H
#define IOS_VIVALDI_SPEED_DIAL_PREFS_H

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/ntp/vivaldi_speed_dial_sorting_mode.h"

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class PrefService;

// Stores and retrieves the prefs for the start page/new tab page.
@interface VivaldiSpeedDialPrefs : NSObject

/// Registers the feature preferences.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

/// Returns the speed dial sorting mode from prefs.
+ (const SpeedDialSortingMode)getSDSortingModeWithPrefService:
    (PrefService*)prefService;

/// Sets the speed dial sorting mode to the prefs.
+ (void)setSDSortingMode:(const SpeedDialSortingMode)mode
          inPrefServices:(PrefService*)prefService;

@end

#endif  // IOS_VIVALDI_SPEED_DIAL_PREFS_H
