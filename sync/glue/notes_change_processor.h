// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_GLUE_NOTES_CHANGE_PROCESSOR_H_
#define SYNC_GLUE_NOTES_CHANGE_PROCESSOR_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/threading/thread_checker.h"
#include "components/sync/model/change_processor.h"
#include "components/sync/model/data_type_error_handler.h"
#include "notes/notes_model_observer.h"
#include "notes/notesnode.h"
#include "sync/glue/notes_model_associator.h"

class Profile;
using vivaldi::Notes_Model;
using vivaldi::Notes_Node;
using vivaldi::NotesModelObserver;

namespace base {
class RefCountedMemory;
}

namespace syncer {
class WriteNode;
class WriteTransaction;
}  // namespace syncer

namespace syncer {
class SyncClient;
}

namespace vivaldi {

// This class is responsible for taking changes from the Notes_Model
// and applying them to the sync API 'syncable' model, and vice versa.
// All operations and use of this class are from the UI thread.
// This is currently notes specific.
class NotesChangeProcessor : public NotesModelObserver,
                             public syncer::ChangeProcessor {
 public:
  NotesChangeProcessor(
      syncer::SyncClient* sync_client,
      NotesModelAssociator* model_associator,
      std::unique_ptr<syncer::DataTypeErrorHandler> error_handler);
  ~NotesChangeProcessor() override;

  // NotesModelObserver implementation.
  // Notes_Model -> sync API model change application.
  void NotesModelLoaded(Notes_Model* model, bool ids_reassigned) override;
  void NotesModelBeingDeleted(Notes_Model* model) override;
  void NotesNodeMoved(Notes_Model* model,
                      const Notes_Node* old_parent,
                      int old_index,
                      const Notes_Node* new_parent,
                      int new_index) override;
  void NotesNodeAdded(Notes_Model* model,
                      const Notes_Node* parent,
                      int index) override;
  void OnWillRemoveNotes(Notes_Model* model,
                         const Notes_Node* parent,
                         int old_index,
                         const Notes_Node* node) override;
  void NotesNodeRemoved(Notes_Model* model,
                        const Notes_Node* parent,
                        int index,
                        const Notes_Node* node) override;
  void NotesAllNodesRemoved(Notes_Model* model) override;
  void NotesNodeChanged(Notes_Model* model, const Notes_Node* node) override;
  // Invoked when a attachment has been loaded or changed.
  void NotesNodeAttachmentChanged(Notes_Model* model,
                                  const Notes_Node* node) override;

  void NotesNodeChildrenReordered(Notes_Model* model,
                                  const Notes_Node* node) override;

  // The change processor implementation, responsible for applying changes from
  // the sync model to the notes model.
  void ApplyChangesFromSyncModel(
      const syncer::BaseTransaction* trans,
      int64_t model_version,
      const syncer::ImmutableChangeRecordList& changes) override;

  // The following methods are static and hence may be invoked at any time, and
  // do not depend on having a running ChangeProcessor.

  // Updates the title, URL, creation time and favicon of the notes |node|
  // with data taken from the |sync_node| sync node.
  static void UpdateNoteWithSyncData(const syncer::BaseNode& sync_node,
                                     Notes_Model* model,
                                     const Notes_Node* node,
                                     syncer::SyncClient* sync_client);

  // Creates a notes node under the given parent node from the given sync
  // node. Returns the newly created node.  The created node is placed at the
  // specified index among the parent's children.
  static const Notes_Node* CreateNotesEntry(syncer::BaseNode* sync_node,
                                            const Notes_Node* parent,
                                            Notes_Model* model,
                                            syncer::SyncClient* sync_client,
                                            int index);

  static const Notes_Node* CreateNotesEntry(const base::string16& title,
                                            const GURL& url,
                                            const syncer::BaseNode* sync_node,
                                            const Notes_Node* parent,
                                            Notes_Model* model,
                                            syncer::SyncClient* sync_client,
                                            int index);

  // Sets the favicon of the given notes node from the given sync node.
  // Returns whether the favicon was set in the notes node.
  // |profile| is the profile that contains the otes_Model
  // for the Note in question.
  static bool SetNoteIcon(const syncer::BaseNode* sync_node,
                          const Notes_Node* notes_node,
                          Notes_Model* model,
                          syncer::SyncClient* sync_client);

  // Treat the |index|th child of |parent| as a newly added node, and create a
  // corresponding node in the sync domain using |trans|.  All properties
  // will be transferred to the new node.  A node corresponding to |parent|
  // must already exist and be associated for this call to succeed.  Returns
  // the ID of the just-created node, or if creation fails, kInvalidID.
  static int64_t CreateSyncNode(const Notes_Node* parent,
                                Notes_Model* model,
                                int index,
                                syncer::WriteTransaction* trans,
                                NotesModelAssociator* associator,
                                syncer::DataTypeErrorHandler* error_handler);

  // Update |notes_node|'s sync node.
  static int64_t UpdateSyncNode(const Notes_Node* notes_node,
                                Notes_Model* model,
                                syncer::WriteTransaction* trans,
                                NotesModelAssociator* associator,
                                syncer::DataTypeErrorHandler* error_handler);

  // Tombstone |topmost_sync_node| node and all its children in the sync domain
  // using transaction |trans|. Returns the number of removed nodes.
  static int RemoveSyncNodeHierarchy(syncer::WriteTransaction* trans,
                                     syncer::WriteNode* topmost_sync_node,
                                     NotesModelAssociator* associator);

  // Update transaction version of |model| and |nodes| to |new_version| if
  // it's valid.
  static void UpdateTransactionVersion(
      int64_t new_version,
      Notes_Model* model,
      const std::vector<const Notes_Node*>& nodes);

 protected:
  void StartImpl() override;

 private:
  enum MoveOrCreate {
    MOVE,
    CREATE,
  };

  // Helper function used to fix the position of a sync node so that it matches
  // the position of a corresponding notes model node. |parent| and
  // |index| identify the notes model position.  |dst| is the node whose
  // position is to be fixed.  If |operation| is CREATE, treat |dst| as an
  // uncreated node and set its position via InitByCreation(); otherwise,
  // |dst| is treated as an existing node, and its position will be set via
  // SetPosition().  |trans| is the transaction to which |dst| belongs. Returns
  // false on failure.
  static bool PlaceSyncNode(MoveOrCreate operation,
                            const Notes_Node* parent,
                            int index,
                            syncer::WriteTransaction* trans,
                            syncer::WriteNode* dst,
                            NotesModelAssociator* associator);

  // Copy properties (but not position) from |src| to |dst|.
  static void UpdateSyncNodeProperties(
      const Notes_Node* src,
      Notes_Model* model,
      syncer::WriteNode* dst,
      syncer::DataTypeErrorHandler* error_handler);

  static void UpdateNoteWithAttachmentData(
      const sync_pb::NotesSpecifics& specifics,
      const Notes_Node* node);

  // Helper function to encode a note's favicon into raw PNG data.
  static void EncodeIcon(const Notes_Node* src,
                         Notes_Model* model,
                         scoped_refptr<base::RefCountedMemory>* dst);

  // Remove all sync nodes, except the permanent nodes.
  void RemoveAllSyncNodes();

  // Remove all the sync nodes associated with |node| and its children.
  void RemoveSyncNodeHierarchy(const Notes_Node* node);

  // Remove all children of |sync_node|. Returns the number of removed
  // children.
  static int RemoveAllChildNodes(syncer::WriteTransaction* trans,
                                 int64_t sync_id,
                                 NotesModelAssociator* associator);

  // Remove |sync_node|. It should not have any children.
  static void RemoveOneSyncNode(syncer::WriteNode* sync_node,
                                NotesModelAssociator* associator);

  // Creates or updates a sync node associated with |node|.
  void CreateOrUpdateSyncNode(const Notes_Node* node);

  // Returns false if |node| should not be synced.
  bool CanSyncNode(const Notes_Node* node);

  base::ThreadChecker thread_checker_;

  // The notes model we are processing changes from.  Non-NULL when
  // |running_| is true.
  Notes_Model* notes_model_;

  syncer::SyncClient* sync_client_;

  // The two models should be associated according to this ModelAssociator.
  NotesModelAssociator* model_associator_;

  DISALLOW_COPY_AND_ASSIGN(NotesChangeProcessor);
};

}  // namespace vivaldi

#endif  // SYNC_GLUE_NOTES_CHANGE_PROCESSOR_H_
