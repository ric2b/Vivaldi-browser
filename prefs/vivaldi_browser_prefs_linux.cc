// Copyright 2015 Vivaldi Technologies

#include "prefs/vivaldi_browser_prefs.h"

#include "prefs/vivaldi_pref_names.h"

namespace vivaldi {

void RegisterPlatformPrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(vivaldiprefs::kSmoothScrollingEnabled, true,
    user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

}  // namespace vivaldi
