// Copyright 2017 Vivaldi Technologies

#include "prefs/vivaldi_browser_prefs.h"

#include "components/prefs/pref_registry_simple.h"
#include "prefs/vivaldi_pref_names.h"

namespace vivaldi {

void RegisterLocalState(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(vivaldiprefs::kVivaldiUniqueUserId, "");
}
}  // namespace vivaldi
