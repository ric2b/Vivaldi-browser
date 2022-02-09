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

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace sync_pb {
class NotesModelMetadata;
class EntitySpecifics;
}  // namespace sync_pb

namespace base {
class GUID;
}  // namespace base

namespace vivaldi {
class NotesModel;
class NoteNode;
}  // namespace vivaldi

namespace syncer {
class ClientTagHash;
struct EntityData;
class UniquePosition;
}  // namespace syncer

namespace sync_notes {

// This class is responsible for keeping the mapping between note nodes in
// the local model and the server-side corresponding sync entities. It manages
// the metadata for its entities and caches entity data upon a local change
// until commit confirmation is received.
class SyncedNoteTracker {
 public:
  class Entity {
   public:
    // |note_node| can be null for tombstones. |metadata| must not be null.
    Entity(const vivaldi::NoteNode* note_node,
           std::unique_ptr<sync_pb::EntityMetadata> metadata);

    Entity(const Entity&) = delete;
    Entity& operator=(const Entity&) = delete;

    ~Entity();

    // Returns true if this data is out of sync with the server.
    // A commit may or may not be in progress at this time.
    bool IsUnsynced() const;

    // Check whether |data| matches the stored specifics hash. It also compares
    // parent information, but only if present in specifics (M94 and above).
    bool MatchesDataPossiblyIncludingParent(
        const syncer::EntityData& data) const;

    // Check whether |specifics| matches the stored specifics_hash.
    bool MatchesSpecificsHash(const sync_pb::EntitySpecifics& specifics) const;

    // Returns null for tombstones.
    const vivaldi::NoteNode* note_node() const { return note_node_; }

    // Used in local deletions to mark and entity as a tombstone.
    void clear_note_node() { note_node_ = nullptr; }

    // Used when replacing a node in order to update its otherwise immutable
    // GUID.
    void set_note_node(const vivaldi::NoteNode* note_node) {
      note_node_ = note_node;
    }

    const sync_pb::EntityMetadata* metadata() const { return metadata_.get(); }

    sync_pb::EntityMetadata* metadata() { return metadata_.get(); }

    bool commit_may_have_started() const { return commit_may_have_started_; }
    void set_commit_may_have_started(bool value) {
      commit_may_have_started_ = value;
    }

    syncer::ClientTagHash GetClientTagHash() const;

    // Returns the estimate of dynamically allocated memory in bytes.
    size_t EstimateMemoryUsage() const;

   private:
    // Null for tombstones.
    raw_ptr<const vivaldi::NoteNode> note_node_;

    // Serializable Sync metadata.
    const std::unique_ptr<sync_pb::EntityMetadata> metadata_;

    // Whether there could be a commit sent to the server for this entity. It's
    // used to protect against sending tombstones for entities that have never
    // been sent to the server. It's only briefly false between the time was
    // first added to the tracker until the first commit request is sent to the
    // server. The tracker sets it to true in the constructor because this code
    // path is only executed in production when loading from disk.
    bool commit_may_have_started_ = false;
  };

  // Returns a client tag hash given a note GUID.
  static syncer::ClientTagHash GetClientTagHashFromGUID(const base::GUID& guid);

  // Creates an empty instance with no entities. Never returns null.
  static std::unique_ptr<SyncedNoteTracker> CreateEmpty(
      sync_pb::ModelTypeState model_type_state);

  // Loads a tracker from a proto (usually from disk) after enforcing the
  // consistency of the metadata against the NotesModel. Returns null if the
  // data is inconsistent with sync metadata (i.e. corrupt). |model| must not be
  // null.
  static std::unique_ptr<SyncedNoteTracker> CreateFromNotesModelAndMetadata(
      const vivaldi::NotesModel* model,
      sync_pb::NotesModelMetadata model_metadata);

  SyncedNoteTracker(const SyncedNoteTracker&) = delete;
  SyncedNoteTracker& operator=(const SyncedNoteTracker&) = delete;

  ~SyncedNoteTracker();

  // This method is used to denote that all notes are reuploaded and there
  // is no need to reupload them again after next browser startup.
  void SetNotesReuploaded();

  // Returns null if no entity is found.
  const Entity* GetEntityForSyncId(const std::string& sync_id) const;

  // Returns null if no entity is found.
  const Entity* GetEntityForClientTagHash(
      const syncer::ClientTagHash& client_tag_hash) const;

