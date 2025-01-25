// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/settings/addressbar/vivaldi_addressbar_settings_prefs.h"

#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "prefs/vivaldi_pref_names.h"
#import "vivaldi/prefs/vivaldi_gen_prefs.h"

@implementation VivaldiAddressBarSettingsPrefs

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  // Register the iOS specific prefs here.
  // The prefs common to all three platforms could be already registered in the
  // backend. So double check before registering it here.
}

+ (void)registerLocalStatePrefs:(PrefRegistrySimple*)registry {
  registry->RegisterBooleanPref(
       vivaldiprefs::kVivaldiShowFullAddressEnabled, NO);
}

@end
