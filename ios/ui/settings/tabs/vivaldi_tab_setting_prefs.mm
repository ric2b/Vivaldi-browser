// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/settings/tabs/vivaldi_tab_setting_prefs.h"

#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "prefs/vivaldi_pref_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation VivaldiTabSettingPrefs

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterBooleanPref(vivaldiprefs::kVivaldiDesktopTabsEnabled,
                                NO);
  registry->RegisterBooleanPref(vivaldiprefs::kVivaldiTabStackEnabled,
                                NO);
}

/// Returns the desktop style tab status
+ (BOOL)getDesktopTabsModeWithPrefService:
  (PrefService*)prefService {
  return prefService->GetBoolean(vivaldiprefs::kVivaldiDesktopTabsEnabled);;
}
/// Returns the setting for tab stack
+ (BOOL)getUseTabStackWithPrefService:
  (PrefService*)prefService {
  return prefService->GetBoolean(vivaldiprefs::kVivaldiTabStackEnabled);;
}

/// Sets the desktop style tab mode.
+ (void)setDesktopTabsMode:(BOOL)enabled
            inPrefServices:(PrefService*)prefService {
  prefService->SetBoolean(vivaldiprefs::kVivaldiDesktopTabsEnabled, enabled);
}
/// Sets the setting for tab stack
+ (void)setUseTabStack:(BOOL)enabled
        inPrefServices:(PrefService*)prefService {
  prefService->SetBoolean(vivaldiprefs::kVivaldiTabStackEnabled, enabled);
}

@end