  // Convenience function, similar to GetEntityForClientTagHash().
  const Entity* GetEntityForGUID(const base::GUID& guid) const;

  // Returns null if no entity is found.
  const SyncedNoteTracker::Entity* GetEntityForNoteNode(
      const vivaldi::NoteNode* node) const;

  // Starts tracking local note |note_node|, which must not be tracked
  // beforehand. The rest of the arguments represent the initial metadata.
  // Returns the tracked entity.
  const Entity* Add(const vivaldi::NoteNode* note_node,
                    const std::string& sync_id,
                    int64_t server_version,
                    base::Time creation_time,
                    const sync_pb::EntitySpecifics& specifics);

  // Updates the sync metadata for a tracked entity. |entity| must be owned by
  // this tracker.
  void Update(const Entity* entity,
              int64_t server_version,
              base::Time modification_time,
              const sync_pb::EntitySpecifics& specifics);

  // Updates the server version of an existing entity. |entity| must be owned by
  // this tracker.
  void UpdateServerVersion(const Entity* entity, int64_t server_version);

  // Marks an existing entry that a commit request might have been sent to the
  // server. |entity| must be owned by this tracker.
  void MarkCommitMayHaveStarted(const Entity* entity);

  // This class maintains the order of calls to this method and the same order
  // is guaranteed when returning local changes in
  // GetEntitiesWithLocalChanges() as well as in BuildNoteModelMetadata().
  // |entity| must be owned by this tracker.
  void MarkDeleted(const Entity* entity);

  // Untracks an entity, which also invalidates the pointer. |entity| must be
  // owned by this tracker.
  void Remove(const Entity* entity);

  // Increment sequence number in the metadata for |entity|. |entity| must be
  // owned by this tracker.
  void IncrementSequenceNumber(const Entity* entity);

  sync_pb::NotesModelMetadata BuildNoteModelMetadata() const;

  // Returns true if there are any local entities to be committed.
  bool HasLocalChanges() const;

  const sync_pb::ModelTypeState& model_type_state() const {
    return model_type_state_;
  }

  void set_model_type_state(sync_pb::ModelTypeState model_type_state) {
    model_type_state_ = std::move(model_type_state);
  }

  // Treats the current time as last sync time.
  // TODO(crbug.com/1032052): Remove this code once all local sync metadata is
  // required to populate the client tag (and be considered invalid otherwise).
  void UpdateLastSyncTime() { last_sync_time_ = base::Time::Now(); }

  std::vector<const Entity*> GetAllEntities() const;

  std::vector<const Entity*> GetEntitiesWithLocalChanges(
      size_t max_entries) const;

  // Updates the tracker after receiving the commit response. |sync_id| should
  // match the already tracked sync ID for |entity|, with the exception of the
  // initial commit, where the temporary client-generated ID will be overridden
  // by the server-provided final ID. |entity| must be owned by this tracker.
  void UpdateUponCommitResponse(const Entity* entity,
                                const std::string& sync_id,
                                int64_t server_version,
                                int64_t acked_sequence_number);

  // Informs the tracker that the sync ID for |entity| has changed. It updates
  // the internal state of the tracker accordingly. |entity| must be owned by
  // this tracker.
  void UpdateSyncIdIfNeeded(const Entity* entity, const std::string& sync_id);

  // Informs the tracker that a NoteNode has been replaced. It updates
  // the internal state of the tracker accordingly.
  void UpdateNoteNodePointer(const vivaldi::NoteNode* old_node,
                             const vivaldi::NoteNode* new_node);

  // Used to start tracking an entity that overwrites a previous local tombstone
  // (e.g. user-initiated note deletion undo). |entity| must be owned by
  // this tracker.
  void UndeleteTombstoneForNoteNode(const Entity* entity,
                                    const vivaldi::NoteNode* node);

  // Set the value of |EntityMetadata.acked_sequence_number| for |entity| to be
  // equal to |EntityMetadata.sequence_number| such that it is not returned in
  // GetEntitiesWithLocalChanges(). |entity| must be owned by this tracker.
  void AckSequenceNumber(const Entity* entity);

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

  // Checks whther all nodes in |notes_model| that *should* be tracked are
  // tracked.
  void CheckAllNodesTracked(const vivaldi::NotesModel* notes_model) const;

