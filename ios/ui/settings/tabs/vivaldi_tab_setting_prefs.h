// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_SETTINGS_TABS_VIVALDI_TAB_SETTING_PREFS_H_
#define IOS_UI_SETTINGS_TABS_VIVALDI_TAB_SETTING_PREFS_H_

#import <UIKit/UIKit.h>

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

/// Sets the desktop style tab mode.
+ (void)setDesktopTabsMode:(BOOL)enabled
          inPrefServices:(PrefService*)prefService;
/// Sets the setting for tab stack
+ (void)setUseTabStack:(BOOL)enabled
        inPrefServices:(PrefService*)prefService;

@end

#endif  // IOS_UI_SETTINGS_TABS_VIVALDI_TAB_SETTING_PREFS_H_
