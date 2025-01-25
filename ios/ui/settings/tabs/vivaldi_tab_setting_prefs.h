// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_SETTINGS_TABS_VIVALDI_TAB_SETTING_PREFS_H_
#define IOS_UI_SETTINGS_TABS_VIVALDI_TAB_SETTING_PREFS_H_

#import <UIKit/UIKit.h>

#import "ios/ui/settings/tabs/vivaldi_ntp_type.h"

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class PrefService;

// Stores and retrieves the prefs for the tab settings.
@interface VivaldiTabSettingPrefs : NSObject

/// Registers the feature preferences.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

/// Returns the desktop style tab status
+ (BOOL)getDesktopTabsModeWithPrefService:
    (PrefService*)prefService;
/// Returns the setting for tab stack
+ (BOOL)getUseTabStackWithPrefService:
    (PrefService*)prefService;

/// Returns Homepage Url
+ (NSString*)getHomepageUrlWithPrefService: (PrefService*)prefService;

/// Get new tab settings
+ (VivaldiNTPType)getNewTabSettingWithPrefService:(PrefService*)prefService;

/// Returns Newtab Url
+ (NSString*)getNewTabUrlWithPrefService: (PrefService*)prefService;

/// Returns whether inactive tabs available. Depends on a
/// chrome flag. Due to dependencies in other place we also
/// show/hide the inactive tabs section on tabs settings based
/// on the same pref value.
+ (BOOL)isInactiveTabsAvailable;

/// Sets the desktop style tab mode.
+ (void)setDesktopTabsMode:(BOOL)enabled
            inPrefServices:(PrefService*)prefService;
/// Sets the bottom omnibox.
+ (void)setBottomOmniboxEnabled:(BOOL)enabled
                 inPrefServices:(PrefService*)prefService;
/// Sets the reverse search suggestion for bottom omnibox.
+ (void)setReverseSearchSuggestionsEnabled:(BOOL)enabled
                            inPrefServices:(PrefService*)prefService;
/// Sets the setting for tab stack
+ (void)setUseTabStack:(BOOL)enabled
        inPrefServices:(PrefService*)prefService;

/// Sets Homepage Url
+ (void)setHomepageUrlWithPrefService:(NSString*)url
        inPrefServices:(PrefService*)prefService;

/// Save the new tab settings
+ (void)setNewTabSettingWithPrefService:(PrefService*)prefService
                             andSetting:(VivaldiNTPType)setting
                                withURL:(NSString *)url;

@end

#endif  // IOS_UI_SETTINGS_TABS_VIVALDI_TAB_SETTING_PREFS_H_
