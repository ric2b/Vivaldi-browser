// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notes/synced_note_tracker_entity.h"

#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/hash/hash.h"
#include "base/hash/sha1.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/notes/note_node.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/proto_memory_estimations.h"
#include "components/sync/protocol/unique_position.pb.h"

namespace sync_notes {

namespace {

void HashSpecifics(const sync_pb::EntitySpecifics& specifics,
                   std::string* hash) {
  DCHECK_GT(specifics.ByteSize(), 0);
  *hash =
      base::Base64Encode(base::SHA1HashString(specifics.SerializeAsString()));
}

}  // namespace

SyncedNoteTrackerEntity::SyncedNoteTrackerEntity(
    const vivaldi::NoteNode* note_node,
    sync_pb::EntityMetadata metadata)
    : note_node_(note_node), metadata_(std::move(metadata)) {
  if (note_node) {
    DCHECK(!metadata_.is_deleted());
  } else {
    DCHECK(metadata_.is_deleted());
  }
}

SyncedNoteTrackerEntity::~SyncedNoteTrackerEntity() = default;

bool SyncedNoteTrackerEntity::IsUnsynced() const {
  return metadata_.sequence_number() > metadata_.acked_sequence_number();
}

bool SyncedNoteTrackerEntity::MatchesData(
    const syncer::EntityData& data) const {
  if (metadata_.is_deleted() || data.is_deleted()) {
    // In case of deletion, no need to check the specifics.
    return metadata_.is_deleted() == data.is_deleted();
  }
  return MatchesSpecificsHash(data.specifics);
}

bool SyncedNoteTrackerEntity::MatchesSpecificsHash(
    const sync_pb::EntitySpecifics& specifics) const {
  DCHECK(!metadata_.is_deleted());
  DCHECK_GT(specifics.ByteSize(), 0);
  std::string hash;
  HashSpecifics(specifics, &hash);
  return hash == metadata_.specifics_hash();
}

syncer::ClientTagHash SyncedNoteTrackerEntity::GetClientTagHash() const {
  return syncer::ClientTagHash::FromHashed(metadata_.client_tag_hash());
}

size_t SyncedNoteTrackerEntity::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  size_t memory_usage = 0;
  // Include the size of the pointer to the note node.
  memory_usage += sizeof(note_node_);
  memory_usage += EstimateMemoryUsage(metadata_);
  return memory_usage;
}

}  // namespace sync_notes
