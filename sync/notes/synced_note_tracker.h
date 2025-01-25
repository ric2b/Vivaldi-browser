// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTES_SYNCED_NOTE_TRACKER_H_
#define SYNC_NOTES_SYNCED_NOTE_TRACKER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/protocol/data_type_state.pb.h"

namespace sync_pb {
class NotesModelMetadata;
class EntitySpecifics;
}  // namespace sync_pb

namespace vivaldi {
class NoteNode;
}  // namespace vivaldi

namespace file_sync {
class SyncedFileStore;
}

namespace sync_notes {

class NoteModelView;
class SyncedNoteTrackerEntity;

// This class is responsible for keeping the mapping between note nodes in
// the local model and the server-side corresponding sync entities. It manages
// the metadata for its entities and caches entity data upon a local change
// until commit confirmation is received.
class SyncedNoteTracker {
 public:
  // Returns a client tag hash given a note UUID.
  static syncer::ClientTagHash GetClientTagHashFromUuid(const base::Uuid& uuid);

  // Creates an empty instance with no entities. Never returns null.
  static std::unique_ptr<SyncedNoteTracker> CreateEmpty(
      sync_pb::DataTypeState data_type_state,
      file_sync::SyncedFileStore* synced_file_store);

  // Loads a tracker from a proto (usually from disk) after enforcing the
  // consistency of the metadata against the NotesModel. Returns null if the
  // data is inconsistent with sync metadata (i.e. corrupt). `model` must not be
  // null.
  static std::unique_ptr<SyncedNoteTracker> CreateFromNotesModelAndMetadata(
      const NoteModelView* model,
      sync_pb::NotesModelMetadata model_metadata,
      file_sync::SyncedFileStore* synced_file_store);

  SyncedNoteTracker(const SyncedNoteTracker&) = delete;
  SyncedNoteTracker& operator=(const SyncedNoteTracker&) = delete;

  ~SyncedNoteTracker();

  // This method is used to denote that all notes are reuploaded and there
  // is no need to reupload them again after next browser startup.
  void SetNotesReuploaded();

  // Returns null if no entity is found.
  const SyncedNoteTrackerEntity* GetEntityForSyncId(
      const std::string& sync_id) const;

  // Returns null if no entity is found.
  const SyncedNoteTrackerEntity* GetEntityForClientTagHash(
      const syncer::ClientTagHash& client_tag_hash) const;

  // Convenience function, similar to GetEntityForClientTagHash().
  const SyncedNoteTrackerEntity* GetEntityForUuid(const base::Uuid& uuid) const;

  // Returns null if no entity is found.
  const SyncedNoteTrackerEntity* GetEntityForNoteNode(
      const vivaldi::NoteNode* node) const;

  // Starts tracking local note `note_node`, which must not be tracked
  // beforehand. The rest of the arguments represent the initial metadata.
  // Returns the tracked entity.
  const SyncedNoteTrackerEntity* Add(const vivaldi::NoteNode* note_node,
                                     const std::string& sync_id,
                                     int64_t server_version,
                                     base::Time creation_time,
                                     const sync_pb::EntitySpecifics& specifics);

  // Updates the sync metadata for a tracked entity. `entity` must be owned by
  // this tracker.
  void Update(const SyncedNoteTrackerEntity* entity,
              int64_t server_version,
              base::Time modification_time,
              const sync_pb::EntitySpecifics& specifics);

  // Updates the server version of an existing entity. `entity` must be owned by
  // this tracker.
  void UpdateServerVersion(const SyncedNoteTrackerEntity* entity,
                           int64_t server_version);

  // Marks an existing entry that a commit request might have been sent to the
  // server. `entity` must be owned by this tracker.
  void MarkCommitMayHaveStarted(const SyncedNoteTrackerEntity* entity);

  // This class maintains the order of calls to this method and the same order
  // is guaranteed when returning local changes in
  // GetEntitiesWithLocalChanges() as well as in BuildNoteModelMetadata().
  // `entity` must be owned by this tracker.
  void MarkDeleted(const SyncedNoteTrackerEntity* entity,
                   const base::Location& location);

  // Untracks an entity, which also invalidates the pointer. `entity` must be
  // owned by this tracker. `location` is used to propagate
  // debug information about which piece of code triggered the deletion.
  void Remove(const SyncedNoteTrackerEntity* entity);

  // Increment sequence number in the metadata for `entity`. `entity` must be
  // owned by this tracker.
  void IncrementSequenceNumber(const SyncedNoteTrackerEntity* entity);

  sync_pb::NotesModelMetadata BuildNoteModelMetadata() const;

  // Returns true if there are any local entities to be committed.
  bool HasLocalChanges() const;

  const sync_pb::DataTypeState& data_type_state() const {
    return data_type_state_;
  }

  void set_data_type_state(sync_pb::DataTypeState data_type_state) {
    data_type_state_ = std::move(data_type_state);
  }

  std::vector<const SyncedNoteTrackerEntity*> GetAllEntities() const;

  std::vector<const SyncedNoteTrackerEntity*> GetEntitiesWithLocalChanges()
      const;

  // Updates the tracker after receiving the commit response. `sync_id` should
  // match the already tracked sync ID for `entity`, with the exception of the
  // initial commit, where the temporary client-generated ID will be overridden
  // by the server-provided final ID. `entity` must be owned by this tracker.
  void UpdateUponCommitResponse(const SyncedNoteTrackerEntity* entity,
                                const std::string& sync_id,
                                int64_t server_version,
                                int64_t acked_sequence_number);

  // Informs the tracker that the sync ID for `entity` has changed. It updates
  // the internal state of the tracker accordingly. `entity` must be owned by
  // this tracker.
  void UpdateSyncIdIfNeeded(const SyncedNoteTrackerEntity* entity,
                            const std::string& sync_id);

