
// Copyright (c) 2024-25 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/settings/pagezoom/vivaldi_pagezoom_settings_prefs.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "prefs/vivaldi_pref_names.h"

@implementation VivaldiPageZoomSettingPrefs

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  // Setting default to 100% zoom
  registry->RegisterIntegerPref(vivaldiprefs::kVivaldiPageZoomLevel, 100);
}

#pragma mark - GETTERS

/// Returns page zoom level
+ (int)getPageZoomLevelWithPrefService:(PrefService*)prefService {
  return prefService->GetInteger(vivaldiprefs::kVivaldiPageZoomLevel);
}

#pragma mark - SETTERS

/// Sets page zoom level
+ (void)setPageZoomLevelWithPrefService:(int)level
                         inPrefServices:(PrefService*)prefService {
  prefService->SetInteger(vivaldiprefs::kVivaldiPageZoomLevel, level);
}

@end
