// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/settings/search_engine/vivaldi_search_engine_settings_prefs.h"

#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "prefs/vivaldi_pref_names.h"

@implementation VivaldiSearchEngineSettingsPrefs

+ (void)registerLocalStatePrefs:(PrefRegistrySimple*)registry {
  registry->RegisterBooleanPref(
      vivaldiprefs::kVivaldiEnableSearchEngineNickname, YES);
}

@end
