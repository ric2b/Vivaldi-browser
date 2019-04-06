// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved
// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/glue/notes_change_processor.h"

#include <map>
#include <memory>
#include <stack>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/syncable/change_record.h"
#include "components/sync/syncable/entry.h"
#include "components/sync/syncable/read_node.h"
#include "components/sync/syncable/syncable_write_transaction.h"
#include "components/sync/syncable/write_node.h"
#include "components/sync/syncable/write_transaction.h"
#include "content/public/browser/browser_thread.h"
#include "notes/notes_factory.h"
#include "notes/notes_model.h"
#include "notes/notesnode.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_util.h"

using content::BrowserThread;
using syncer::ChangeRecord;
using syncer::ChangeRecordList;
using vivaldi::NotesModelFactory;
using vivaldi::Notes_Model;
using vivaldi::Notes_Node;

namespace vivaldi {

NotesChangeProcessor::NotesChangeProcessor(
    syncer::SyncClient* sync_client,
    NotesModelAssociator* model_associator,
    std::unique_ptr<syncer::DataTypeErrorHandler> err_handler)
    : syncer::ChangeProcessor(std::move(err_handler)),
      notes_model_(NULL),
      sync_client_(sync_client),
      model_associator_(model_associator) {
  DCHECK(model_associator);
  DCHECK(sync_client_);
  DCHECK(error_handler());
}

NotesChangeProcessor::~NotesChangeProcessor() {
  if (notes_model_)
    notes_model_->RemoveObserver(this);
}

void NotesChangeProcessor::StartImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!notes_model_);
  notes_model_ = sync_client_->GetNotesModel();
  DCHECK(notes_model_->loaded());
  notes_model_->AddObserver(this);
}

void NotesChangeProcessor::UpdateSyncNodeProperties(
    const Notes_Node* src,
    Notes_Model* model,
    syncer::WriteNode* dst,
    syncer::DataTypeErrorHandler* error_handler) {
  // Set the properties of the item.
  dst->SetIsFolder(src->is_folder());
  dst->SetTitle(base::UTF16ToUTF8(src->GetTitle()));
  sync_pb::NotesSpecifics notes_specifics(dst->GetNotesSpecifics());
  if (!src->is_folder()) {
    if (!src->GetURL().is_empty() && !src->GetURL().is_valid()) {
      // Report the invalid URL and continue.
      // TODO(stanisc): crbug/482155: Revisit this once the root cause for
      // invalid URLs is understood.
      error_handler->CreateAndUploadError(
          FROM_HERE,
          "Creating sync note with invalid url " +
              src->GetURL().possibly_invalid_spec(),
          syncer::NOTES);
    }
    notes_specifics.set_url(src->GetURL().spec());
    notes_specifics.set_content(base::UTF16ToUTF8(src->GetContent()));
    notes_specifics.clear_attachments();
    for (const auto& item : src->GetAttachments()) {
      auto* attachment = notes_specifics.add_attachments();
      attachment->set_checksum(item.second.checksum());
    }
  }
  if (src->is_trash())
    notes_specifics.set_special_node_type(sync_pb::NotesSpecifics::TRASH_NODE);
  if (src->is_separator())
    notes_specifics.set_special_node_type(sync_pb::NotesSpecifics::SEPARATOR);
  notes_specifics.set_creation_time_us(
      src->GetCreationTime().ToInternalValue());
  notes_specifics.set_content(base::UTF16ToUTF8(src->GetContent()));
  // notes_specifics.
  dst->SetNotesSpecifics(notes_specifics);
}

// static
int NotesChangeProcessor::RemoveSyncNodeHierarchy(
    syncer::WriteTransaction* trans,
    syncer::WriteNode* sync_node,
    NotesModelAssociator* associator) {
  // Remove children.
  int num_removed = RemoveAllChildNodes(trans, sync_node->GetId(), associator);
  // Remove the node itself.
  RemoveOneSyncNode(sync_node, associator);
  return num_removed + 1;
}

