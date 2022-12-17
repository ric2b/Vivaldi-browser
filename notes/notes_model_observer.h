// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NOTES_NOTES_MODEL_OBSERVER_H_
#define NOTES_NOTES_MODEL_OBSERVER_H_

#include "base/observer_list_types.h"

namespace vivaldi {

class NotesModel;
class NoteNode;

// Observer for the NotesModel.
class NotesModelObserver : public base::CheckedObserver {
 public:
  // Invoked when the model has finished loading. |ids_reassigned| mirrors
  // that of NoteLoadDetails::ids_reassigned. See it for details.
  virtual void NotesModelLoaded(NotesModel* model, bool ids_reassigned) {}

  // Invoked from the destructor of the NotesModel.
  virtual void NotesModelBeingDeleted(NotesModel* model) {}

  // Invoked when a node has moved.
  virtual void NotesNodeMoved(NotesModel* model,
                              const NoteNode* old_parent,
                              size_t old_index,
                              const NoteNode* new_parent,
                              size_t new_index) {}

  // Invoked when a node has been added.
  virtual void NotesNodeAdded(NotesModel* model,
                              const NoteNode* parent,
                              size_t index) {}

  // Invoked before a node is removed.
  // |parent| the parent of the node that will be removed.
  // |old_index| the index of the node about to be removed in |parent|.
  // |node| is the node to be removed.
  virtual void OnWillRemoveNotes(NotesModel* model,
                                 const NoteNode* parent,
                                 size_t old_index,
                                 const NoteNode* node) {}

  // Invoked when a node has been removed, the item may still be starred though.
  // |parent| the parent of the node that was removed.
  // |old_index| the index of the removed node in |parent| before it was
  // removed.
  // |node| is the node that was removed.
  virtual void NotesNodeRemoved(NotesModel* model,
                                const NoteNode* parent,
                                size_t old_index,
                                const NoteNode* node) {}

  // Invoked before the title or url of a node is changed.
  virtual void OnWillChangeNotesNode(NotesModel* model, const NoteNode* node) {}

  // Invoked when the title or url of a node changes.
  virtual void NotesNodeChanged(NotesModel* model, const NoteNode* node) {}

  // Invoked before the direct children of |node| have been reordered in some
  // way, such as sorted.
  virtual void OnWillReorderNotesNode(NotesModel* model, const NoteNode* node) {
  }

  // Invoked when the children (just direct children, not descendants) of
  // |node| have been reordered in some way, such as sorted.
  virtual void NotesNodeChildrenReordered(NotesModel* model,
                                          const NoteNode* node) {}

  // Invoked before an extensive set of model changes is about to begin.
  // This tells UI intensive observers to wait until the updates finish to
  // update themselves.
  // These methods should only be used for imports and sync.
  // Observers should still respond to NotesNodeRemoved immediately,
  // to avoid holding onto stale node pointers.
  virtual void ExtensiveNotesChangesBeginning(NotesModel* model) {}

  // Invoked after an extensive set of model changes has ended.
  // This tells observers to update themselves if they were waiting for the
  // update to finish.
  virtual void ExtensiveNotesChangesEnded(NotesModel* model) {}

  // Invoked before all non-permanent notes nodes are removed.
  virtual void OnWillRemoveAllNotes(NotesModel* model) {}

  // Invoked when all non-permanent notes nodes have been removed.
  virtual void NotesAllNodesRemoved(NotesModel* model) {}

  virtual void GroupedNotesChangesBeginning(NotesModel* model) {}
  virtual void GroupedNotesChangesEnded(NotesModel* model) {}

 protected:
  ~NotesModelObserver() override {}
};

}  // namespace vivaldi

#endif  // NOTES_NOTES_MODEL_OBSERVER_H_
