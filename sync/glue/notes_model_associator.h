// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_GLUE_NOTES_MODEL_ASSOCIATOR_H_
#define SYNC_GLUE_NOTES_MODEL_ASSOCIATOR_H_

#include <map>
#include <memory>
#include <set>
#include <stack>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/threading/thread_checker.h"
#include "components/sync/base/unrecoverable_error_handler.h"
#include "components/sync/driver/data_type_controller.h"
#include "components/sync/driver/model_associator.h"
#include "components/sync/model/data_type_error_handler.h"

class Profile;
class GURL;

namespace vivaldi {
class Notes_Model;
class Notes_Node;
}

using vivaldi::Notes_Model;
using vivaldi::Notes_Node;

namespace syncer {
class BaseNode;
class BaseTransaction;
struct UserShare;
class WriteTransaction;
}

namespace syncer {
class SyncClient;
}

namespace vivaldi {

// Contains all model association related logic:
// * Algorithm to associate notes model and sync model.
// * Methods to get a notes node for a given sync node and vice versa.
// * Persisting model associations and loading them back.
class NotesModelAssociator
    : public syncer::AssociatorInterface {
 public:
  static syncer::ModelType model_type() { return syncer::NOTES; }
  NotesModelAssociator(Notes_Model* notes_model,
                       syncer::SyncClient* sync_client,
                       syncer::UserShare* user_share,
                       std::unique_ptr<syncer::DataTypeErrorHandler>
                           unrecoverable_error_handler);
  ~NotesModelAssociator() override;

  // AssociatorInterface implementation.
  //
  // AssociateModels iterates through both the sync and the browser
  // notes model, looking for matched pairs of items.  For any pairs it
  // finds, it will call AssociateSyncID.  For any unmatched items,
  // MergeAndAssociateModels will try to repair the match, e.g. by adding a new
  // node.  After successful completion, the models should be identical and
  // corresponding. Returns true on success.  On failure of this step, we
  // should abort the sync operation and report an error to the user.
  syncer::SyncError AssociateModels(
      syncer::SyncMergeResult* local_merge_result,
      syncer::SyncMergeResult* syncer_merge_result) override;

  syncer::SyncError DisassociateModels() override;

  // The has_nodes out param is true if the sync model has nodes other
  // than the permanent tagged nodes.
  bool SyncModelHasUserCreatedNodes(bool* has_nodes) override;

  // Returns sync id for the given notes node id.
  // Returns syncer::kInvalidId if the sync node is not found for the given
  // notes node id.
  int64_t GetSyncIdFromChromeId(const int64_t& node_id);

  // Returns the notes node for the given sync id.
  // Returns NULL if no notes node is found for the given sync id.
  const Notes_Node* GetChromeNodeFromSyncId(int64_t sync_id);

  // Initializes the given sync node from the given notes node id.
  // Returns false if no sync node was found for the given notes node id or
  // if the initialization of sync node fails.
  bool InitSyncNodeFromChromeId(const int64_t& node_id,
                                syncer::BaseNode* sync_node);

  void AddAssociation(const Notes_Node* node, int64_t sync_id);

  // Associates the given notes node with the given sync id.
  void Associate(const Notes_Node* node,
                 const syncer::BaseNode& sync_node);
  // Remove the association that corresponds to the given sync id.
  void Disassociate(int64_t sync_id);

  void AbortAssociation() override {
    // No implementation needed, this associator runs on the main
    // thread.
  }

  // See ModelAssociator interface.
  bool CryptoReadyIfNecessary() override;

 private:
  typedef std::map<int64_t, int64_t> NotesIdToSyncIdMap;
  typedef std::map<int64_t, const Notes_Node*> SyncIdToNotesNodeMap;
  typedef std::set<int64_t> DirtyAssociationsSyncIds;
  typedef std::vector<const Notes_Node*> NotesList;
  typedef std::stack<const Notes_Node*> NotesStack;

  // Posts a task to persist dirty associations.
  void PostPersistAssociationsTask();
  // Persists all dirty associations.
  void PersistAssociations();

  // Matches up the notes model and the sync model to build model
  // associations.
  syncer::SyncError BuildAssociations(
      syncer::SyncMergeResult* local_merge_result,
      syncer::SyncMergeResult* syncer_merge_result);

  // Result of the native model version check against the sync
  // version performed by CheckModelSyncState.
  enum NativeModelSyncState {
    // The native version is syncer::syncable::kInvalidTransactionVersion,
    // which is the case when the version has either not been set yet or reset
    // as a result of a previous error during the association. Basically the
    // state should return back to UNSET on an association following the one
    // where the state was different than IN_SYNC.
    UNSET,
    // The native version was in sync with the Sync version.
    IN_SYNC,
    // The native version was behing the sync version which indicates a failure
    // to persist the native notes model.
    BEHIND,
    // The native version was ahead of the sync version which indicates a
    // a failure to persist Sync DB.
    AHEAD,
    NATIVE_MODEL_SYNC_STATE_COUNT
  };

  // Helper class used within AssociateModels to simplify the logic and
  // minimize the number of arguments passed between private functions.
  class Context {
   public:
    Context(syncer::SyncMergeResult* local_merge_result,
            syncer::SyncMergeResult* syncer_merge_result);
    ~Context();

    // Push a sync node to the DFS stack.
    void PushNode(int64_t sync_id);
    // Pops a sync node from the DFS stack. Returns false if the stack
    // is empty.
    bool PopNode(int64_t* sync_id);

    // The following methods are used to update |local_merge_result_| and
    // |syncer_merge_result_|.
    void SetPreAssociationVersions(int64_t native_version,
                                   int64_t sync_version);
    void SetNumItemsBeforeAssociation(int local_num, int sync_num);
    void SetNumItemsAfterAssociation(int local_num, int sync_num);
    void IncrementLocalItemsDeleted();
    void IncrementLocalItemsAdded();
    void IncrementLocalItemsModified();
    void IncrementSyncItemsAdded();
    void IncrementSyncItemsDeleted(int count);

    void UpdateDuplicateCount(const base::string16& title,
                              const base::string16& content,
                              const GURL& url);

    int duplicate_count() const { return duplicate_count_; }

    NativeModelSyncState native_model_sync_state() const {
      return native_model_sync_state_;
    }
    void set_native_model_sync_state(NativeModelSyncState state) {
      native_model_sync_state_ = state;
    }

    // notes roots participating in the sync.
    void AddNotesRoot(const Notes_Node* root);
    const NotesList& notes_roots() const { return notes_roots_; }

    // Index of local notes nodes by native ID.
    int64_t GetSyncPreAssociationVersion() const;

    void MarkForVersionUpdate(const Notes_Node* node);
    const NotesList& notes_for_version_update() const {
      return notes_for_version_update_;
    }

   private:
    // DFS stack of sync nodes traversed during association.
    std::stack<int64_t> dfs_stack_;
    // Local and merge results are not owned.
    syncer::SyncMergeResult* local_merge_result_;
    syncer::SyncMergeResult* syncer_merge_result_;
    // |hashes_| contains hash codes of all native notes
    // for the purpose of detecting duplicates. A small number of
    // false positives due to hash collisions is OK because this
    // data is used for reporting purposes only.
    base::hash_set<size_t> hashes_;
    // Overall number of notes collisions from RecordDuplicates call.
    int duplicate_count_;
    // Result of the most recent NotesModelAssociator::CheckModelSyncState.
    NativeModelSyncState native_model_sync_state_;
    // List of notes model roots participating in the sync.
    NotesList notes_roots_;
    // List of notes nodes for which the transaction version needs to be
    // updated.
    NotesList notes_for_version_update_;

    DISALLOW_COPY_AND_ASSIGN(Context);
  };

  // Matches up the notes model and the sync model to build model
  // associations.
  syncer::SyncError BuildAssociations(Context* context);

  // Two helper functions that populate SyncMergeResult with numbers of
  // items before/after the association.
  void SetNumItemsBeforeAssociation(syncer::BaseTransaction* trans,
                                    Context* context);
  void SetNumItemsAfterAssociation(syncer::BaseTransaction* trans,
                                   Context* context);

  // Used by SetNumItemsBeforeAssociation.
  // Similar to NotesNode::GetTotalNodeCount but also scans the native
  // model for duplicates and records them in |context|.
  int GetTotalNotesCountAndRecordDuplicates(const Notes_Node* node,
                                            Context* context) const;

  // Helper function that associates all tagged permanent folders and primes
  // the provided context with sync IDs of those folders.
  syncer::SyncError AssociatePermanentFolders(syncer::BaseTransaction* trans,
                                              Context* context);
  bool AssociatePermanentFolders(syncer::BaseTransaction* trans,
                                 Context* context,
                                 Notes_Node* node,
                                 const char* tag);

  // Associate a top-level node of the notes model with a permanent node in
  // the sync domain.  Such permanent nodes are identified by a tag that is
  // well known to the server and the client, and is unique within a particular
  // user's share. The sync nodes are server-created.
  // Returns true on success, false if association failed.
  bool AssociateTaggedPermanentNode(syncer::BaseTransaction* trans,
                                    const Notes_Node* permanent_node,
                                    const std::string& tag) WARN_UNUSED_RESULT;

  // Removes notes nodes whose corresponding sync nodes have been deleted
  // according to sync delete journals.
  void ApplyDeletesFromSyncJournal(syncer::BaseTransaction* trans,
                                   Context* context);

  // The main part of the association process that associatiates
  // native nodes that are children of |parent_node| with sync nodes with IDs
  // from |sync_ids|.
  syncer::SyncError BuildAssociations(syncer::WriteTransaction* trans,
                                      const Notes_Node* parent_node,
                                      const std::vector<int64_t>& sync_ids,
                                      Context* context);

  // Helper method for creating a new native notes node.
  const Notes_Node* CreateNotesNode(const Notes_Node* parent_node,
                                    int note_index,
                                    const syncer::BaseNode* sync_child_node,
                                    const GURL& url,
                                    Context* context,
                                    syncer::SyncError* error);

  // Helper method for deleting a sync node and all its children.
  // Returns the number of sync nodes deleted.
  int RemoveSyncNodeHierarchy(syncer::WriteTransaction* trans, int64_t sync_id);

  // Check whether notes model and sync model are synced by comparing
  // their transaction versions.
  // Returns a PERSISTENCE_ERROR if a transaction mismatch was detected where
  // the native model has a newer transaction verison.
  syncer::SyncError CheckModelSyncState(Context* context) const;

  base::ThreadChecker thread_checker_;
  Notes_Model* notes_model_;
  syncer::SyncClient* sync_client_;
  syncer::UserShare* user_share_;
  std::unique_ptr<syncer::DataTypeErrorHandler> unrecoverable_error_handler_;
  NotesIdToSyncIdMap id_map_;
  SyncIdToNotesNodeMap id_map_inverse_;
  // Stores sync ids for dirty associations.
  DirtyAssociationsSyncIds dirty_associations_sync_ids_;

  // Used to post PersistAssociation tasks to the current message loop and
  // guarantees no invocations can occur if |this| has been deleted. (This
  // allows this class to be non-refcounted).
  base::WeakPtrFactory<NotesModelAssociator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NotesModelAssociator);
};

}  // namespace vivaldi

#endif  // SYNC_GLUE_NOTES_MODEL_ASSOCIATOR_H_
