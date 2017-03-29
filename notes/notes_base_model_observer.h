// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIVALDI_NOTES_NOTES_BASE_MODEL_OBSERVER_H_
#define VIVALDI_NOTES_NOTES_BASE_MODEL_OBSERVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "notes/notes_model_observer.h"


namespace Vivaldi {

// Observer for the Notes_Model.
class NotesBaseModelObserver : public NotesModelObserver {
public:

	virtual void NotesModelChanged() = 0;

	// Invoked when the model has finished loading. |ids_reassigned| mirrors
	// that of NotesLoadDetails::ids_reassigned. See it for details.
	void Loaded(Notes_Model* model, bool ids_reassigned) override;

	// Invoked when a node has moved.
	void NotesNodeMoved(Notes_Model* model,
		const Notes_Node* old_parent,
		int old_index,
		const Notes_Node* new_parent,
		int new_index)  override;

	// Invoked when a node has been added.
	void NotesNodeAdded(Notes_Model* model,
		const Notes_Node* parent,
		int index) override;

	// Invoked when a node has been removed, the item may still be starred though.
	// |parent| the parent of the node that was removed.
	// |old_index| the index of the removed node in |parent| before it was
	// removed.
	// |node| is the node that was removed.
	void NotesNodeRemoved(Notes_Model* model,
		const Notes_Node* parent,
		int old_index,
		const Notes_Node* node) override;

	// Invoked when the title or url of a node changes.
	void NotesNodeChanged(Notes_Model* model,
                                   const Notes_Node* node) override;

	// Invoked when a favicon has been loaded or changed.
	void NotesNodeFaviconChanged(Notes_Model* model,
		const Notes_Node* node) override;

	// Invoked when the children (just direct children, not descendants) of
	// |node| have been reordered in some way, such as sorted.
	void NotesNodeChildrenReordered(Notes_Model* model,
		const Notes_Node* node) override;

	// Invoked when all non-permanent bookmark nodes have been removed.
	void NotesAllNodesRemoved(Notes_Model* model) override;

protected:
	~NotesBaseModelObserver() override {}
};

}
#endif  // VIVALDI_NOTES_NOTES_BASE_MODEL_OBSERVER_H_
