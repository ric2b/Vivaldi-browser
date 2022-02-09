// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_device_info_utils.h"

#include "components/prefs/pref_service.h"
#include "prefs/vivaldi_gen_prefs.h"

namespace vivaldi {
std::string GetSessionNameOverride(PrefService* prefs) {
  return prefs->GetString(vivaldiprefs::kSyncSessionName);
}
}  // namespace vivaldi
