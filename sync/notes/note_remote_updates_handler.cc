// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notes/note_remote_updates_handler.h"

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "base/guid.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "base/trace_event/trace_event.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/protocol/unique_position.pb.h"
#include "notes/note_node.h"
#include "notes/notes_model.h"
#include "sync/notes/note_specifics_conversions.h"
#include "components/sync_bookmarks/switches.h"

namespace sync_notes {

namespace {

// Recursive method to traverse a forest created by ReorderUpdates() to
// emit updates in top-down order. |ordered_updates| must not be null because
// traversed updates are appended to |*ordered_updates|.
void TraverseAndAppendChildren(
    const base::StringPiece& node_id,
    const std::unordered_map<base::StringPiece,
                             const syncer::UpdateResponseData*,
                             base::StringPieceHash>& id_to_updates,
    const std::unordered_map<base::StringPiece,
                             std::vector<base::StringPiece>,
                             base::StringPieceHash>& node_to_children,
    std::vector<const syncer::UpdateResponseData*>* ordered_updates) {
  // If no children to traverse, we are done.
  if (node_to_children.count(node_id) == 0) {
    return;
  }
  // Recurse over all children.
  for (const base::StringPiece& child : node_to_children.at(node_id)) {
    DCHECK_NE(id_to_updates.count(child), 0U);
    ordered_updates->push_back(id_to_updates.at(child));
    TraverseAndAppendChildren(child, id_to_updates, node_to_children,
                              ordered_updates);
  }
}

size_t ComputeChildNodeIndex(const vivaldi::NoteNode* parent,
                             const sync_pb::UniquePosition& unique_position,
                             const SyncedNoteTracker* note_tracker) {
  const syncer::UniquePosition position =
      syncer::UniquePosition::FromProto(unique_position);
  for (size_t i = 0; i < parent->children().size(); ++i) {
    const vivaldi::NoteNode* child = parent->children()[i].get();
    const SyncedNoteTracker::Entity* child_entity =
        note_tracker->GetEntityForNoteNode(child);
    DCHECK(child_entity);
    const syncer::UniquePosition child_position =
        syncer::UniquePosition::FromProto(
            child_entity->metadata()->unique_position());
    if (position.LessThan(child_position)) {
      return i;
    }
  }
  return parent->children().size();
}

void ApplyRemoteUpdate(
    const syncer::UpdateResponseData& update,
    const SyncedNoteTracker::Entity* tracked_entity,
    const SyncedNoteTracker::Entity* new_parent_tracked_entity,
    vivaldi::NotesModel* model,
    SyncedNoteTracker* tracker) {
  const syncer::EntityData& update_entity = update.entity;
  DCHECK(!update_entity.is_deleted());
  DCHECK(tracked_entity);
  DCHECK(new_parent_tracked_entity);
  DCHECK(model);
  DCHECK(tracker);
  const vivaldi::NoteNode* node = tracked_entity->note_node();
  const vivaldi::NoteNode* old_parent = node->parent();
  const vivaldi::NoteNode* new_parent = new_parent_tracked_entity->note_node();

  if (update_entity.is_folder != node->is_folder()) {
    DLOG(ERROR) << "Could not update node. Remote node is a "
                << (update_entity.is_folder ? "folder" : "note")
                << " while local node is a "
                << (node->is_folder() ? "folder" : "note");
    return;
  }

  if ((update_entity.specifics.notes().special_node_type() ==
       sync_pb::NotesSpecifics::SEPARATOR) != node->is_separator()) {
    DLOG(ERROR) << "Could not update node. Remote node is a "
                << (update_entity.specifics.notes().special_node_type() ==
                            sync_pb::NotesSpecifics::SEPARATOR
                        ? "separator"
                        : "regular note")
                << " while local node is a "
                << (node->is_separator() ? "separator" : "regular note");
    return;
  }

  // If there is a different GUID in the specifics and it is valid, we must
  // replace the entire node in order to use it, as GUIDs are immutable. Further
  // updates are then applied to the new node instead.
  if (update_entity.specifics.notes().guid() != node->guid() &&
      base::IsValidGUIDOutputString(update_entity.specifics.notes().guid())) {
    const vivaldi::NoteNode* old_node = node;
    node = ReplaceNoteNodeGUID(node, update_entity.specifics.notes().guid(),
                               model);
    tracker->UpdateNoteNodePointer(old_node, node);
  }

  UpdateNoteNodeFromSpecifics(update_entity.specifics.notes(), node, model);
  // Compute index information before updating the |tracker|.
  const size_t old_index = size_t{old_parent->GetIndexOf(node)};
  const size_t new_index =
      ComputeChildNodeIndex(new_parent, update_entity.unique_position, tracker);
  tracker->Update(tracked_entity, update.response_version,
                  update_entity.modification_time,
                  update_entity.unique_position, update_entity.specifics);

  if (new_parent == old_parent &&
      (new_index == old_index || new_index == old_index + 1)) {
    // Node hasn't moved. No more work to do.
    return;
  }
  // Node has moved to another position under the same parent. Update the model.
  // NotesModel takes care of placing the node in the correct position if the
  // node is move to the left. (i.e. no need to subtract one from |new_index|).
  model->Move(node, new_parent, new_index);
}

}  // namespace

NoteRemoteUpdatesHandler::NoteRemoteUpdatesHandler(
    vivaldi::NotesModel* notes_model,
    SyncedNoteTracker* note_tracker)
    : notes_model_(notes_model), note_tracker_(note_tracker) {
  DCHECK(notes_model);
  DCHECK(note_tracker);
}

void NoteRemoteUpdatesHandler::Process(
    const syncer::UpdateResponseDataList& updates,
    bool got_new_encryption_requirements) {
  note_tracker_->CheckAllNodesTracked(notes_model_);
  TRACE_EVENT0("sync", "NoteRemoteUpdatesHandler::Process");

  // If new encryption requirements come from the server, the entities that are
  // in |updates| will be recorded here so they can be ignored during the
  // re-encryption phase at the end.
  std::unordered_set<std::string> entities_with_up_to_date_encryption;

  for (const syncer::UpdateResponseData* update : ReorderUpdates(&updates)) {
    const syncer::EntityData& update_entity = update->entity;
    // Only non deletions and non premanent node should have valid specifics and
    // unique positions.
    if (!update_entity.is_deleted() &&
        update_entity.server_defined_unique_tag.empty()) {
      if (!IsValidNotesSpecifics(update_entity.specifics.notes(),
                                 update_entity.is_folder)) {
        // Ignore updates with invalid specifics.
        DLOG(ERROR)
            << "Couldn't process an update note with an invalid specifics.";
        continue;
      }
      if (!syncer::UniquePosition::FromProto(update_entity.unique_position)
               .IsValid()) {
        // Ignore updates with invalid unique position.
        DLOG(ERROR) << "Couldn't process an update note with an invalid "
                       "unique position.";
        continue;
      }
      if (!HasExpectedNoteGuid(update_entity.specifics.notes(),
                               update_entity.originator_cache_guid,
                               update_entity.originator_client_item_id)) {
        // Ignore updates with an unexpected GUID.
        DLOG(ERROR) << "Couldn't process an update note with unexpected GUID: "
                    << update_entity.specifics.notes().guid();
        continue;
      }
    }

    const SyncedNoteTracker::Entity* tracked_entity =
        note_tracker_->GetEntityForSyncId(update_entity.id);

    // This may be a good chance to populate the client tag for the first time.
    // TODO(crbug.com/1032052): Remove this code once all local sync metadata
    // is required to populate the client tag (and be considered invalid
    // otherwise).
    bool local_guid_needs_update = false;
    const std::string& remote_guid = update_entity.specifics.notes().guid();
    if (tracked_entity && !update_entity.is_deleted() &&
        update_entity.server_defined_unique_tag.empty() &&
        !tracked_entity->final_guid_matches(remote_guid)) {
      DCHECK(base::IsValidGUIDOutputString(remote_guid));
      note_tracker_->PopulateFinalGuid(tracked_entity, remote_guid);
      // In many cases final_guid_matches() may return false because a final
      // GUID is not known for sure, but actually it matches the local GUID.
      if (tracked_entity->note_node() &&
          remote_guid != tracked_entity->note_node()->guid()) {
        local_guid_needs_update = true;
      }
    }

    if (tracked_entity &&
        tracked_entity->metadata()->server_version() >=
            update->response_version &&
        !local_guid_needs_update) {
      // Seen this update before; just ignore it.
      continue;
    }

    // If a commit succeeds, but the response does not come back fast enough
    // (e.g. before shutdown or crash), then the |note_tracker| might
    // assume that it was never committed. The server will track the client that
    // sent up the original commit and return this in a get updates response. We
    // need to check if we have an entry that didn't get its server id updated
    // correctly. The server sends down a |originator_cache_guid| and an
    // |original_client_item_id|. If we have a entry by that description, we
    // should update the |sync_id| in |note_tracker|. The rest of code will
    // handle this a conflict and adjust the model if needed.
    const SyncedNoteTracker::Entity* old_tracked_entity =
        note_tracker_->GetEntityForSyncId(
            update_entity.originator_client_item_id);
    if (old_tracked_entity) {
      if (tracked_entity) {
        DCHECK_NE(tracked_entity, old_tracked_entity);
        // We generally shouldn't have an entry for both the old ID and the new
        // ID, but it could happen due to some past bug (see crbug.com/1004205).
        // In that case, the two entries should be duplicates in the sense that
        // they have the same content.
        // TODO(crbug.com/516866): Clean up the workaround once this has been
        // resolved.
        const vivaldi::NoteNode* old_node = old_tracked_entity->note_node();
        const vivaldi::NoteNode* new_node = tracked_entity->note_node();
        if (new_node == nullptr) {
          // This might happen in case a synced note (with a non-temporary
          // server ID but no known client tag) was deleted locally and then
          // recreated locally while the commit is in flight. This leads to two
          // entities in the tracker (for the same note GUID), one of them
          // being a tombstone (|tracked_entity|). The commit response (for the
          // deletion) may never be received (e.g. network issues) and instead a
          // remote update is received (possibly our own reflection). Resolving
          // a situation with duplicate entries is simple as long as at least
          // one of the two (it could also be both) is a tombstone: one of the
          // entities can be simply untracked.
          //
          // In the current case the |old_tracked_entity| must be a new entity
          // since it does not have server ID yet. The |new_node| must be always
          // a tombstone (the note which was deleted). Just remove the
          // tombstone and continue applying current update (even if |old_node|
          // is a tombstone too).
          note_tracker_->Remove(tracked_entity);
          tracked_entity = nullptr;
        } else {
          // |old_node| may be null when |old_entity| is a tombstone pending
          // commit.
          if (old_node != nullptr) {
            DCHECK_NE(old_node, new_node);
            notes_model_->Remove(old_node);
          }
          note_tracker_->Remove(old_tracked_entity);
          continue;
        }
      }

      note_tracker_->UpdateSyncIdForLocalCreationIfNeeded(
          old_tracked_entity,
          /*sync_id=*/update_entity.id);

      // The tracker has changed. Re-retrieve the |tracker_entity|.
      tracked_entity = note_tracker_->GetEntityForSyncId(update_entity.id);
    }

    if (tracked_entity && tracked_entity->IsUnsynced()) {
      ProcessConflict(*update, tracked_entity);
      if (!note_tracker_->GetEntityForSyncId(update_entity.id)) {
        // During conflict resolution, the entity could be dropped in case of
        // a conflict between local and remote deletions. We shouldn't worry
        // about changes to the encryption in that case.
        continue;
      }
    } else if (update_entity.is_deleted()) {
      ProcessDelete(update_entity, tracked_entity);
      // If the local entity has been deleted, no need to check for out of date
      // encryption. Therefore, we can go ahead and process the next update.
      continue;
    } else if (!tracked_entity) {
      tracked_entity = ProcessCreate(*update);
      if (!tracked_entity) {
        // If no new node has been tracked, we shouldn't worry about changes to
        // the encryption.
        continue;
      }
      // TODO(crbug.com/516866): The below CHECK is added to debug some crashes.
      // Should be removed after figuring out the reason for the crash.
      CHECK_EQ(tracked_entity,
               note_tracker_->GetEntityForSyncId(update_entity.id));
    } else {
      // Ignore changes to the permanent nodes (e.g. main notes). We only
      // care about their children.
      if (notes_model_->is_permanent_node(tracked_entity->note_node())) {
        continue;
      }
      ProcessUpdate(*update, tracked_entity);
      // TODO(crbug.com/516866): The below CHECK is added to debug some crashes.
      // Should be removed after figuring out the reason for the crash.
      CHECK_EQ(tracked_entity,
               note_tracker_->GetEntityForSyncId(update_entity.id));
    }
    // If the received entity has out of date encryption, we schedule another
    // commit to fix it.
    if (note_tracker_->model_type_state().encryption_key_name() !=
        update->encryption_key_name) {
      DVLOG(2) << "Notes: Requesting re-encrypt commit "
               << update->encryption_key_name << " -> "
               << note_tracker_->model_type_state().encryption_key_name();
      note_tracker_->IncrementSequenceNumber(tracked_entity);
    }

    if (got_new_encryption_requirements) {
      entities_with_up_to_date_encryption.insert(update_entity.id);
    }
  }

  // Recommit entities with out of date encryption.
  if (got_new_encryption_requirements) {
    std::vector<const SyncedNoteTracker::Entity*> all_entities =
        note_tracker_->GetAllEntities();
    for (const SyncedNoteTracker::Entity* entity : all_entities) {
      // No need to recommit tombstones and permanent nodes.
      if (entity->metadata()->is_deleted()) {
        continue;
      }
      DCHECK(entity->note_node());
      if (entity->note_node()->is_permanent_node()) {
        continue;
      }
      if (entities_with_up_to_date_encryption.count(
              entity->metadata()->server_id()) != 0) {
        continue;
      }
      note_tracker_->IncrementSequenceNumber(entity);
    }
  }
  note_tracker_->CheckAllNodesTracked(notes_model_);
}

// static
std::vector<const syncer::UpdateResponseData*>
NoteRemoteUpdatesHandler::ReorderUpdatesForTest(
    const syncer::UpdateResponseDataList* updates) {
  return ReorderUpdates(updates);
}

// static
std::vector<const syncer::UpdateResponseData*>
NoteRemoteUpdatesHandler::ReorderUpdates(
    const syncer::UpdateResponseDataList* updates) {
  // This method sorts the remote updates according to the following rules:
  // 1. Creations and updates come before deletions.
  // 2. Parent creation/update should come before child creation/update.
  // 3. No need to further order deletions. Parent deletions can happen before
  //    child deletions. This is safe because all updates (e.g. moves) should
  //    have been processed already.

  // The algorithm works by constructing a forest of all non-deletion updates
  // and then traverses each tree in the forest recursively: Forest
  // Construction:
  // 1. Iterate over all updates and construct the |parent_to_children| map and
  //    collect all parents in |roots|.
  // 2. Iterate over all updates again and drop any parent that has a
  //    coressponding update. What's left in |roots| are the roots of the
  //    forest.
  // 3. Start at each root in |roots|, emit the update and recurse over its
  //    children.

  std::unordered_map<base::StringPiece, const syncer::UpdateResponseData*,
                     base::StringPieceHash>
      id_to_updates;
  std::set<base::StringPiece> roots;
  std::unordered_map<base::StringPiece, std::vector<base::StringPiece>,
                     base::StringPieceHash>
      parent_to_children;

  // Add only non-deletions to |id_to_updates|.
  for (const syncer::UpdateResponseData& update : *updates) {
    const syncer::EntityData& update_entity = update.entity;
    // Ignore updates to root nodes.
    if (update_entity.parent_id == "0") {
      continue;
    }
    if (update_entity.is_deleted()) {
      continue;
    }
    id_to_updates[update_entity.id] = &update;
  }
  // Iterate over |id_to_updates| and construct |roots| and
  // |parent_to_children|.
  for (const std::pair<const base::StringPiece,
                       const syncer::UpdateResponseData*>& pair :
       id_to_updates) {
    const syncer::EntityData& update_entity = pair.second->entity;
    parent_to_children[update_entity.parent_id].push_back(update_entity.id);
    // If this entity's parent has no pending update, add it to |roots|.
    if (id_to_updates.count(update_entity.parent_id) == 0) {
      roots.insert(update_entity.parent_id);
    }
  }
  // |roots| contains only root of all trees in the forest all of which are
  // ready to be processed because none has a pending update.
  std::vector<const syncer::UpdateResponseData*> ordered_updates;
  for (const base::StringPiece& root : roots) {
    TraverseAndAppendChildren(root, id_to_updates, parent_to_children,
                              &ordered_updates);
  }

  int root_node_updates_count = 0;
  // Add deletions.
  for (const syncer::UpdateResponseData& update : *updates) {
    const syncer::EntityData& update_entity = update.entity;
    // Ignore updates to root nodes.
    if (update_entity.parent_id == "0") {
      root_node_updates_count++;
      continue;
    }
    if (update_entity.is_deleted()) {
      ordered_updates.push_back(&update);
    }
  }
  // All non root updates should have been included in |ordered_updates|.
  DCHECK_EQ(updates->size(), ordered_updates.size() + root_node_updates_count);
  return ordered_updates;
}

const SyncedNoteTracker::Entity* NoteRemoteUpdatesHandler::ProcessCreate(
    const syncer::UpdateResponseData& update) {
  const syncer::EntityData& update_entity = update.entity;
  DCHECK(!update_entity.is_deleted());
  if (!update_entity.server_defined_unique_tag.empty()) {
    DLOG(ERROR)
        << "Permanent nodes should have been merged during intial sync.";
    return nullptr;
  }

  DCHECK(IsValidNotesSpecifics(update_entity.specifics.notes(),
                               update_entity.is_folder));

  const vivaldi::NoteNode* parent_node = GetParentNode(update_entity);
  if (!parent_node) {
    // If we cannot find the parent, we can do nothing.
    DLOG(ERROR) << "Could not find parent of node being added."
                << " Node title: "
                << update_entity.specifics.notes().legacy_canonicalized_title()
                << ", parent id = " << update_entity.parent_id;
    return nullptr;
  }
  if (!parent_node->is_folder()) {
    DLOG(ERROR) << "Parent node is not a folder. Node title: "
                << update_entity.specifics.notes().legacy_canonicalized_title()
                << ", parent id: " << update_entity.parent_id;
    return nullptr;
  }
  const vivaldi::NoteNode* note_node = CreateNoteNodeFromSpecifics(
      update_entity.specifics.notes(), parent_node,
      ComputeChildNodeIndex(parent_node, update_entity.unique_position,
                            note_tracker_),
      update_entity.is_folder, notes_model_);
  DCHECK(note_node);
  const SyncedNoteTracker::Entity* entity = note_tracker_->Add(
      note_node, update_entity.id, update.response_version,
      update_entity.creation_time, update_entity.unique_position,
      update_entity.specifics);
  ReuploadEntityIfNeeded(update_entity.specifics.notes(), entity);
  return entity;
}

void NoteRemoteUpdatesHandler::ProcessUpdate(
    const syncer::UpdateResponseData& update,
    const SyncedNoteTracker::Entity* tracked_entity) {
  const syncer::EntityData& update_entity = update.entity;
  // Can only update existing nodes.
  DCHECK(tracked_entity);
  DCHECK_EQ(tracked_entity,
            note_tracker_->GetEntityForSyncId(update_entity.id));
  // Must not be a deletion.
  DCHECK(!update_entity.is_deleted());

  DCHECK(IsValidNotesSpecifics(update_entity.specifics.notes(),
                               update_entity.is_folder));
  DCHECK(!tracked_entity->IsUnsynced());

  const vivaldi::NoteNode* node = tracked_entity->note_node();
  const vivaldi::NoteNode* old_parent = node->parent();

  const SyncedNoteTracker::Entity* new_parent_entity =
      note_tracker_->GetEntityForSyncId(update_entity.parent_id);
  if (!new_parent_entity) {
    DLOG(ERROR) << "Could not update node. Parent node doesn't exist: "
                << update_entity.parent_id;
    return;
  }
  const vivaldi::NoteNode* new_parent = new_parent_entity->note_node();
  if (!new_parent) {
    DLOG(ERROR)
        << "Could not update node. Parent node has been deleted already.";
    return;
  }
  if (!new_parent->is_folder()) {
    DLOG(ERROR) << "Could not update node. Parent not is not a folder.";
    return;
  }
  // Node update could be either in the node data (e.g. title or
  // unique_position), or it could be that the node has moved under another
  // parent without any data change. Should check both the data and the parent
  // to confirm that no updates to the model are needed.
  if (tracked_entity->MatchesDataIgnoringParent(update_entity) &&
      new_parent == old_parent) {
    note_tracker_->Update(tracked_entity, update.response_version,
                          update_entity.modification_time,
                          update_entity.unique_position,
                          update_entity.specifics);
    return;
  }
  ApplyRemoteUpdate(update, tracked_entity, new_parent_entity, notes_model_,
                    note_tracker_);
  ReuploadEntityIfNeeded(update_entity.specifics.notes(), tracked_entity);
}

void NoteRemoteUpdatesHandler::ProcessDelete(
    const syncer::EntityData& update_entity,
    const SyncedNoteTracker::Entity* tracked_entity) {
  DCHECK(update_entity.is_deleted());

  DCHECK_EQ(tracked_entity,
            note_tracker_->GetEntityForSyncId(update_entity.id));

  // Handle corner cases first.
  if (tracked_entity == nullptr) {
    // Process deletion only if the entity is still tracked. It could have
    // been recursively deleted already with an earlier deletion of its
    // parent.
    DVLOG(1) << "Received remote delete for a non-existing item.";
    return;
  }

  const vivaldi::NoteNode* node = tracked_entity->note_node();
  // Ignore changes to the permanent top-level nodes.  We only care about
  // their children.
  if (notes_model_->is_permanent_node(node)) {
    return;
  }
  // Remove the entities of |node| and its children.
  RemoveEntityAndChildrenFromTracker(node);
  // Remove the node and its children from the model.
  notes_model_->Remove(node);
}

void NoteRemoteUpdatesHandler::ProcessConflict(
    const syncer::UpdateResponseData& update,
    const SyncedNoteTracker::Entity* tracked_entity) {
  const syncer::EntityData& update_entity = update.entity;
  // TODO(crbug.com/516866): Handle the case of conflict as a result of
  // re-encryption request.

  // Can only conflict with existing nodes.
  DCHECK(tracked_entity);
  DCHECK_EQ(tracked_entity,
            note_tracker_->GetEntityForSyncId(update_entity.id));

  if (tracked_entity->metadata()->is_deleted() && update_entity.is_deleted()) {
    // Both have been deleted, delete the corresponding entity from the tracker.
    note_tracker_->Remove(tracked_entity);
    DLOG(WARNING) << "Conflict: CHANGES_MATCH";
    return;
  }

  if (update_entity.is_deleted()) {
    // Only remote has been deleted. Local wins. Record that we received the
    // update from the server but leave the pending commit intact.
    note_tracker_->UpdateServerVersion(tracked_entity, update.response_version);
    DLOG(WARNING) << "Conflict: USE_LOCAL";
    return;
  }

  if (tracked_entity->metadata()->is_deleted()) {
    // Only local node has been deleted. It should be restored from the server
    // data as a remote creation.
    note_tracker_->Remove(tracked_entity);
    ProcessCreate(update);
    DLOG(WARNING) << "Conflict: USE_REMOTE";
    return;
  }

  // No deletions, there are potentially conflicting updates.
  const vivaldi::NoteNode* node = tracked_entity->note_node();
  const vivaldi::NoteNode* old_parent = node->parent();

  const SyncedNoteTracker::Entity* new_parent_entity =
      note_tracker_->GetEntityForSyncId(update_entity.parent_id);
  // The |new_parent_entity| could be null in some racy conditions.  For
  // example, when a client A moves a node and deletes the old parent and
  // commits, and then updates the node again, and at the same time client B
  // updates before receiving the move updates. The client B update will arrive
  // at client A after the parent entity has been deleted already.
  if (!new_parent_entity) {
    DLOG(ERROR) << "Could not update node. Parent node doesn't exist: "
                << update_entity.parent_id;
    return;
  }
  const vivaldi::NoteNode* new_parent = new_parent_entity->note_node();
  // |new_parent| would be null if the parent has been deleted locally and not
  // committed yet. Deletions are executed recursively, so a parent deletions
  // entails child deletion, and if this child has been updated on another
  // client, this would cause conflict.
  if (!new_parent) {
    DLOG(ERROR)
        << "Could not update node. Parent node has been deleted already.";
    return;
  }
  // Either local and remote data match or server wins, and in both cases we
  // should squash any pending commits.
  note_tracker_->AckSequenceNumber(tracked_entity);

  // Node update could be either in the node data (e.g. title or
  // unique_position), or it could be that the node has moved under another
  // parent without any data change. Should check both the data and the parent
  // to confirm that no updates to the model are needed.
  if (tracked_entity->MatchesDataIgnoringParent(update_entity) &&
      new_parent == old_parent) {
    note_tracker_->Update(tracked_entity, update.response_version,
                          update_entity.modification_time,
                          update_entity.unique_position,
                          update_entity.specifics);

    // The changes are identical so there isn't a real conflict.
    DLOG(WARNING) << "Conflict: CHANGES_MATCH";
    return;
  } else {
    // Conflict where data don't match and no remote deletion, and hence server
    // wins. Update the model from server data.
    DLOG(WARNING) << "Conflict: USE_REMOTE";
    ApplyRemoteUpdate(update, tracked_entity, new_parent_entity, notes_model_,
                      note_tracker_);
  }
  ReuploadEntityIfNeeded(update_entity.specifics.notes(), tracked_entity);
}

void NoteRemoteUpdatesHandler::RemoveEntityAndChildrenFromTracker(
    const vivaldi::NoteNode* node) {
  const SyncedNoteTracker::Entity* entity =
      note_tracker_->GetEntityForNoteNode(node);
  DCHECK(entity);
  note_tracker_->Remove(entity);

  for (const auto& child : node->children())
    RemoveEntityAndChildrenFromTracker(child.get());
}

const vivaldi::NoteNode* NoteRemoteUpdatesHandler::GetParentNode(
    const syncer::EntityData& update_entity) const {
  const SyncedNoteTracker::Entity* parent_entity =
      note_tracker_->GetEntityForSyncId(update_entity.parent_id);
  if (!parent_entity) {
    return nullptr;
  }
  return parent_entity->note_node();
}

void NoteRemoteUpdatesHandler::ReuploadEntityIfNeeded(
    const sync_pb::NotesSpecifics& specifics,
    const SyncedNoteTracker::Entity* tracked_entity) {
  if (!IsFullTitleReuploadNeeded(specifics)) {
    return;
  }
  note_tracker_->IncrementSequenceNumber(tracked_entity);
}

}  // namespace sync_notes
