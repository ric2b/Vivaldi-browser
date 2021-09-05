// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTES_SYNCED_NOTE_TRACKER_H_
#define SYNC_NOTES_SYNCED_NOTE_TRACKER_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/notes_model_metadata.pb.h"
#include "components/sync/protocol/unique_position.pb.h"

namespace vivaldi {
class NotesModel;
class NoteNode;
}  // namespace vivaldi

namespace syncer {
struct EntityData;
}

namespace sync_bookmarks {
// Exposed for testing.
extern const base::Feature kInvalidateBookmarkSyncMetadataIfMismatchingGuid;
}  // namespace sync_bookmarks

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
    ~Entity();

    // Returns true if this data is out of sync with the server.
    // A commit may or may not be in progress at this time.
    bool IsUnsynced() const;

    // Check whether |data| matches the stored specifics hash. It ignores parent
    // information.
    bool MatchesDataIgnoringParent(const syncer::EntityData& data) const;

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

    const sync_pb::EntityMetadata* metadata() const {
      // TODO(crbug.com/516866): The below CHECK is added to debug some crashes.
      // Should be removed after figuring out the reason for the crash.
      CHECK(metadata_);
      return metadata_.get();
    }
    sync_pb::EntityMetadata* metadata() {
      // TODO(crbug.com/516866): The below CHECK is added to debug some crashes.
      // Should be removed after figuring out the reason for the crash.
      CHECK(metadata_);
      return metadata_.get();
    }

    bool commit_may_have_started() const { return commit_may_have_started_; }
    void set_commit_may_have_started(bool value) {
      commit_may_have_started_ = value;
    }

    // Returns whether the note's GUID is known to match the server-side
    // originator client item ID (or for pre-2015 notes, the equivalent
    // inferred GUID). This function may return false negatives since the
    // required local metadata got populated with M81.
    // TODO(crbug.com/1032052): Remove this code once all local sync metadata
    // is required to populate the client tag (and be considered invalid
    // otherwise).
    bool has_final_guid() const;

    // Returns true if the final GUID is known and it matches |guid|.
    bool final_guid_matches(const std::string& guid) const;

    // TODO(crbug.com/1032052): Remove this code once all local sync metadata
    // is required to populate the client tag (and be considered invalid
    // otherwise).
    void set_final_guid(const std::string& guid);

    // Returns the estimate of dynamically allocated memory in bytes.
    size_t EstimateMemoryUsage() const;

   private:
    // Null for tombstones.
    const vivaldi::NoteNode* note_node_;

    // Serializable Sync metadata.
    const std::unique_ptr<sync_pb::EntityMetadata> metadata_;

    // Whether there could be a commit sent to the server for this entity. It's
    // used to protect against sending tombstones for entities that have never
    // been sent to the server. It's only briefly false between the time was
    // first added to the tracker until the first commit request is sent to the
    // server. The tracker sets it to true in the constructor because this code
    // path is only executed in production when loading from disk.
    bool commit_may_have_started_ = false;

    DISALLOW_COPY_AND_ASSIGN(Entity);
  };

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

  ~SyncedNoteTracker();

  // This method is used to denote that all notes are reuploaded and there
  // is no need to reupload them again after next browser startup.
  void SetNotesFullTitleReuploaded();

  // Returns null if no entity is found.
  const Entity* GetEntityForSyncId(const std::string& sync_id) const;

  // Returns null if no entity is found.
  const SyncedNoteTracker::Entity* GetEntityForNoteNode(
      const vivaldi::NoteNode* node) const;

  // Returns null if no tombstone entity is found.
  const Entity* GetTombstoneEntityForGuid(const std::string& guid) const;

  // Starts tracking local note |note_node|, which must not be tracked
  // beforehand. The rest of the arguments represent the initial metadata.
  // Returns the tracked entity.
  const Entity* Add(const vivaldi::NoteNode* note_node,
                    const std::string& sync_id,
                    int64_t server_version,
                    base::Time creation_time,
                    const sync_pb::UniquePosition& unique_position,
                    const sync_pb::EntitySpecifics& specifics);

  // Updates the sync metadata for a tracked entity. |entity| must be owned by
  // this tracker.
  void Update(const Entity* entity,
              int64_t server_version,
              base::Time modification_time,
              const sync_pb::UniquePosition& unique_position,
              const sync_pb::EntitySpecifics& specifics);

  // Updates the server version of an existing entity. |entity| must be owned by
  // this tracker.
  void UpdateServerVersion(const Entity* entity, int64_t server_version);

  // Populates a note's final GUID. |entity| must be owned by this tracker.
  void PopulateFinalGuid(const Entity* entity, const std::string& guid);

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
  void UpdateSyncIdForLocalCreationIfNeeded(const Entity* entity,
                                            const std::string& sync_id);

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

  // Returns number of tracked entities. Used only in test.
  size_t TrackedEntitiesCountForTest() const;

  // Returns number of tracked notws that aren't deleted.
  size_t TrackedNotesCountForDebugging() const;

  // Returns number of notes that have been deleted but the server hasn't
  // confirmed the deletion yet.
  size_t TrackedUncommittedTombstonesCountForDebugging() const;

  // Checks whther all nodes in |notes_model| that *should* be tracked are
  // tracked.
  void CheckAllNodesTracked(const vivaldi::NotesModel* notes_model) const;

 private:
  explicit SyncedNoteTracker(sync_pb::ModelTypeState model_type_state,
                             bool notes_full_title_reuploaded);

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

  // This method is used to mark all entities except permanent nodes as
  // unsynced. This will cause reuploading of all notes. This reupload
  // should be initiated only when the |notes_full_title_reuploaded| field
  // in NotesMetadata is false. This field is used to prevent reuploading
  // after each browser restart. It is set to true in
  // BuildNotesModelMetadata.
  // TODO(crbug.com/1066962): remove this code when most of notes are
  // reuploaded.
  void ReuploadNotesOnLoadIfNeeded();

  // Recursive method that starting from |node| appends all corresponding
  // entities with updates in top-down order to |ordered_entities|.
  void TraverseAndAppend(
      const vivaldi::NoteNode* node,
      std::vector<const SyncedNoteTracker::Entity*>* ordered_entities) const;

  // A map of sync server ids to sync entities. This should contain entries and
  // metadata for almost everything.
  std::map<std::string, std::unique_ptr<Entity>> sync_id_to_entities_map_;

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
  // NotesMetadata::notes_full_title_reuploaded.
  // TODO(crbug.com/1066962): remove this code when most of notes are
  // reuploaded.
  bool notes_full_title_reuploaded_ = false;

  DISALLOW_COPY_AND_ASSIGN(SyncedNoteTracker);
};

}  // namespace sync_notes

#endif  // SYNC_NOTES_SYNCED_NOTE_TRACKER_H_
