// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/settings/search_engine/vivaldi_search_engine_settings_prefs.h"

#import "Foundation/Foundation.h"

#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/first_run/model/first_run.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "prefs/vivaldi_pref_names.h"

NSString* kShouldMigrateSearchSuggestionsPref =
    @"kShouldMigrateSearchSuggestionsPref";

@implementation VivaldiSearchEngineSettingsPrefs

+ (void)registerLocalStatePrefs:(PrefRegistrySimple*)registry {
  registry->RegisterBooleanPref(
      vivaldiprefs::kVivaldiEnableSearchEngineNickname, YES);
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

  // Check if migration is needed
  BOOL shouldMigrate = !migrationKeyExists && !FirstRun::IsChromeFirstRun();

  if (shouldMigrate && !migrationKeyExists) {
    prefs->SetBoolean(prefs::kSearchSuggestEnabled, YES);
  }

  // Set the migration done flag. This flag will prevent second time migration
  // and migration for new users.
  [[NSUserDefaults standardUserDefaults]
      setBool:NO forKey:kShouldMigrateSearchSuggestionsPref];
  [[NSUserDefaults standardUserDefaults] synchronize];
}

@end
