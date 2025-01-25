// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_SETTINGS_ADDRESSBAR_VIVALDI_ADDRESSBAR_SETTINGS_PREFS_H_
#define IOS_UI_SETTINGS_ADDRESSBAR_VIVALDI_ADDRESSBAR_SETTINGS_PREFS_H_

#import <UIKit/UIKit.h>

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs
class PrefRegistrySimple;

class PrefService;

// Stores and retrieves the prefs for the addressbar settings.
@interface VivaldiAddressBarSettingsPrefs : NSObject
/// Registers the syncable preferences.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;
/// Registers the local preferences.
+ (void)registerLocalStatePrefs:(PrefRegistrySimple*)registry;

@end

#endif  // IOS_UI_SETTINGS_ADDRESSBAR_VIVALDI_ADDRESSBAR_SETTINGS_PREFS_H_
