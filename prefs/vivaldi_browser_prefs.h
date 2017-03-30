// Copyright (c) 2015 Vivaldi Technologies

#ifndef VIVALDI_BROWSER_PREFS_BROWSER_PREFS_H_
#define VIVALDI_BROWSER_PREFS_BROWSER_PREFS_H_

#include "chrome/browser/prefs/browser_prefs.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace vivaldi {

  // Vivaldi: Register platform specific preferences
  void RegisterPlatformPrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace vivaldi

#endif  // VIVALDI_BROWSER_PREFS_BROWSER_PREFS_H_