void NotesChangeProcessor::RemoveSyncNodeHierarchy(const Notes_Node* topmost) {
  int64_t new_version = syncer::syncable::kInvalidTransactionVersion;
  {
    syncer::WriteTransaction trans(FROM_HERE, share_handle(), &new_version);
    syncer::WriteNode topmost_sync_node(&trans);
    if (!model_associator_->InitSyncNodeFromChromeId(topmost->id(),
                                                     &topmost_sync_node)) {
      syncer::SyncError error(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                              "Failed to init sync node from chrome node",
                              syncer::NOTES);
      error_handler()->OnUnrecoverableError(error);
      return;
    }
    RemoveSyncNodeHierarchy(&trans, &topmost_sync_node, model_associator_);
  }

  // Don't need to update versions of deleted nodes.
  UpdateTransactionVersion(new_version, notes_model_,
                           std::vector<const Notes_Node*>());
}

void NotesChangeProcessor::RemoveAllSyncNodes() {
  int64_t new_version = syncer::syncable::kInvalidTransactionVersion;
  {
    syncer::WriteTransaction trans(FROM_HERE, share_handle(), &new_version);

    int64_t main_notes_node_id = model_associator_->GetSyncIdFromChromeId(
        notes_model_->main_node()->id());
    DCHECK_NE(syncer::kInvalidId, main_notes_node_id);
    RemoveAllChildNodes(&trans, main_notes_node_id, model_associator_);
    // Note: the root node may have additional extra nodes. Currently none of
    // them are meant to sync.
  }

  // Don't need to update versions of deleted nodes.
  UpdateTransactionVersion(new_version, notes_model_,
                           std::vector<const Notes_Node*>());
}

// static
int NotesChangeProcessor::RemoveAllChildNodes(
    syncer::WriteTransaction* trans,
    int64_t topmost_sync_id,
    NotesModelAssociator* associator) {
  // Do a DFS and delete all the child sync nodes, use sync id instead of
  // notes node ids since the notes nodes may already be deleted.
  // The equivalent recursive version of this iterative DFS:
  // remove_all_children(node_id, topmost_node_id):
  //    node.initByIdLookup(node_id)
  //    while(node.GetFirstChildId() != syncer::kInvalidId)
  //      remove_all_children(node.GetFirstChildId(), topmost_node_id)
  //    if(node_id != topmost_node_id)
  //      delete node

  int num_removed = 0;
  std::stack<int64_t> dfs_sync_id_stack;
  // Push the topmost node.
  dfs_sync_id_stack.push(topmost_sync_id);
  while (!dfs_sync_id_stack.empty()) {
    const int64_t sync_node_id = dfs_sync_id_stack.top();
    syncer::WriteNode node(trans);
    node.InitByIdLookup(sync_node_id);
    if (!node.GetIsFolder() || node.GetFirstChildId() == syncer::kInvalidId) {
      // All children of the node has been processed, delete the node and
      // pop it off the stack.
      dfs_sync_id_stack.pop();
      // Do not delete the topmost node.
      if (sync_node_id != topmost_sync_id) {
        RemoveOneSyncNode(&node, associator);
        num_removed++;
      } else {
        // if we are processing topmost node, all other nodes must be processed
        // the stack should be empty.
        DCHECK(dfs_sync_id_stack.empty());
      }
    } else {
      int64_t child_id = node.GetFirstChildId();
      if (child_id != syncer::kInvalidId) {
        dfs_sync_id_stack.push(child_id);
      }
    }
  }
  return num_removed;
}

// static
void NotesChangeProcessor::RemoveOneSyncNode(syncer::WriteNode* sync_node,
                                             NotesModelAssociator* associator) {
  // This node should have no children.
  DCHECK(!sync_node->HasChildren());
  // Remove association and delete the sync node.
  associator->Disassociate(sync_node->GetId());
  sync_node->Tombstone();
}

