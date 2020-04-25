// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "components/sync_sessions/session_sync_prefs.h"

#include "components/prefs/pref_service.h"
#include "prefs/vivaldi_gen_prefs.h"

namespace sync_sessions {
std::string SessionSyncPrefs::GetSessionNameOverride() const {
  return pref_service_->GetString(vivaldiprefs::kSyncSessionName);
}
}  // namespace sync_sessions
