// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTES_NOTES_MODEL_OBSERVER_IMPL_H_
#define SYNC_NOTES_NOTES_MODEL_OBSERVER_IMPL_H_

#include <set>
#include <string>

#include "base/callback.h"
#include "notes/note_node.h"
#include "notes/notes_model_observer.h"
#include "url/gurl.h"

namespace syncer {
class UniquePosition;
}

namespace sync_notes {

class SyncedNoteTracker;

// Class for listening to local changes in the notes model and updating
// metadata in SyncedNoteTracker, such that ultimately the processor exposes
// those local changes to the sync engine.
class NotesModelObserverImpl : public vivaldi::NotesModelObserver {
 public:
  // |note_tracker_| must not be null and must outlive this object.
  NotesModelObserverImpl(
      const base::RepeatingClosure& nudge_for_commit_closure,
      base::OnceClosure on_notes_model_being_deleted_closure,
      SyncedNoteTracker* note_tracker);
  ~NotesModelObserverImpl() override;

  //  vivaldi::NotesModelObserver:
  void NotesModelLoaded(vivaldi::NotesModel* model,
                        bool ids_reassigned) override;
  void NotesModelBeingDeleted(vivaldi::NotesModel* model) override;
  void NotesNodeMoved(vivaldi::NotesModel* model,
                      const vivaldi::NoteNode* old_parent,
                      size_t old_index,
                      const vivaldi::NoteNode* new_parent,
                      size_t new_index) override;
  void NotesNodeAdded(vivaldi::NotesModel* model,
                      const vivaldi::NoteNode* parent,
                      size_t index) override;
  void OnWillRemoveNotes(vivaldi::NotesModel* model,
                         const vivaldi::NoteNode* parent,
                         size_t old_index,
                         const vivaldi::NoteNode* node) override;
  void NotesNodeRemoved(vivaldi::NotesModel* model,
                        const vivaldi::NoteNode* parent,
                        size_t old_index,
                        const vivaldi::NoteNode* node) override;
  void NotesNodeChanged(vivaldi::NotesModel* model,
                        const vivaldi::NoteNode* node) override;
  void NotesNodeAttachmentChanged(vivaldi::NotesModel* model,
                                  const vivaldi::NoteNode* node) override;
  void NotesNodeChildrenReordered(vivaldi::NotesModel* model,
                                  const vivaldi::NoteNode* node) override;
  void OnWillRemoveAllNotes(vivaldi::NotesModel* model) override;
  void NotesAllNodesRemoved(vivaldi::NotesModel* model) override;

 private:
  syncer::UniquePosition ComputePosition(const vivaldi::NoteNode& parent,
                                         size_t index,
                                         const std::string& sync_id);

  // Processes the deletion of a note node and updates the
  // |note_tracker_| accordingly. If |node| is a note, it gets marked
  // as deleted and that it requires a commit. If it's a folder, it recurses
  // over all children before processing the folder itself.
  void ProcessDelete(const vivaldi::NoteNode* parent,
                     const vivaldi::NoteNode* node);

  // Points to the tracker owned by the processor. It keeps the mapping between
  // note nodes and corresponding sync server entities.
  SyncedNoteTracker* const note_tracker_;

  // The callback used to inform the sync engine that there are local changes to
  // be committed.
  const base::RepeatingClosure nudge_for_commit_closure_;

  // The callback used to inform the processor that the note is getting
  // deleted.
  base::OnceClosure on_notes_model_being_deleted_closure_;

  DISALLOW_COPY_AND_ASSIGN(NotesModelObserverImpl);
};

}  // namespace sync_notes

#endif  // SYNC_NOTES_NOTES_MODEL_OBSERVER_IMPL_H_