void NotesChangeProcessor::CreateOrUpdateSyncNode(const Notes_Node* node) {
  if (!CanSyncNode(node)) {
    NOTREACHED();
    return;
  }

  int64_t new_version = syncer::syncable::kInvalidTransactionVersion;
  int64_t sync_id = syncer::kInvalidId;
  {
    // Acquire a scoped write lock via a transaction.
    syncer::WriteTransaction trans(FROM_HERE, share_handle(), &new_version);
    sync_id = model_associator_->GetSyncIdFromChromeId(node->id());
    if (sync_id != syncer::kInvalidId) {
      UpdateSyncNode(node, notes_model_, &trans, model_associator_,
                     error_handler());
    } else {
      const Notes_Node* parent = node->parent();
      int index = parent->GetIndexOf(node);
      sync_id = CreateSyncNode(parent, notes_model_, index, &trans,
                               model_associator_, error_handler());
    }
  }

  if (syncer::kInvalidId != sync_id) {
    // Siblings of added node in sync DB will also be updated to reflect new
    // PREV_ID/NEXT_ID and thus get a new version. But we only update version
    // of added node here. After switching to ordinals for positioning,
    // PREV_ID/NEXT_ID will be deprecated and siblings will not be updated.
    UpdateTransactionVersion(new_version, notes_model_,
                             std::vector<const Notes_Node*>(1, node));
  }
}

void NotesChangeProcessor::NotesModelLoaded(Notes_Model* model,
                                            bool ids_reassigned) {
  NOTREACHED();
}

void NotesChangeProcessor::NotesModelBeingDeleted(Notes_Model* model) {
  NOTREACHED();
  notes_model_ = nullptr;
}

void NotesChangeProcessor::NotesNodeAdded(Notes_Model* model,
                                          const Notes_Node* parent,
                                          int index) {
  DCHECK(share_handle());
  const Notes_Node* node = parent->GetChild(index);
  CreateOrUpdateSyncNode(node);
}

// static
int64_t NotesChangeProcessor::CreateSyncNode(
    const Notes_Node* parent,
    Notes_Model* model,
    int index,
    syncer::WriteTransaction* trans,
    NotesModelAssociator* associator,
    syncer::DataTypeErrorHandler* error_handler) {
  const Notes_Node* child = parent->GetChild(index);
  DCHECK(child);

  // Create a WriteNode container to hold the new node.
  syncer::WriteNode sync_child(trans);

  // Actually create the node with the appropriate initial position.
  if (!PlaceSyncNode(CREATE, parent, index, trans, &sync_child, associator)) {
    syncer::SyncError error(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                            "Failed to create sync node.", syncer::NOTES);
    error_handler->OnUnrecoverableError(error);
    return syncer::kInvalidId;
  }

  UpdateSyncNodeProperties(child, model, &sync_child, error_handler);

  // Associate the ID from the sync domain with the notes node, so that we
  // can refer back to this item later.
  associator->Associate(child, sync_child);

  return sync_child.GetId();
}

void NotesChangeProcessor::OnWillRemoveNotes(Notes_Model* model,
                                             const Notes_Node* parent,
                                             int old_index,
                                             const Notes_Node* node) {
  if (CanSyncNode(node))
    RemoveSyncNodeHierarchy(node);
}

void NotesChangeProcessor::NotesNodeRemoved(Notes_Model* model,
                                            const Notes_Node* parent,
                                            int index,
                                            const Notes_Node* node) {
  // All the work should have already been done in OnWillRemoveNotes.
  DCHECK_EQ(syncer::kInvalidId,
            model_associator_->GetSyncIdFromChromeId(node->id()));
}

void NotesChangeProcessor::NotesAllNodesRemoved(Notes_Model* model) {
  RemoveAllSyncNodes();
}

void NotesChangeProcessor::NotesNodeChanged(Notes_Model* model,
                                            const Notes_Node* node) {
  if (!CanSyncNode(node))
    return;
  // We shouldn't see changes to the top-level nodes.
  if (model->is_permanent_node(node)) {
    NOTREACHED() << "Saw update to permanent node!";
    return;
  }
  CreateOrUpdateSyncNode(node);
}

