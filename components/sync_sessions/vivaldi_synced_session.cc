// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/sync_sessions/synced_session.h"

namespace sync_sessions {

void SyncedSession::SetExtData(const std::string &data) {
  viv_ext_data = data;
}

std::optional<std::string> SyncedSession::GetExtData() const {
  return viv_ext_data;
}

} // namespace sync_sessions
