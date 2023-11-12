// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTES_SYNCED_NOTE_TRACKER_ENTITY_H_
#define SYNC_NOTES_SYNCED_NOTE_TRACKER_ENTITY_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/protocol/entity_metadata.pb.h"

namespace sync_pb {
class EntitySpecifics;
}  // namespace sync_pb

namespace vivaldi {
class NoteNode;
}  // namespace vivaldi

namespace syncer {
struct EntityData;
}  // namespace syncer

namespace sync_notes {

// This class manages the metadata corresponding to an individual NoteNode
// instance. It is analogous to the more generic syncer::ProcessorEntity, which
// is not reused for notes for historic reasons.
class SyncedNoteTrackerEntity {
 public:
  // |note_node| can be null for tombstones.
  SyncedNoteTrackerEntity(const vivaldi::NoteNode* note_node,
                          sync_pb::EntityMetadata metadata);
  SyncedNoteTrackerEntity(const SyncedNoteTrackerEntity&) = delete;
  SyncedNoteTrackerEntity(SyncedNoteTrackerEntity&&) = delete;
  ~SyncedNoteTrackerEntity();

  SyncedNoteTrackerEntity& operator=(const SyncedNoteTrackerEntity&) = delete;
  SyncedNoteTrackerEntity& operator=(SyncedNoteTrackerEntity&&) = delete;

  // Returns true if this data is out of sync with the server.
  // A commit may or may not be in progress at this time.
  bool IsUnsynced() const;

  // Check whether |data| matches the stored specifics hash. It also compares
  // parent information (which is included in specifics).
  bool MatchesData(const syncer::EntityData& data) const;

  // Check whether |specifics| matches the stored specifics_hash.
  bool MatchesSpecificsHash(const sync_pb::EntitySpecifics& specifics) const;

  // Returns null for tombstones.
  const vivaldi::NoteNode* note_node() const { return note_node_; }

  // Used in local deletions to mark and entity as a tombstone.
  void clear_note_node() { note_node_ = nullptr; }

  // Used when replacing a node in order to update its otherwise immutable
  // UUID.
  void set_note_node(const vivaldi::NoteNode* note_node) {
    note_node_ = note_node;
  }

  const sync_pb::EntityMetadata& metadata() const { return metadata_; }

  sync_pb::EntityMetadata* MutableMetadata() { return &metadata_; }

  bool commit_may_have_started() const { return commit_may_have_started_; }
  void set_commit_may_have_started(bool value) {
    commit_may_have_started_ = value;
  }

  syncer::ClientTagHash GetClientTagHash() const;

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;

 private:
  // Null for tombstones.
  raw_ptr<const vivaldi::NoteNode, AcrossTasksDanglingUntriaged> note_node_;

  // Serializable Sync metadata.
  sync_pb::EntityMetadata metadata_;

  // Whether there could be a commit sent to the server for this entity. It's
  // used to protect against sending tombstones for entities that have never
  // been sent to the server. It's only briefly false between the time was
  // first added to the tracker until the first commit request is sent to the
  // server. The tracker sets it to true in the constructor because this code
  // path is only executed in production when loading from disk.
  bool commit_may_have_started_ = false;
};

}  // namespace sync_notes

#endif  // SYNC_NOTES_SYNCED_NOTE_TRACKER_ENTITY_H_