// Static.
int64_t NotesChangeProcessor::UpdateSyncNode(
    const Notes_Node* node,
    Notes_Model* model,
    syncer::WriteTransaction* trans,
    NotesModelAssociator* associator,
    syncer::DataTypeErrorHandler* error_handler) {
  // Lookup the sync node that's associated with |node|.
  syncer::WriteNode sync_node(trans);
  if (!associator->InitSyncNodeFromChromeId(node->id(), &sync_node)) {
    syncer::SyncError error(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                            "Failed to init sync node from chrome node",
                            syncer::NOTES);
    error_handler->OnUnrecoverableError(error);
    return syncer::kInvalidId;
  }
  UpdateSyncNodeProperties(node, model, &sync_node, error_handler);
  DCHECK_EQ(sync_node.GetIsFolder(), node->is_folder());
  DCHECK_EQ(associator->GetChromeNodeFromSyncId(sync_node.GetParentId()),
            node->parent());
  DCHECK_EQ(node->parent()->GetIndexOf(node), sync_node.GetPositionIndex());
  return sync_node.GetId();
}

void NotesChangeProcessor::NotesNodeMoved(Notes_Model* model,
                                          const Notes_Node* old_parent,
                                          int old_index,
                                          const Notes_Node* new_parent,
                                          int new_index) {
  const Notes_Node* child = new_parent->GetChild(new_index);

  if (!CanSyncNode(child))
    return;

  // We shouldn't see changes to the top-level nodes.
  if (model->is_permanent_node(child)) {
    NOTREACHED() << "Saw update to permanent node!";
    return;
  }

  int64_t new_version = syncer::syncable::kInvalidTransactionVersion;
  {
    // Acquire a scoped write lock via a transaction.
    syncer::WriteTransaction trans(FROM_HERE, share_handle(), &new_version);

    // Lookup the sync node that's associated with |child|.
    syncer::WriteNode sync_node(&trans);
    if (!model_associator_->InitSyncNodeFromChromeId(child->id(), &sync_node)) {
      syncer::SyncError error(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                              "Failed to init sync node from chrome node",
                              syncer::NOTES);
      error_handler()->OnUnrecoverableError(error);
      return;
    }

    if (!PlaceSyncNode(MOVE, new_parent, new_index, &trans, &sync_node,
                       model_associator_)) {
      syncer::SyncError error(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                              "Failed to place sync node", syncer::NOTES);
      error_handler()->OnUnrecoverableError(error);
      return;
    }
  }

  UpdateTransactionVersion(new_version, model,
                           std::vector<const Notes_Node*>(1, child));
}

void NotesChangeProcessor::NotesNodeAttachmentChanged(Notes_Model* model,
                                                      const Notes_Node* node) {
  NotesNodeChanged(model, node);
}

void NotesChangeProcessor::NotesNodeChildrenReordered(Notes_Model* model,
                                                      const Notes_Node* node) {
  if (!CanSyncNode(node))
    return;
  int64_t new_version = syncer::syncable::kInvalidTransactionVersion;
  std::vector<const Notes_Node*> children;
  {
    // Acquire a scoped write lock via a transaction.
    syncer::WriteTransaction trans(FROM_HERE, share_handle(), &new_version);

    // The given node's children got reordered. We need to reorder all the
    // children of the corresponding sync node.
    for (int i = 0; i < node->child_count(); ++i) {
      const Notes_Node* child = node->GetChild(i);
      children.push_back(child);

      syncer::WriteNode sync_child(&trans);
      if (!model_associator_->InitSyncNodeFromChromeId(child->id(),
                                                       &sync_child)) {
        syncer::SyncError error(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                                "Failed to init sync node from chrome node",
                                syncer::NOTES);
        error_handler()->OnUnrecoverableError(error);
        return;
      }
      DCHECK_EQ(sync_child.GetParentId(),
                model_associator_->GetSyncIdFromChromeId(node->id()));

      if (!PlaceSyncNode(MOVE, node, i, &trans, &sync_child,
                         model_associator_)) {
        syncer::SyncError error(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                                "Failed to place sync node", syncer::NOTES);
        error_handler()->OnUnrecoverableError(error);
        return;
      }
    }
  }

  // TODO(haitaol): Filter out children that didn't actually change.
  UpdateTransactionVersion(new_version, model, children);
}

