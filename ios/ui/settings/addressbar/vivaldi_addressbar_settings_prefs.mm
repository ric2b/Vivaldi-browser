// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/settings/addressbar/vivaldi_addressbar_settings_prefs.h"

#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/first_run/model/first_run.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "prefs/vivaldi_pref_names.h"
#import "vivaldi/prefs/vivaldi_gen_prefs.h"

NSString* kShouldMigrateSearchSuggestionsPref =
    @"kShouldMigrateSearchSuggestionsPref";

@implementation VivaldiAddressBarSettingsPrefs

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  // Register the iOS specific prefs here.
  // The prefs common to all three platforms could be already registered in the
  // backend. So double check before registering it here.
}

+ (void)registerLocalStatePrefs:(PrefRegistrySimple*)registry {
  registry->RegisterBooleanPref(
       vivaldiprefs::kVivaldiShowFullAddressEnabled, NO);
  registry->RegisterBooleanPref(
       vivaldiprefs::kVivaldiShowXForSuggestionEnabled, NO);
}

+ (void)migratePrefsIfNeeded:(PrefService*)prefs {

  // Search Suggestions pref should be migrated in a way that:
  // 1: New users should have it disabled by default to match our other clients.
  // This part is handled in the PrefRegistry.
  // 2: The old users should have it enabled because that was enabled by default
  // before this update. So, the migration should only happen if its not the
  // first run for the user, and already not migrated.

  // Check if migration has already been done. If UserDefaults has object for
  // this key, that means migration is alreay completed once.
  BOOL migrationKeyExists =
      [[NSUserDefaults standardUserDefaults]
          objectForKey:kShouldMigrateSearchSuggestionsPref];

  if (!migrationKeyExists && !FirstRun::IsChromeFirstRun()) {
    prefs->SetBoolean(prefs::kSearchSuggestEnabled, YES);
  }

  // Set the migration done flag. This flag will prevent second time migration
  // and migration for new users.
  [[NSUserDefaults standardUserDefaults]
      setBool:NO forKey:kShouldMigrateSearchSuggestionsPref];
  [[NSUserDefaults standardUserDefaults] synchronize];
}

@end
