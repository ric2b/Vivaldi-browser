// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTES_NOTES_MODEL_OBSERVER_H_
#define COMPONENTS_NOTES_NOTES_MODEL_OBSERVER_H_

#include "base/location.h"
#include "base/observer_list_types.h"

namespace vivaldi {

class NoteNode;

// Observer for the NotesModel.
class NotesModelObserver : public base::CheckedObserver {
 public:
  // Invoked when the model has finished loading. `ids_reassigned` mirrors
  // that of NoteLoadDetails::ids_reassigned. See it for details.
  virtual void NotesModelLoaded(bool ids_reassigned) {}

  // Invoked from the destructor of the NotesModel.
  virtual void NotesModelBeingDeleted() {}

  // Invoked when a node has moved.
  virtual void NotesNodeMoved(const NoteNode* old_parent,
                              size_t old_index,
                              const NoteNode* new_parent,
                              size_t new_index) {}

  // Invoked when a node has been added.
  virtual void NotesNodeAdded(const NoteNode* parent, size_t index) {}

  // Invoked before a node is removed.
  // `parent` the parent of the node that will be removed.
  // `old_index` the index of the node about to be removed in `parent`.
  // `node` is the node to be removed.
  virtual void OnWillRemoveNotes(const NoteNode* parent,
                                 size_t old_index,
                                 const NoteNode* node,
                                 const base::Location& location) {}

  // Invoked when a node has been removed, the item may still be starred though.
  // `parent` the parent of the node that was removed.
  // `old_index` the index of the removed node in `parent` before it was
  // removed.
  // `node` is the node that was removed.
  virtual void NotesNodeRemoved(const NoteNode* parent,
                                size_t old_index,
                                const NoteNode* node,
                                const base::Location& location) {}

  // Invoked before the title or url of a node is changed.
  virtual void OnWillChangeNotesNode(const NoteNode* node) {}

  // Invoked when the title or url of a node changes.
  virtual void NotesNodeChanged(const NoteNode* node) {}

  // Invoked before the direct children of `node` have been reordered in some
  // way, such as sorted.
  virtual void OnWillReorderNotesNode(const NoteNode* node) {}

  // Invoked when the children (just direct children, not descendants) of
  // `node` have been reordered in some way, such as sorted.
  virtual void NotesNodeChildrenReordered(const NoteNode* node) {}

  // Invoked before an extensive set of model changes is about to begin.
  // This tells UI intensive observers to wait until the updates finish to
  // update themselves.
  // These methods should only be used for imports and sync.
  // Observers should still respond to NotesNodeRemoved immediately,
  // to avoid holding onto stale node pointers.
  virtual void ExtensiveNotesChangesBeginning() {}

  // Invoked after an extensive set of model changes has ended.
  // This tells observers to update themselves if they were waiting for the
  // update to finish.
  virtual void ExtensiveNotesChangesEnded() {}

  // Invoked before all non-permanent notes nodes are removed.
  virtual void OnWillRemoveAllNotes(const base::Location& location) {}

  // Invoked when all non-permanent notes nodes have been removed.
  virtual void NotesAllNodesRemoved(const base::Location& location) {}

  virtual void GroupedNotesChangesBeginning() {}
  virtual void GroupedNotesChangesEnded() {}

 protected:
  ~NotesModelObserver() override {}
};

}  // namespace vivaldi

#endif  // COMPONENTS_NOTES_NOTES_MODEL_OBSERVER_H_