// static
bool NotesChangeProcessor::PlaceSyncNode(MoveOrCreate operation,
                                         const Notes_Node* parent,
                                         int index,
                                         syncer::WriteTransaction* trans,
                                         syncer::WriteNode* dst,
                                         NotesModelAssociator* associator) {
  syncer::ReadNode sync_parent(trans);
  if (!associator->InitSyncNodeFromChromeId(parent->id(), &sync_parent)) {
    LOG(WARNING) << "Parent lookup failed";
    return false;
  }

  bool success = false;
  if (index == 0) {
    // Insert into first position.
    success = (operation == CREATE)
                  ? dst->InitNotesByCreation(sync_parent, NULL)
                  : dst->SetPosition(sync_parent, NULL);
    if (success) {
      DCHECK_EQ(dst->GetParentId(), sync_parent.GetId());
      DCHECK_EQ(dst->GetId(), sync_parent.GetFirstChildId());
      DCHECK_EQ(dst->GetPredecessorId(), syncer::kInvalidId);
    }
  } else {
    // Find the notes model predecessor, and insert after it.
    const Notes_Node* prev = parent->GetChild(index - 1);
    syncer::ReadNode sync_prev(trans);
    if (!associator->InitSyncNodeFromChromeId(prev->id(), &sync_prev)) {
      LOG(WARNING) << "Predecessor lookup failed";
      return false;
    }
    success = (operation == CREATE)
                  ? dst->InitNotesByCreation(sync_parent, &sync_prev)
                  : dst->SetPosition(sync_parent, &sync_prev);
    if (success) {
      DCHECK_EQ(dst->GetParentId(), sync_parent.GetId());
      DCHECK_EQ(dst->GetPredecessorId(), sync_prev.GetId());
      DCHECK_EQ(dst->GetId(), sync_prev.GetSuccessorId());
    }
  }
  return success;
}

