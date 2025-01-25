// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/settings/tabs/vivaldi_tab_setting_prefs.h"

#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "prefs/vivaldi_pref_names.h"

@implementation VivaldiTabSettingPrefs

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterBooleanPref(vivaldiprefs::kVivaldiDesktopTabsEnabled,
                                NO);
  registry->RegisterBooleanPref(vivaldiprefs::kVivaldiTabStackEnabled,
                                NO);
  registry->RegisterBooleanPref(
      vivaldiprefs::kVivaldiReverseSearchResultsEnabled, NO);
  registry->RegisterBooleanPref(
      vivaldiprefs::kVivaldiShowXButtonBackgroundTabsEnabled, NO);
  registry->RegisterBooleanPref(
      vivaldiprefs::kVivaldiOpenNTPOnClosingLastTabEnabled, YES);
  registry->RegisterBooleanPref(
      vivaldiprefs::kVivaldiFocusOmniboxOnNTPEnabled, YES);
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

/// Returns whether inactive tabs available.
+ (BOOL)isInactiveTabsAvailable {
  return IsInactiveTabsAvailable();
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

/// Sets the bottom omnibox.
+ (void)setBottomOmniboxEnabled:(BOOL)enabled
                 inPrefServices:(PrefService*)prefService {
  prefService->SetBoolean(prefs::kBottomOmnibox, enabled);
}

/// Sets the reverse search suggestion for bottom omnibox.
+ (void)setReverseSearchSuggestionsEnabled:(BOOL)enabled
                            inPrefServices:(PrefService*)prefService {
  prefService->SetBoolean(vivaldiprefs::kVivaldiReverseSearchResultsEnabled,
                          enabled);
}

@end
