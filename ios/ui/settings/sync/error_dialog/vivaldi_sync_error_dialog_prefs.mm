// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/settings/sync/error_dialog/vivaldi_sync_error_dialog_prefs.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "prefs/vivaldi_pref_names.h"

@implementation VivaldiSyncErrorDialogPrefs

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  // Register pref to store last sync error dialog shown date
  registry->RegisterStringPref(
              vivaldiprefs::kVivaldiLastSyncErrorDialogShownDate, "");
  // Register pref to store flag for asking user again, default to YES
  registry->RegisterBooleanPref(
              vivaldiprefs::kVivaldiShouldAskSyncErrorAgain, YES);
}

#pragma mark - GETTERS

/// Returns the date when the sync error dialog was last shown
+ (NSDate*)getLastSyncErrorDialogShownDateWithPrefService:
             (PrefService*)prefService {
  std::string dateString =
    prefService->GetString(vivaldiprefs::kVivaldiLastSyncErrorDialogShownDate);
  if (dateString.empty()) {
    return nil;
  }
  return [NSDate dateWithTimeIntervalSince1970:std::stod(dateString)];
}

/// Returns whether we should ask the user again about the sync error
+ (BOOL)shouldAskUserAgainWithPrefService:(PrefService*)prefService {
  return prefService->GetBoolean(vivaldiprefs::kVivaldiShouldAskSyncErrorAgain);
}

#pragma mark - SETTERS

/// Sets the date when the sync error dialog was last shown
+ (void)setLastSyncErrorDialogShownDate:(NSDate*)date
                         withPrefService:(PrefService*)prefService {
  std::string dateString = std::to_string([date timeIntervalSince1970]);
  prefService->SetString(
                  vivaldiprefs::kVivaldiLastSyncErrorDialogShownDate,
                  dateString);
}

/// Sets the flag indicating if we should ask
/// the user again about the sync error
+ (void)setShouldAskUserAgain:(BOOL)shouldAskAgain
              withPrefService:(PrefService*)prefService {
  prefService->SetBoolean(
                   vivaldiprefs::kVivaldiShouldAskSyncErrorAgain,
                   shouldAskAgain);
}

@end
