// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync_sessions/synced_session.h"
#include "components/sync_sessions/vivaldi_specific.h"

namespace sync_sessions {

void SyncedSession::SetVivaldiSpecific(
    const VivaldiSpecific& vivaldi_specific) {
  vivaldi_specific_ = vivaldi_specific;
}

const VivaldiSpecific& SyncedSession::GetVivaldiSpecific() const {
  return vivaldi_specific_;
}
} // namespace sync_sessions
