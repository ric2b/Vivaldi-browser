// Copyright 2025 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_SETTINGS_SYNC_ERROR_DIALOG_VIVALDI_SYNC_ERROR_DIALOG_PREFS_H_
#define IOS_UI_SETTINGS_SYNC_ERROR_DIALOG_VIVALDI_SYNC_ERROR_DIALOG_PREFS_H_

#import <UIKit/UIKit.h>

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class PrefService;

// Stores and retrieves the prefs for the sync error dialog.
@interface VivaldiSyncErrorDialogPrefs : NSObject

/// Registers the feature preferences.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

/// Returns the date when the sync error dialog was last shown
+ (NSDate*)getLastSyncErrorDialogShownDateWithPrefService:
      (PrefService*)prefService;

/// Sets the date when the sync error dialog was last shown
+ (void)setLastSyncErrorDialogShownDate:(NSDate*)date
                        withPrefService:(PrefService*)prefService;

/// Returns whether we should ask the user again about the sync error
+ (BOOL)shouldAskUserAgainWithPrefService:(PrefService*)prefService;

/// Sets the flag indicating if we should ask
/// the user again about the sync error
+ (void)setShouldAskUserAgain:(BOOL)shouldAskAgain
              withPrefService:(PrefService*)prefService;
@end

#endif  // IOS_UI_SETTINGS_SYNC_ERROR_DIALOG_VIVALDI_SYNC_ERROR_DIALOG_PREFS_H_
