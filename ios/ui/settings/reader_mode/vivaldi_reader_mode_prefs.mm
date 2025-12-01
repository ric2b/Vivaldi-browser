// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/settings/reader_mode/vivaldi_reader_mode_prefs.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "prefs/vivaldi_pref_names.h"

namespace {
// Default values
const int kDefaultTextZoom = 100;  // Default zoom of 100%
const char kDefaultFontFamily[] = "sans-serif";  // Matches Swift enum rawValue
const char kDefaultTheme[] = "light";  // Matches Swift enum rawValue
}

@implementation VivaldiReaderModePrefs

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterIntegerPref(
                      vivaldiprefs::kReaderModeFontSize, kDefaultTextZoom);
  registry->RegisterStringPref(
                      vivaldiprefs::kReaderModeFontFamily, kDefaultFontFamily);
  registry->RegisterStringPref(
                      vivaldiprefs::kReaderModeTheme, kDefaultTheme);
  // Keep vivaldi reader mode enable  by default
  registry->RegisterBooleanPref(vivaldiprefs::kVivaldiReaderModeEnabled, YES);
}

+ (int)getReaderModeFontSizeWithPrefService: (PrefService*)prefService {
  int savedSize = prefService->GetInteger(vivaldiprefs::kReaderModeFontSize);
  return savedSize;
}

+ (BOOL)getReaderModeEnabledWithPrefService:(PrefService*)prefService {
  return prefService->GetBoolean(vivaldiprefs::kVivaldiReaderModeEnabled);
}

+ (void)setReaderModeFontSizeWithPrefService:(int)size
                              inPrefServices:(PrefService*)prefService {
  prefService->SetInteger(vivaldiprefs::kReaderModeFontSize, size);
}

+ (NSString*)getReaderModeFontFamilyWithPrefService: (PrefService*)prefService {
  return base::SysUTF8ToNSString(
                  prefService->GetString(vivaldiprefs::kReaderModeFontFamily));
}

+ (void)setReaderModeFontFamilyWithPrefService:(NSString*)family
                                inPrefServices:(PrefService*)prefService {
  prefService->SetString(
                 vivaldiprefs::kReaderModeFontFamily,
                         base::SysNSStringToUTF8(family));
}

+ (NSString*)getReaderModeThemeWithPrefService: (PrefService*)prefService {
  return base::SysUTF8ToNSString(
                prefService->GetString(vivaldiprefs::kReaderModeTheme));
}

+ (void)setReaderModeThemeWithPrefService:(NSString*)theme
                           inPrefServices:(PrefService*)prefService {
  prefService->SetString(
                vivaldiprefs::kReaderModeTheme,
                        base::SysNSStringToUTF8(theme));
}

+ (void)setReaderModeEnabledWithPrefService:(BOOL)enabled
                             inPrefServices:(PrefService*)prefService {
  prefService->SetBoolean(vivaldiprefs::kVivaldiReaderModeEnabled, enabled);
}

@end
