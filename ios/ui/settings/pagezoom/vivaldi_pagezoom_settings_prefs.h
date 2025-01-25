// Copyright (c) 2024-25 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_SETTINGS_PAGEZOOM_VIVALDI_PAGEZOOM_SETTING_PREFS_H_
#define IOS_UI_SETTINGS_PAGEZOOM_VIVALDI_PAGEZOOM_SETTING_PREFS_H_

#import <UIKit/UIKit.h>

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class PrefService;

// Stores and retrieves the prefs for the page zoom settings.
@interface VivaldiPageZoomSettingPrefs : NSObject

/// Registers the feature preferences.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

/// Returns page zoom level
+ (int)getPageZoomLevelWithPrefService: (PrefService*)prefService;

/// Returns page zoom enabled status
+ (BOOL)getGlobalPageZoomEnabledWithPrefService:(PrefService*)prefService;

/// Sets page zoom level
+ (void)setPageZoomLevelWithPrefService:(int)level
                       inPrefServices:(PrefService*)prefService;

/// Sets page zoom enabled status
+ (void)setGlobalPageZoomEnabledWithPrefService:(BOOL)enabled
                                inPrefServices:(PrefService*)prefService;
@end

#endif  // IOS_UI_SETTINGS_PAGEZOOM_VIVALDI_PAGEZOOM_SETTING_PREFS_H_
