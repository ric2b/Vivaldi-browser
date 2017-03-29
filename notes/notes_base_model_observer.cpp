// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "notes/notes_base_model_observer.h"

namespace Vivaldi {

void NotesBaseModelObserver::Loaded(Notes_Model* model, bool ids_reassigned)
{
}

void NotesBaseModelObserver::NotesNodeMoved(Notes_Model* model,
                                 const Notes_Node* old_parent,
                                 int old_index,
                                 const Notes_Node* new_parent,
                                 int new_index)
{
	NotesModelChanged();
}

void NotesBaseModelObserver::NotesNodeAdded(Notes_Model* model,
                                 const Notes_Node* parent,
                                 int index)
{
	NotesModelChanged();
}

void NotesBaseModelObserver::NotesNodeRemoved(Notes_Model* model,
                                   const Notes_Node* parent,
                                   int old_index,
                                   const Notes_Node* node)
{
	NotesModelChanged();
}

void NotesBaseModelObserver::NotesNodeChanged(Notes_Model* model,
                                   const Notes_Node* node)
{
	NotesModelChanged();
}

void NotesBaseModelObserver::NotesNodeFaviconChanged(Notes_Model* model,
                                          const Notes_Node* node)
{
}

void NotesBaseModelObserver::NotesNodeChildrenReordered(Notes_Model* model,
                                             const Notes_Node* node)
{
	NotesModelChanged();
}


void NotesBaseModelObserver::NotesAllNodesRemoved(Notes_Model* model)
{
	NotesModelChanged();
}

}
