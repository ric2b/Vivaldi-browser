// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/sync_sessions/local_session_event_handler_impl.h"
#include "components/sync_sessions/synced_session_tracker.h"
#include "components/sync_sessions/tab_node_pool.h"
#include "components/sync_sessions/vivaldi_specific.h"

namespace sync_sessions {
void LocalSessionEventHandlerImpl::OnVivDataModified(
    const VivaldiSpecific& data) {
  auto specifics = std::make_unique<sync_pb::SessionSpecifics>();
  specifics->set_session_tag(current_session_tag_);
  specifics->set_tab_node_id(TabNodePool::kVivaldiTabNodeID);
  SyncedSession* current_session =
      session_tracker_->GetSession(current_session_tag_);

  SetSyncDataFromVivaldiSpecific(data, specifics->mutable_vivaldi_specific());
  current_session->SetModifiedTime(base::Time::Now());
  std::unique_ptr<WriteBatch> batch = delegate_->CreateLocalSessionWriteBatch();
  batch->Put(std::move(specifics));
  batch->Commit();
}
}  // namespace sync_sessions
