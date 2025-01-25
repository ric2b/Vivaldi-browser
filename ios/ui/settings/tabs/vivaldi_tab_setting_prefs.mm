// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/settings/tabs/vivaldi_tab_setting_prefs.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
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
  registry->RegisterStringPref(vivaldiprefs::kVivaldiNewTabURL, "");
  registry->RegisterIntegerPref(
      vivaldiprefs::kVivaldiNewTabSetting, VivaldiNTPTypeStartpage);
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

/// Returns Homepage Url
+ (NSString*)getHomepageUrlWithPrefService:(PrefService*)prefService {
  NSString *url = base::SysUTF8ToNSString(
                     prefService->GetString(vivaldiprefs::kVivaldiHomepageURL));
  return url;
}

/// Get new tab settings
+ (VivaldiNTPType)getNewTabSettingWithPrefService:(PrefService*)prefService {
  int settingIndex =
    prefService->GetInteger(vivaldiprefs::kVivaldiNewTabSetting);
  switch (settingIndex) {
    case 0:
      return VivaldiNTPTypeStartpage;
    case 1:
      return VivaldiNTPTypeHomepage;
    case 2:
      return VivaldiNTPTypeBlankpage;
    case 3:
      return VivaldiNTPTypeURL;
    default:
      return VivaldiNTPTypeStartpage;
  }
}

/// Returns Newtab Url
+ (NSString*)getNewTabUrlWithPrefService: (PrefService*)prefService {
  NSString *url = base::SysUTF8ToNSString(
                     prefService->GetString(vivaldiprefs::kVivaldiNewTabURL));
  return url;
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

/// Sets Homepage Url
+ (void)setHomepageUrlWithPrefService:(NSString*)url
                       inPrefServices:(PrefService*)prefService {
  prefService->SetString(vivaldiprefs::kVivaldiHomepageURL,
                         base::SysNSStringToUTF8(url));
}

/// Save the new tab setting
+ (void)setNewTabSettingWithPrefService:(PrefService*)prefService
                             andSetting:(VivaldiNTPType)setting
                                withURL:(NSString *)url {
  prefService->SetInteger(vivaldiprefs::kVivaldiNewTabSetting, setting);
  prefService->SetString(vivaldiprefs::kVivaldiNewTabURL,
                         base::SysNSStringToUTF8(url));
}

@end