// ApplyModelChanges is called by the sync backend after changes have been made
// to the sync engine's model.  Apply these changes to the browser notes
// model.
void NotesChangeProcessor::ApplyChangesFromSyncModel(
    const syncer::BaseTransaction* trans,
    int64_t model_version,
    const syncer::ImmutableChangeRecordList& changes) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // A note about ordering.  Sync backend is responsible for ordering the change
  // records in the following order:
  //
  // 1. Deletions, from leaves up to parents.
  // 2. Existing items with synced parents & predecessors.
  // 3. New items with synced parents & predecessors.
  // 4. Items with parents & predecessors in the list.
  // 5. Repeat #4 until all items are in the list.
  //
  // "Predecessor" here means the previous item within a given folder; an item
  // in the first position is always said to have a synced predecessor.
  // For the most part, applying these changes in the order given will yield
  // the correct result.  There is one exception, however: for items that are
  // moved away from a folder that is being deleted, we will process the delete
  // before the move.  Since deletions in the notes model propagate from
  // parent to child, we must move them to a temporary location.
  Notes_Model* model = notes_model_;

  // We are going to make changes to the notes model, but don't want to end
  // up in a feedback loop, so remove ourselves as an observer while applying
  // changes.
  model->RemoveObserver(this);

  // Notify UI intensive observers of Notes_Model that we are about to make
  // potentially significant changes to it, so the updates may be batched. For
  // example, on Mac, the notes bar displays animations when notes items
  // are added or deleted.
  model->BeginExtensiveChanges();

  // A parent to hold nodes temporarily orphaned by parent deletion.  It is
  // created only if it is needed.
  const Notes_Node* foster_parent = NULL;

  // Iterate over the deletions, which are always at the front of the list.
  ChangeRecordList::const_iterator it;
  for (it = changes.Get().begin();
       it != changes.Get().end() && it->action == ChangeRecord::ACTION_DELETE;
       ++it) {
    const Notes_Node* dst = model_associator_->GetChromeNodeFromSyncId(it->id);

    // Ignore changes to the permanent top-level nodes.  We only care about
    // their children.
    if (model->is_permanent_node(dst))
      continue;

    // Can't do anything if we can't find the chrome node.
    if (!dst)
      continue;

    // Children of a deleted node should not be deleted; they may be
    // reparented by a later change record.  Move them to a temporary place.
    if (!dst->empty()) {
      if (!foster_parent) {
        foster_parent = model->AddFolder(model->other_node(),
                                         model->other_node()->child_count(),
                                         base::string16());
        if (!foster_parent) {
          syncer::SyncError error(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                                  "Failed to create foster parent",
                                  syncer::NOTES);
          error_handler()->OnUnrecoverableError(error);
          return;
        }
      }
      for (int i = dst->child_count() - 1; i >= 0; --i) {
        model->Move(dst->GetChild(i), foster_parent,
                    foster_parent->child_count());
      }
    }
    DCHECK_EQ(dst->child_count(), 0) << "Node being deleted has children";

    model_associator_->Disassociate(it->id);

    const Notes_Node* parent = dst->parent();
    int index = parent->GetIndexOf(dst);
    if (index > -1)
      model->Remove(parent->GetChild(index));
  }

  // A map to keep track of some reordering work we defer until later.
  std::multimap<int, const Notes_Node*> to_reposition;

  syncer::ReadNode synced_notes(trans);

  // Continue iterating where the previous loop left off.
  for (; it != changes.Get().end(); ++it) {
    const Notes_Node* dst = model_associator_->GetChromeNodeFromSyncId(it->id);

    // Ignore changes to the permanent top-level nodes.  We only care about
    // their children.
    if (model->is_permanent_node(dst))
      continue;

    DCHECK_NE(it->action, ChangeRecord::ACTION_DELETE)
        << "We should have passed all deletes by this point.";

    syncer::ReadNode src(trans);
    if (src.InitByIdLookup(it->id) != syncer::BaseNode::INIT_OK) {
      syncer::SyncError error(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                              "Failed to load sync node", syncer::NOTES);
      error_handler()->OnUnrecoverableError(error);
      return;
    }

    const Notes_Node* parent =
        model_associator_->GetChromeNodeFromSyncId(src.GetParentId());
    if (!parent) {
      LOG(ERROR) << "Could not find parent of node being added/updated."
                 << " Node title: " << src.GetTitle()
                 << ", parent id = " << src.GetParentId();
      continue;
    }

    if (dst) {
      DCHECK(it->action == ChangeRecord::ACTION_UPDATE)
          << "ACTION_UPDATE should be seen if and only if the node is known.";
      UpdateNoteWithSyncData(src, model, dst, sync_client_);

      // Move all modified entries to the right.  We'll fix it later.
      model->Move(dst, parent, parent->child_count());
    } else {
      DCHECK(it->action == ChangeRecord::ACTION_ADD)
          << "ACTION_ADD should be seen if and only if the node is unknown.";

      dst = CreateNotesEntry(&src, parent, model, sync_client_,
                             parent->child_count());
      if (!dst) {
        // We ignore notes we can't add. Chances are this is caused by
        // a note that was not fully associated.
        LOG(ERROR) << "Failed to create note node with title "
                   << src.GetTitle() + " and url "
                   << src.GetNotesSpecifics().url();
        continue;
      }
      model_associator_->Associate(dst, src);
    }

    to_reposition.insert(std::make_pair(src.GetPositionIndex(), dst));
    notes_model_->SetNodeSyncTransactionVersion(dst, model_version);
  }

  // When we added or updated notes in the previous loop, we placed them to
  // the far right position.  Now we iterate over all these modified items in
  // sync order, left to right, moving them into their proper positions.
  for (auto it : to_reposition) {
    const Notes_Node* parent = it.second->parent();
    model->Move(it.second, parent, it.first);
  }

  // Clean up the temporary node.
  if (foster_parent) {
    // There should be no nodes left under the foster parent.
    DCHECK_EQ(foster_parent->child_count(), 0);
    model->Remove(foster_parent);
    foster_parent = NULL;
  }

  // Notify UI intensive observers of Notes_Model that all updates have been
  // applied, and that they may now be consumed. This prevents issues like the
  // one described in crbug.com/281562, where old and new items on the notes
  // bar would overlap.
  model->EndExtensiveChanges();

  // We are now ready to hear about notes changes again.
  model->AddObserver(this);

  // All changes are applied in notes model. Set transaction version on
  // notes model to mark as synced.
  model->SetNodeSyncTransactionVersion(model->root_node(), model_version);
}

