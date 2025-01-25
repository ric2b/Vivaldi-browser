// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_SETTINGS_SEARCH_ENGINE_VIVALDI_SEARCH_ENGINE_SETTINGS_PREFS_H_
#define IOS_UI_SETTINGS_SEARCH_ENGINE_VIVALDI_SEARCH_ENGINE_SETTINGS_PREFS_H_

#import <UIKit/UIKit.h>

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs
class PrefRegistrySimple;

class PrefService;

// Stores and retrieves the prefs for the search engine settings.
@interface VivaldiSearchEngineSettingsPrefs : NSObject
/// Registers the local preferences.
+ (void)registerLocalStatePrefs:(PrefRegistrySimple*)registry;

/// Call for migrating the prefs.
+ (void)migratePrefsIfNeeded:(PrefService*)prefs;

@end

#endif  // IOS_UI_SETTINGS_SEARCH_ENGINE_VIVALDI_SEARCH_ENGINE_SETTINGS_PREFS_H_