  // This method is used to mark all entities except permanent nodes as
  // unsynced. This will cause reuploading of all notes. The reupload will be
  // initiated only when the |notes_hierarchy_fields_reuploaded| field in
  // NotesMetadata is false. This field is used to prevent reuploading after
  // each browser restart. Returns true if the reupload was initiated.
  // TODO(crbug.com/1232951): remove this code when most of notes are
  // reuploaded.
  bool ReuploadNotesOnLoadIfNeeded();

  // Returns whether note commits sent to the server (most importantly
  // creations) should populate client tags.
  bool note_client_tags_in_protocol_enabled() const;

  // Causes the tracker to remember that a remote sync update (initial or
  // incremental) was ignored because its parent was unknown (either because
  // the data was corrupt or because the update is a descendant of an
  // unsupported permanent folder).
  void RecordIgnoredServerUpdateDueToMissingParent(int64_t server_version);

  absl::optional<int64_t> GetNumIgnoredUpdatesDueToMissingParentForTest() const;
  absl::optional<int64_t>
  GetMaxVersionAmongIgnoredUpdatesDueToMissingParentForTest() const;

 private:
  explicit SyncedNoteTracker(
      sync_pb::ModelTypeState model_type_state,
      bool notes_reuploaded,
      base::Time last_sync_time,
      absl::optional<int64_t> num_ignored_updates_due_to_missing_parent,
      absl::optional<int64_t>
          max_version_among_ignored_updates_due_to_missing_parent);

  // Add entities to |this| tracker based on the content of |*model| and
  // |model_metadata|. Validates the integrity of |*model| and |model_metadata|
  // and returns an enum representing any inconsistency.
  bool InitEntitiesFromModelAndMetadata(
      const vivaldi::NotesModel* model,
      sync_pb::NotesModelMetadata model_metadata);

  // Conceptually, find a tracked entity that matches |entity| and returns a
  // non-const pointer of it. The actual implementation is a const_cast.
  // |entity| must be owned by this tracker.
  Entity* AsMutableEntity(const Entity* entity);

  // Reorders |entities| that represents local non-deletions such that parent
  // creation/update is before child creation/update. Returns the ordered list.
  std::vector<const Entity*> ReorderUnsyncedEntitiesExceptDeletions(
      const std::vector<const Entity*>& entities) const;

  // Recursive method that starting from |node| appends all corresponding
  // entities with updates in top-down order to |ordered_entities|.
  void TraverseAndAppend(
      const vivaldi::NoteNode* node,
      std::vector<const SyncedNoteTracker::Entity*>* ordered_entities) const;

  // A map of sync server ids to sync entities. This should contain entries and
  // metadata for almost everything.
  std::unordered_map<std::string, std::unique_ptr<Entity>>
      sync_id_to_entities_map_;

  // Index for efficient lookups by client tag hash.
  std::unordered_map<syncer::ClientTagHash,
                     const Entity*,
                     syncer::ClientTagHash::Hash>
      client_tag_hash_to_entities_map_;

  // A map of note nodes to sync entities. It's keyed by the note node
  // pointers which get assigned when loading the note model. This map is
  // first initialized in the constructor.
  std::unordered_map<const vivaldi::NoteNode*, Entity*>
      note_node_to_entities_map_;

  // A list of pending local note deletions. They should be sent to the
  // server in the same order as stored in the list. The same order should also
  // be maintained across browser restarts (i.e. across calls to the ctor() and
  // BuildNoteModelMetadata().
  std::vector<Entity*> ordered_local_tombstones_;

  // The model metadata (progress marker, initial sync done, etc).
  sync_pb::ModelTypeState model_type_state_;

  // This field contains the value of
  // NotesMetadata::notes_hierarchy_fields_reuploaded.
  // TODO(crbug.com/1232951): remove this code when most of notes are
  // reuploaded.
  bool notes_reuploaded_ = false;

  // The local timestamp corresponding to the last time remote updates were
  // received.
  // TODO(crbug.com/1032052): Remove this code once all local sync metadata is
  // required to populate the client tag (and be considered invalid otherwise).
  base::Time last_sync_time_;

  // See corresponding proto fields in NotesModelMetadata.
  absl::optional<int64_t> num_ignored_updates_due_to_missing_parent_;
  absl::optional<int64_t>
      max_version_among_ignored_updates_due_to_missing_parent_;
};

}  // namespace sync_notes

#endif  // SYNC_NOTES_SYNCED_NOTE_TRACKER_H_
