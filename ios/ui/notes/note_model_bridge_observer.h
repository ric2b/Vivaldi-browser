// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_NOTES_NOTE_MODEL_BRIDGE_OBSERVER_H_
#define IOS_UI_NOTES_NOTE_MODEL_BRIDGE_OBSERVER_H_

#import <Foundation/Foundation.h>

#import "base/compiler_specific.h"
#import "components/notes/notes_model.h"
#import "components/notes/notes_model_observer.h"

// The ObjC translations of the C++ observer callbacks are defined here.
@protocol NoteModelBridgeObserver <NSObject>
// The note model has loaded.
- (void)noteModelLoaded;
// The node has changed, but not its children.
- (void)noteNodeChanged:(const vivaldi::NoteNode*)notesNode;
// The node has not changed, but the ordering and existence of its children have
// changed.
- (void)noteNodeChildrenChanged:(const vivaldi::NoteNode*)noteNode;
// The node has moved to a new parent folder.
- (void)noteNode:(const vivaldi::NoteNode*)noteNode
    movedFromParent:(const vivaldi::NoteNode*)oldParent
           toParent:(const vivaldi::NoteNode*)newParent;
// |node| was deleted from |folder|.
- (void)noteNodeDeleted:(const vivaldi::NoteNode*)node
             fromFolder:(const vivaldi::NoteNode*)folder;
// All non-permanent nodes have been removed.
- (void)noteModelRemovedAllNodes;

@end

namespace notes {
// A bridge that translates vivaldi::NotesModelObserver C++ callbacks into ObjC
// callbacks.

class NoteModelBridge : public vivaldi::NotesModelObserver {
 public:
  explicit NoteModelBridge(id<NoteModelBridgeObserver> observer,
                           vivaldi::NotesModel* model);
  ~NoteModelBridge() override;

 private:
  void NotesModelLoaded(bool ids_reassigned) override;
  void NotesModelBeingDeleted() override;
  void NotesNodeMoved(const vivaldi::NoteNode* old_parent,
                      size_t old_index,
                      const vivaldi::NoteNode* new_parent,
                      size_t new_index) override;
  void NotesNodeAdded(const vivaldi::NoteNode* parent, size_t index) override;
  void NotesNodeRemoved(const vivaldi::NoteNode* parent,
                        size_t old_index,
                        const vivaldi::NoteNode* node,
                        const base::Location& location) override;
  void NotesNodeChanged(const vivaldi::NoteNode* node) override;
  void NotesNodeChildrenReordered(const vivaldi::NoteNode* node) override;

  __weak id<NoteModelBridgeObserver> observer_;
  vivaldi::NotesModel* model_;  // weak
};
}  // namespace notes

#endif  // IOS_UI_NOTES_NOTE_MODEL_BRIDGE_OBSERVER_H_
