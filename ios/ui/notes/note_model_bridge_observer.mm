// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/notes/note_model_bridge_observer.h"

#import <Foundation/Foundation.h>

#import "base/check.h"
#import "base/notreached.h"
#import "notes/notes_model.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace notes {

NoteModelBridge::NoteModelBridge(
    id<NoteModelBridgeObserver> observer,
    vivaldi::NotesModel* model)
    : observer_(observer), model_(model) {
  DCHECK(observer_);
  DCHECK(model_);
  model_->AddObserver(this);
}

NoteModelBridge::~NoteModelBridge() {
  if (model_) {
    model_->RemoveObserver(this);
  }
}

void NoteModelBridge::NotesModelLoaded(vivaldi::NotesModel* model,
                                              bool ids_reassigned) {
  [observer_ noteModelLoaded];
}

void NoteModelBridge::NotesModelBeingDeleted(vivaldi::NotesModel* model) {
  DCHECK(model_);
  model_->RemoveObserver(this);
  model_ = nullptr;
}

void NoteModelBridge::NotesNodeMoved(vivaldi::NotesModel* model,
                                            const vivaldi::NoteNode* old_parent,
                                            size_t old_index,
                                            const vivaldi::NoteNode* new_parent,
                                            size_t new_index) {
  const vivaldi::NoteNode* node = new_parent->children()[new_index].get();
  [observer_ noteNode:node movedFromParent:old_parent toParent:new_parent];
}

void NoteModelBridge::NotesNodeAdded(vivaldi::NotesModel* model,
                                            const vivaldi::NoteNode* parent,
                                            size_t index) {
   [observer_ noteNodeChildrenChanged:parent];
}

void NoteModelBridge::NotesNodeRemoved(
    vivaldi::NotesModel* model,
    const vivaldi::NoteNode* parent,
    size_t old_index,
    const vivaldi::NoteNode* node) {
  // Hold a non-weak reference to |observer_|, in case the first event below
  // destroys |this|.
  id<NoteModelBridgeObserver> observer = observer_;

  [observer noteNodeDeleted:node fromFolder:parent];
  [observer noteNodeChildrenChanged:parent];
}

void NoteModelBridge::NotesNodeChanged(vivaldi::NotesModel* model,
                                              const vivaldi::NoteNode* node) {
  [observer_ noteNodeChanged:node];
}

void NoteModelBridge::NotesNodeChildrenReordered(
    vivaldi::NotesModel* model,
    const vivaldi::NoteNode* node) {
  [observer_ noteNodeChildrenChanged:node];
}

}  // namespace Note s
