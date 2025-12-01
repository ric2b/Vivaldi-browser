// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/settings/general/vivaldi_general_settings_prefs.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "prefs/vivaldi_pref_names.h"

@implementation VivaldiGeneralSettingPrefs

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterBooleanPref(vivaldiprefs::kVivaldiHomepageEnabled, NO);
  registry->RegisterStringPref(vivaldiprefs::kVivaldiHomepageURL, "");
  registry->RegisterBooleanPref(vivaldiprefs::kVivaldiBackgroundAudioEnabled,
                                NO);
}

#pragma mark - GETTERS

/// Returns showHomepage enabled status
+ (BOOL)getHomepageEnabledWithPrefService: (PrefService*)prefService {
  return prefService->GetBoolean(vivaldiprefs::kVivaldiHomepageEnabled);
}
/// Returns Homepage Url
+ (NSString*)getHomepageUrlWithPrefService:(PrefService*)prefService {
  NSString *url = base::SysUTF8ToNSString(
                    prefService->GetString(vivaldiprefs::kVivaldiHomepageURL));
  return url;
}

+ (BOOL)getBackgroundAudioEnabled:(PrefService*)prefService {
  return prefService->GetBoolean(vivaldiprefs::kVivaldiBackgroundAudioEnabled);
}

#pragma mark - SETTERS

/// Sets showHomepage enabled
+ (void)setHomepageEnabled:(BOOL)enabled
            inPrefServices:(PrefService*)prefService {
  prefService->SetBoolean(vivaldiprefs::kVivaldiHomepageURL, enabled);
}
/// Sets Homepage Url
+ (void)setHomepageUrlWithPrefService:(NSString*)url
                       inPrefServices:(PrefService*)prefService {
  prefService->SetString(vivaldiprefs::kVivaldiHomepageURL,
                         base::SysNSStringToUTF8(url));
}

+ (void)setBackgroundAudioEnabled:(BOOL)enabled
                   inPrefServices:(PrefService*)prefService {
  prefService->SetBoolean(vivaldiprefs::kVivaldiBackgroundAudioEnabled,
                          enabled);
}

@end