  // Used to start tracking an entity that overwrites a previous local tombstone
  // (e.g. user-initiated note deletion undo). `entity` must be owned by
  // this tracker.
  void UndeleteTombstoneForNoteNode(const SyncedNoteTrackerEntity* entity,
                                    const vivaldi::NoteNode* node);

  // Set the value of `EntityMetadata.acked_sequence_number` for `entity` to be
  // equal to `EntityMetadata.sequence_number` such that it is not returned in
  // GetEntitiesWithLocalChanges(). `entity` must be owned by this tracker.
  void AckSequenceNumber(const SyncedNoteTrackerEntity* entity);

  // Whether the tracker is empty or not.
  bool IsEmpty() const;

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;

  // Returns number of tracked notes that aren't deleted.
  size_t TrackedNotesCount() const;

  // Returns number of notes that have been deleted but the server hasn't
  // confirmed the deletion yet.
  size_t TrackedUncommittedTombstonesCount() const;

  // Returns number of tracked entities. Used only in test.
  size_t TrackedEntitiesCountForTest() const;

  // Checks whether all nodes in `note_model` that *should* be tracked as
  // per IsNodeSyncable() are tracked.
  void CheckAllNodesTracked(const NoteModelView* notes_model) const;

  // This method is used to mark all entities except permanent nodes as
  // unsynced. This will cause reuploading of all notes. The reupload will be
  // initiated only when the `notes_hierarchy_fields_reuploaded` field in
  // NotesMetadata is false. This field is used to prevent reuploading after
  // each browser restart. Returns true if the reupload was initiated.
  // TODO(crbug.com/40780588): remove this code when most of notes are
  // reuploaded.
  bool ReuploadNotesOnLoadIfNeeded();

  // Causes the tracker to remember that a remote sync update (initial or
  // incremental) was ignored because its parent was unknown (either because
  // the data was corrupt or because the update is a descendant of an
  // unsupported permanent folder).
  void RecordIgnoredServerUpdateDueToMissingParent(int64_t server_version);

  std::optional<int64_t> GetNumIgnoredUpdatesDueToMissingParentForTest() const;
  std::optional<int64_t>
  GetMaxVersionAmongIgnoredUpdatesDueToMissingParentForTest() const;

 private:
  explicit SyncedNoteTracker(
      sync_pb::DataTypeState data_type_state,
      bool notes_reuploaded,
      std::optional<int64_t> num_ignored_updates_due_to_missing_parent,
      std::optional<int64_t>
          max_version_among_ignored_updates_due_to_missing_parent,
      file_sync::SyncedFileStore* synced_file_store);

  // Add entities to `this` tracker based on the content of `*model` and
  // `model_metadata`. Validates the integrity of `*model` and `model_metadata`
  // and returns an enum representing any inconsistency.
  bool InitEntitiesFromModelAndMetadata(
      const NoteModelView* model,
      sync_pb::NotesModelMetadata model_metadata);

  // Conceptually, find a tracked entity that matches `entity` and returns a
  // non-const pointer of it. The actual implementation is a const_cast.
  // `entity` must be owned by this tracker.
  SyncedNoteTrackerEntity* AsMutableEntity(
      const SyncedNoteTrackerEntity* entity);

  // Reorders `entities` that represents local non-deletions such that parent
  // creation/update is before child creation/update. Returns the ordered list.
  std::vector<const SyncedNoteTrackerEntity*>
  ReorderUnsyncedEntitiesExceptDeletions(
      const std::vector<const SyncedNoteTrackerEntity*>& entities) const;

  // Recursive method that starting from `node` appends all corresponding
  // entities with updates in top-down order to `ordered_entities`.
  void TraverseAndAppend(
      const vivaldi::NoteNode* node,
      std::vector<const SyncedNoteTrackerEntity*>* ordered_entities) const;

  const raw_ptr<file_sync::SyncedFileStore> synced_file_store_;

  // A map of sync server ids to sync entities. This should contain entries and
  // metadata for almost everything.
  std::unordered_map<std::string, std::unique_ptr<SyncedNoteTrackerEntity>>
      sync_id_to_entities_map_;

  // Index for efficient lookups by client tag hash.
  std::unordered_map<syncer::ClientTagHash,
                     raw_ptr<const SyncedNoteTrackerEntity, CtnExperimental>,
                     syncer::ClientTagHash::Hash>
      client_tag_hash_to_entities_map_;

  // A map of note nodes to sync entities. It's keyed by the note node
  // pointers which get assigned when loading the note model. This map is
  // first initialized in the constructor.
  std::unordered_map<const vivaldi::NoteNode*, SyncedNoteTrackerEntity*>
      note_node_to_entities_map_;

  // A list of pending local note deletions. They should be sent to the
  // server in the same order as stored in the list. The same order should also
  // be maintained across browser restarts (i.e. across calls to the ctor() and
  // BuildNoteModelMetadata().
  std::vector<raw_ptr<SyncedNoteTrackerEntity, VectorExperimental>>
      ordered_local_tombstones_;

  // The model metadata (progress marker, initial sync done, etc).
  sync_pb::DataTypeState data_type_state_;

  // This field contains the value of
  // NotesMetadata::notes_hierarchy_fields_reuploaded.
  // TODO(crbug.com/1232951): remove this code when most of notes are
  // reuploaded.
  bool notes_reuploaded_ = false;

  // See corresponding proto fields in NotesModelMetadata.
  std::optional<int64_t> num_ignored_updates_due_to_missing_parent_;
  std::optional<int64_t>
      max_version_among_ignored_updates_due_to_missing_parent_;
};

}  // namespace sync_notes

#endif  // SYNC_NOTES_SYNCED_NOTE_TRACKER_H_
