// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/sync_sessions/synced_session_tracker.h"
#include "components/sync_sessions/local_session_event_handler_impl.h"

namespace sync_sessions {
void LocalSessionEventHandlerImpl::OnVivDataModified(const std::string& data) {
  auto specifics = std::make_unique<sync_pb::SessionSpecifics>();
  specifics->set_session_tag(current_session_tag_);
  SyncedSession* current_session =
      session_tracker_->GetSession(current_session_tag_);
  current_session->SetExtData(data);
  current_session->SetModifiedTime(base::Time::Now());
  *specifics->mutable_header() = current_session->ToSessionHeaderProto();
  std::unique_ptr<WriteBatch> batch = delegate_->CreateLocalSessionWriteBatch();
  batch->Put(std::move(specifics));
  batch->Commit();
}
}  // namespace sync_sessions
