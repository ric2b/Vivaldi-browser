// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_SETTINGS_READER_MODE_VIVALDI_READER_MODE_PREFS_H_
#define IOS_UI_SETTINGS_READER_MODE_VIVALDI_READER_MODE_PREFS_H_

#import <UIKit/UIKit.h>

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class PrefService;

// Stores and retrieves the prefs for the reader mode settings.
@interface VivaldiReaderModePrefs : NSObject

/// Registers the feature preferences.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

/// Returns reader mode font size
+ (int)getReaderModeFontSizeWithPrefService:(PrefService*)prefService;

/// Sets reader mode font size
+ (void)setReaderModeFontSizeWithPrefService:(int)size
                              inPrefServices:(PrefService*)prefService;

/// Returns reader mode font family
+ (NSString*)getReaderModeFontFamilyWithPrefService:(PrefService*)prefService;

/// Sets reader mode font family
+ (void)setReaderModeFontFamilyWithPrefService:(NSString*)family
                                inPrefServices:(PrefService*)prefService;

/// Returns reader mode theme
+ (NSString*)getReaderModeThemeWithPrefService: (PrefService*)prefService;

/// Sets reader mode theme
+ (void)setReaderModeThemeWithPrefService:(NSString*)theme
                           inPrefServices:(PrefService*)prefService;

/// Returns reader mode enabled
+ (BOOL)getReaderModeEnabledWithPrefService:(PrefService*)prefService;

/// Sets reader mode enabled
+ (void)setReaderModeEnabledWithPrefService:(BOOL)enabled
                             inPrefServices:(PrefService*)prefService;

@end

#endif  // IOS_UI_SETTINGS_READER_MODE_VIVALDI_READER_MODE_PREFS_H_