// Static.
// Update a notes node with specified sync data.
void NotesChangeProcessor::UpdateNoteWithSyncData(
    const syncer::BaseNode& sync_node,
    Notes_Model* model,
    const Notes_Node* node,
    syncer::SyncClient* sync_client) {
  DCHECK_EQ(sync_node.GetIsFolder(), node->is_folder());
  const sync_pb::NotesSpecifics& specifics = sync_node.GetNotesSpecifics();
  if (!sync_node.GetIsFolder())
    model->SetURL(node, GURL(specifics.url()));
  model->SetTitle(node, base::UTF8ToUTF16(sync_node.GetTitle()));
  model->SetContent(node, base::UTF8ToUTF16(specifics.content()));
  if (specifics.has_creation_time_us()) {
    model->SetDateAdded(
        node, base::Time::FromInternalValue(specifics.creation_time_us()));
  }
  UpdateNoteWithAttachmentData(specifics, node);
}

// Static.
// Update a notes node with specified sync data.
void NotesChangeProcessor::UpdateNoteWithAttachmentData(
    const sync_pb::NotesSpecifics& specifics,
    const Notes_Node* node) {
  Notes_Node* mutable_node = AsMutable(node);
  if (!node->is_folder() && specifics.attachments_size()) {
    NoteAttachments new_attachments;
    for (auto it : specifics.attachments()) {
      if (!it.has_checksum())
        continue;
      mutable_node->AddAttachment(NoteAttachment(it.checksum(), ""));
    }
  }
  if (specifics.has_special_node_type()) {
    switch (specifics.special_node_type()) {
      case sync_pb::NotesSpecifics::TRASH_NODE:
        mutable_node->SetType(Notes_Node::TRASH);
        break;
      case sync_pb::NotesSpecifics::SEPARATOR:
        mutable_node->SetType(Notes_Node::SEPARATOR);
        break;
      default:
        break;
    }
  }
}

// static
void NotesChangeProcessor::UpdateTransactionVersion(
    int64_t new_version,
    Notes_Model* model,
    const std::vector<const Notes_Node*>& nodes) {
  if (new_version != syncer::syncable::kInvalidTransactionVersion) {
    model->SetNodeSyncTransactionVersion(model->root_node(), new_version);
    for (size_t i = 0; i < nodes.size(); ++i) {
      model->SetNodeSyncTransactionVersion(nodes[i], new_version);
    }
  }
}

// static
// Creates a notes node under the given parent node from the given sync
// node. Returns the newly created node.
const Notes_Node* NotesChangeProcessor::CreateNotesEntry(
    syncer::BaseNode* sync_node,
    const Notes_Node* parent,
    Notes_Model* model,
    syncer::SyncClient* sync_client,
    int index) {
  return CreateNotesEntry(base::UTF8ToUTF16(sync_node->GetTitle()),
                          GURL(sync_node->GetNotesSpecifics().url()), sync_node,
                          parent, model, sync_client, index);
}

// static
// Creates a notes node under the given parent node from the given sync
// node. Returns the newly created node.
const Notes_Node* NotesChangeProcessor::CreateNotesEntry(
    const base::string16& title,
    const GURL& url,
    const syncer::BaseNode* sync_node,
    const Notes_Node* parent,
    Notes_Model* model,
    syncer::SyncClient* sync_client,
    int index) {
  DCHECK(parent);

  const Notes_Node* node;
  const sync_pb::NotesSpecifics& specifics = sync_node->GetNotesSpecifics();
  if (sync_node->GetIsFolder()) {
    node = model->AddFolder(parent, index, title);
  } else {
    node = model->AddNote(parent, index, title, GURL(specifics.url()),
                          base::UTF8ToUTF16(specifics.content()));
  }
  UpdateNoteWithAttachmentData(specifics, node);
  return node;
}

bool NotesChangeProcessor::CanSyncNode(const Notes_Node* node) {
  return true;
}

}  // namespace vivaldi
