// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTES_NOTE_REMOTE_UPDATES_HANDLER_H_
#define SYNC_NOTES_NOTE_REMOTE_UPDATES_HANDLER_H_

#include <map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "sync/notes/synced_note_tracker.h"

namespace vivaldi {
class NoteNode;
}  // namespace vivaldi

namespace sync_notes {

class NoteModelView;

// Responsible for processing one batch of remote updates received from the sync
// server.
class NoteRemoteUpdatesHandler {
 public:
  // |notes_model| and |note_tracker| must not be null
  // and must outlive this object.
  NoteRemoteUpdatesHandler(NoteModelView* notes_model,
                           SyncedNoteTracker* note_tracker);

  NoteRemoteUpdatesHandler(const NoteRemoteUpdatesHandler&) = delete;
  NoteRemoteUpdatesHandler& operator=(const NoteRemoteUpdatesHandler&) = delete;

  // Processes the updates received from the sync server in |updates| and
  // updates the |notes_model_| and |note_tracker_| accordingly. If
  // |got_new_encryption_requirements| is true, it recommits all tracked
  // entities except those in |updates| which should use the latest encryption
  // key and hence don't need recommitting.
  void Process(const syncer::UpdateResponseDataList& updates,
               bool got_new_encryption_requirements);

  // Public for testing.
  static std::vector<const syncer::UpdateResponseData*>
  ReorderValidUpdatesForTest(const syncer::UpdateResponseDataList* updates);

  static size_t ComputeChildNodeIndexForTest(
      const vivaldi::NoteNode* parent,
      const sync_pb::UniquePosition& unique_position,
      const SyncedNoteTracker* note_tracker);

 private:
  // Reorders incoming updates such that parent creation is before child
  // creation and child deletion is before parent deletion, and deletions should
  // come last. The returned pointers point to the elements in the original
  // |updates|. In this process, invalid updates are filtered out.
  static std::vector<const syncer::UpdateResponseData*> ReorderValidUpdates(
      const syncer::UpdateResponseDataList* updates);

  // Returns the tracked entity that should be affected by a remote change, or
  // null if there is none (e.g. indicating a remote creation).
  // |should_ignore_update| must not be null and it can be marked as true if the
  // function reports that the update should not be processed further (e.g. it
  // is invalid).
  const SyncedNoteTrackerEntity* DetermineLocalTrackedEntityToUpdate(
      const syncer::EntityData& update_entity,
      bool* should_ignore_update);

  // Given a remote update entity, it returns the parent note node of the
  // corresponding node. It returns null if the parent node cannot be found.
  const vivaldi::NoteNode* GetParentNode(
      const syncer::EntityData& update_entity) const;

  // Processes a remote creation of a note node.
  // 1. For permanent folders, they are only registered in |note_tracker_|.
  // 2. If the nodes parent cannot be found, the remote creation update is
  //    ignored.
  // 3. Otherwise, a new node is created in the local note model and
  //    registered in |note_tracker_|.
  //
  // Returns the newly tracked entity or null if the creation failed.
  const SyncedNoteTrackerEntity* ProcessCreate(
      const syncer::UpdateResponseData& update);

  // Processes a remote update of a note node. |update| must not be a
  // deletion, and the server_id must be already tracked, otherwise, it is a
  // creation that gets handled in ProcessCreate(). |tracked_entity| is
  // the tracked entity for that server_id. It is passed as a dependency instead
  // of performing a lookup inside ProcessUpdate() to avoid wasting CPU
  // cycles for doing another lookup (this code runs on the UI thread).
  void ProcessUpdate(const syncer::UpdateResponseData& update,
                     const SyncedNoteTrackerEntity* tracked_entity);

  // Processes a remote delete of a note node. |update_entity| must not be a
  // deletion. |tracked_entity| is the tracked entity for that server_id. It is
  // passed as a dependency instead of performing a lookup inside
  // ProcessDelete() to avoid wasting CPU cycles for doing another lookup
  // (this code runs on the UI thread).
  void ProcessDelete(const syncer::EntityData& update_entity,
                     const SyncedNoteTrackerEntity* tracked_entity);

  // Processes a conflict where the note has been changed both locally and
  // remotely. It applies the general policy the server wins except in the case
  // of remote deletions in which local wins. |tracked_entity| is the tracked
  // entity for that server_id. It is passed as a dependency instead of
  // performing a lookup inside ProcessDelete() to avoid wasting CPU cycles for
  // doing another lookup (this code runs on the UI thread). Returns the tracked
  // entity (if any) as a result of resolving the conflict, which is often the
  // same as the input |tracked_entity|, but may also be different, including
  // null (if the conflict led to untracking).
  [[nodiscard]] const SyncedNoteTrackerEntity* ProcessConflict(
      const syncer::UpdateResponseData& update,
      const SyncedNoteTrackerEntity* tracked_entity);

  // Recursively removes the entities corresponding to |node| and its children
  // from |note_tracker_|.
  void RemoveEntityAndChildrenFromTracker(const vivaldi::NoteNode* node);

  // Initiate reupload for the update with |entity_data|. |tracked_entity| must
  // not be nullptr.
  void ReuploadEntityIfNeeded(const syncer::EntityData& entity_data,
                              const SyncedNoteTrackerEntity* tracked_entity);

  const raw_ptr<NoteModelView> notes_model_;
  const raw_ptr<SyncedNoteTracker> note_tracker_;
};

}  // namespace sync_notes

#endif  // SYNC_NOTES_NOTE_REMOTE_UPDATES_HANDLER_H_
