// Copyright (c) 2015 Vivaldi Technologies

#ifndef PREFS_VIVALDI_BROWSER_PREFS_H_
#define PREFS_VIVALDI_BROWSER_PREFS_H_

#include "chrome/browser/prefs/browser_prefs.h"

class PrefRegistrySimple;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace vivaldi {

// Vivaldi: Register platform specific preferences
void RegisterPlatformPrefs(user_prefs::PrefRegistrySyncable* registry);

// Vivaldi: Register preferences to the local state
void RegisterLocalState(PrefRegistrySimple* registry);

}  // namespace vivaldi

#endif  // PREFS_VIVALDI_BROWSER_PREFS_H_
