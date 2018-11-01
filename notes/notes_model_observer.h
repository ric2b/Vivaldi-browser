// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NOTES_NOTES_MODEL_OBSERVER_H_
#define NOTES_NOTES_MODEL_OBSERVER_H_

namespace vivaldi {

class Notes_Model;
class Notes_Node;

// Observer for the Notes_Model.
class NotesModelObserver {
 public:
  // Invoked when the model has finished loading. |ids_reassigned| mirrors
  // that of NotesLoadDetails::ids_reassigned. See it for details.
  virtual void NotesModelLoaded(Notes_Model* model, bool ids_reassigned) {}

  // Invoked from the destructor of the Notes_Model.
  virtual void NotesModelBeingDeleted(Notes_Model* model) {}

  // Invoked when a node has moved.
  virtual void NotesNodeMoved(Notes_Model* model,
                              const Notes_Node* old_parent,
                              int old_index,
                              const Notes_Node* new_parent,
                              int new_index) {}

  // Invoked when a node has been added.
  virtual void NotesNodeAdded(Notes_Model* model,
                              const Notes_Node* parent,
                              int index) {}

  // Invoked before a node is removed.
  // |parent| the parent of the node that will be removed.
  // |old_index| the index of the node about to be removed in |parent|.
  // |node| is the node to be removed.
  virtual void OnWillRemoveNotes(Notes_Model* model,
                                 const Notes_Node* parent,
                                 int old_index,
                                 const Notes_Node* node) {}

  // Invoked when a node has been removed, the item may still be starred though.
  // |parent| the parent of the node that was removed.
  // |old_index| the index of the removed node in |parent| before it was
  // removed.
  // |node| is the node that was removed.
  virtual void NotesNodeRemoved(Notes_Model* model,
                                const Notes_Node* parent,
                                int old_index,
                                const Notes_Node* node) {}

  // Invoked before the title or url of a node is changed.
  virtual void OnWillChangeNotesNode(Notes_Model* model,
                                     const Notes_Node* node) {}

  // Invoked when the title or url of a node changes.
  virtual void NotesNodeChanged(Notes_Model* model, const Notes_Node* node) {}

  // Invoked when a attachment has been loaded or changed.
  virtual void NotesNodeAttachmentChanged(Notes_Model* model,
                                          const Notes_Node* node) {}

  // Invoked before the direct children of |node| have been reordered in some
  // way, such as sorted.
  virtual void OnWillReorderNotesNode(Notes_Model* model,
                                      const Notes_Node* node) {}

  // Invoked when the children (just direct children, not descendants) of
  // |node| have been reordered in some way, such as sorted.
  virtual void NotesNodeChildrenReordered(Notes_Model* model,
                                          const Notes_Node* node) {}

  // Invoked before an extensive set of model changes is about to begin.
  // This tells UI intensive observers to wait until the updates finish to
  // update themselves.
  // These methods should only be used for imports and sync.
  // Observers should still respond to NotesNodeRemoved immediately,
  // to avoid holding onto stale node pointers.
  virtual void ExtensiveNotesChangesBeginning(Notes_Model* model) {}

  // Invoked after an extensive set of model changes has ended.
  // This tells observers to update themselves if they were waiting for the
  // update to finish.
  virtual void ExtensiveNotesChangesEnded(Notes_Model* model) {}

  // Invoked before all non-permanent notes nodes are removed.
  virtual void OnWillRemoveAllNotes(Notes_Model* model) {}

  // Invoked when all non-permanent notes nodes have been removed.
  virtual void NotesAllNodesRemoved(Notes_Model* model) {}

  virtual void GroupedNotesChangesBeginning(Notes_Model* model) {}
  virtual void GroupedNotesChangesEnded(Notes_Model* model) {}

 protected:
  virtual ~NotesModelObserver() {}
};

}  // namespace vivaldi

#endif  // NOTES_NOTES_MODEL_OBSERVER_H_
