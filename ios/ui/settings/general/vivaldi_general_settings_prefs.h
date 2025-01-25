// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_SETTINGS_GENERAL_VIVALDI_GENERAL_SETTING_PREFS_H_
#define IOS_UI_SETTINGS_GENERAL_VIVALDI_GENERAL_SETTING_PREFS_H_

#import <UIKit/UIKit.h>

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class PrefService;

// Stores and retrieves the prefs for the general settings.
@interface VivaldiGeneralSettingPrefs : NSObject

/// Registers the feature preferences.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

/// Returns Homepage Url
+ (NSString*)getHomepageUrlWithPrefService: (PrefService*)prefService;
/// Returns show homepage button status
+ (BOOL)getHomepageEnabledWithPrefService: (PrefService*)prefService;

/// Sets Homepage Url
+ (void)setHomepageUrlWithPrefService:(NSString*)url
                       inPrefServices:(PrefService*)prefService;
/// Sets showHomepage enabled
+ (void)setHomepageEnabled:(BOOL)enabled
            inPrefServices:(PrefService*)prefService;
@end

#endif  // IOS_UI_SETTINGS_GENERAL_VIVALDI_GENERAL_SETTING_PREFS_H_
