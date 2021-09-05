// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notes/note_model_merger.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/guid.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine/engine_util.h"
#include "components/sync_bookmarks/switches.h"
#include "notes/note_node.h"
#include "notes/notes_model.h"
#include "sync/notes/note_specifics_conversions.h"
#include "sync/notes/synced_note_tracker.h"
#include "sync/vivaldi_hash_util.h"
#include "ui/base/models/tree_node_iterator.h"

using syncer::EntityData;
using syncer::UpdateResponseData;
using syncer::UpdateResponseDataList;

namespace sync_notes {

namespace {

static const size_t kInvalidIndex = -1;

// The sync protocol identifies top-level entities by means of well-known tags,
// (aka server defined tags) which should not be confused with titles or client
// tags that aren't supported by notes (at the time of writing). Each tag
// corresponds to a singleton instance of a particular top-level node in a
// user's share; the tags are consistent across users. The tags allow us to
// locate the specific folders whose contents we care about synchronizing,
// without having to do a lookup by name or path.  The tags should not be made
// user-visible. For example, the tag "main_notes" represents the permanent
// node under which notes are normally stored in vivaldi. The tag "other_notes"
// represents the currently unused permanent folder Other Notes in Vivaldi.
//
// It is the responsibility of something upstream (at time of writing, the sync
// server) to create these tagged nodes when initializing sync for the first
// time for a user.  Thus, once the backend finishes initializing, the
// ProfileSyncService can rely on the presence of tagged nodes.
const char kMainNotesTag[] = "main_notes";
const char kOtherNotesTag[] = "other_notes";
const char kTrashNotesTag[] = "trash_notes";

// Maximum depth to sync bookmarks tree to protect against stack overflow.
// Keep in sync with |base::internal::kAbsoluteMaxDepth| in json_common.h.
const size_t kMaxNoteTreeDepth = 200;

// The value must be a list since there is a container using pointers to its
// elements.
using UpdatesPerParentId =
    std::unordered_map<std::string, std::list<syncer::UpdateResponseData>>;

enum class NotesGUIDDuplicates {
  // Both entities are notes.
  kBothNotes = 0,
  // Both entities are folders.
  kBothFolders = 1,
  // Entities have different types.
  kDifferentTypes = 2,
};

// Gets the note node corresponding to a permanent folder identified by
// |server_defined_unique_tag|. |notes_model| must not be null.
const vivaldi::NoteNode* GetPermanentFolder(
    const vivaldi::NotesModel* notes_model,
    const std::string& server_defined_unique_tag) {
  DCHECK(notes_model);

  if (server_defined_unique_tag == kMainNotesTag) {
    return notes_model->main_node();
  }
  if (server_defined_unique_tag == kOtherNotesTag) {
    return notes_model->other_node();
  }
  if (server_defined_unique_tag == kTrashNotesTag) {
    return notes_model->trash_node();
  }

  return nullptr;
}

std::string LegacyCanonicalizedTitleFromSpecifics(
    const sync_pb::NotesSpecifics& specifics) {
  if (specifics.has_full_title()) {
    return FullTitleToLegacyCanonicalizedTitle(specifics.full_title());
  }
  return specifics.legacy_canonicalized_title();
}

// Heuristic to consider two nodes (local and remote) a match by semantics for
// the purpose of merging. Two folders match by semantics if they have the same
// title, two notes match by semantics if they have the same title, content and
// url. Separators are matched by title as well. Folders, notes and separators
// never match one another.
bool NodeSemanticsMatch(const vivaldi::NoteNode* local_node,
                        const EntityData& remote_node) {
  if (local_node->is_folder() != remote_node.is_folder) {
    return false;
  }
  const sync_pb::NotesSpecifics& specifics = remote_node.specifics.notes();
  const std::string local_title = base::UTF16ToUTF8(local_node->GetTitle());
  const std::string remote_title =
      LegacyCanonicalizedTitleFromSpecifics(specifics);
  // Titles match if they are identical or the remote one is the canonical form
  // of the local one. The latter is the case when a legacy client has
  // canonicalized the same local title before committing it. Modern clients
  // don't canonicalize titles anymore.
  // TODO(rushans): the comment above is off.
  if (local_title != remote_title &&
      sync_notes::FullTitleToLegacyCanonicalizedTitle(local_title) !=
          remote_title) {
    return false;
  }
  if (remote_node.is_folder) {
    return true;
  }

  if (specifics.special_node_type() == sync_pb::NotesSpecifics::SEPARATOR) {
    return local_node->is_separator();
  }

  return local_node->GetURL() == GURL(specifics.url()) &&
         local_node->GetContent() == base::UTF8ToUTF16(specifics.content());
}

NotesGUIDDuplicates MatchNotesGUIDDuplicates(
    const UpdateResponseData& update,
    const UpdateResponseData& duplicate_update) {
  if (update.entity.is_folder != duplicate_update.entity.is_folder) {
    return NotesGUIDDuplicates::kDifferentTypes;
  }
  if (update.entity.is_folder) {
    return NotesGUIDDuplicates::kBothFolders;
  }
  return NotesGUIDDuplicates::kBothNotes;
}

void ReparentAllChildren(const std::string& from_parent_id,
                         const std::string& to_parent_id,
                         UpdatesPerParentId* updates_per_parent_id) {
  // Any of parents may be empty.
  auto from_parent_updates_iter = updates_per_parent_id->find(from_parent_id);
  if (from_parent_updates_iter == updates_per_parent_id->end()) {
    // There is nothing to merge.
    return;
  }

  // Update parent ids for all entities before moving.
  for (auto& update : from_parent_updates_iter->second) {
    DCHECK_EQ(update.entity.parent_id, from_parent_id);
    update.entity.parent_id = to_parent_id;
  }

  // Move all elements to a new parent (create one if it didn't exist).
  (*updates_per_parent_id)[to_parent_id].splice(
      (*updates_per_parent_id)[to_parent_id].end(),
      from_parent_updates_iter->second);
  updates_per_parent_id->erase(from_parent_id);

  // No need to update iterators since splice doesn't invalidate them.
}

void DeduplicateValidUpdatesByGUID(UpdatesPerParentId* updates_per_parent_id) {
  DCHECK(updates_per_parent_id);

  std::unordered_map<std::string, std::list<UpdateResponseData>::iterator>
      guid_to_update;

  // Removing data in a separate loop helps easier merge parents since one of
  // them may have already been processed.
  std::list<std::list<UpdateResponseData>::iterator> updates_to_remove;
  for (auto& parent_id_and_updates : *updates_per_parent_id) {
    std::list<UpdateResponseData>* updates = &parent_id_and_updates.second;
    for (auto updates_iter = updates->begin(); updates_iter != updates->end();
         ++updates_iter) {
      const UpdateResponseData& update = *updates_iter;
      DCHECK(!update.entity.is_deleted());

      const std::string& guid_in_specifics =
          update.entity.specifics.notes().guid();
      DCHECK(!guid_in_specifics.empty());

      auto it_and_success =
          guid_to_update.emplace(guid_in_specifics, updates_iter);
      if (it_and_success.second) {
        continue;
      }
      const UpdateResponseData& duplicate_update =
          *it_and_success.first->second;
      DCHECK_EQ(guid_in_specifics,
                duplicate_update.entity.specifics.notes().guid());
      DLOG(ERROR) << "Duplicate guid for new sync ID " << update.entity.id
                  << " and original sync ID " << duplicate_update.entity.id;
      const NotesGUIDDuplicates match_result =
          MatchNotesGUIDDuplicates(update, duplicate_update);
      if (match_result == NotesGUIDDuplicates::kDifferentTypes ||
          !base::FeatureList::IsEnabled(
              switches::kSyncDeduplicateAllBookmarksWithSameGUID)) {
        // There shouldn't be cases with different types for duplicate
        // entities.
        continue;
      }

      // Choose the latest element to keep.
      if (update.entity.creation_time > duplicate_update.entity.creation_time) {
        updates_to_remove.push_back(it_and_success.first->second);
        // Update |guid_to_update| to find a duplicate folder and merge them.
        guid_to_update[guid_in_specifics] = updates_iter;
      } else {
        updates_to_remove.push_back(updates_iter);
      }
    }
  }

  for (std::list<UpdateResponseData>::iterator updates_iter :
       updates_to_remove) {
    if (updates_iter->entity.is_folder) {
      const std::string& guid = updates_iter->entity.specifics.notes().guid();
      DCHECK_EQ(1U, guid_to_update.count(guid));
      DCHECK(guid_to_update[guid] != updates_iter);
      // Merge doesn't affect iterators.
      ReparentAllChildren(
          /*from_parent_id=*/updates_iter->entity.id,
          /*to_parent_id=*/guid_to_update[guid]->entity.id,
          updates_per_parent_id);
    }

    const std::string& parent_id = updates_iter->entity.parent_id;
    DCHECK_EQ(1U, updates_per_parent_id->count(parent_id));
    (*updates_per_parent_id)[parent_id].erase(updates_iter);
  }
}

// Groups all valid updates by the server ID of their parent and moves them away
// from |*updates|. |updates| must not be null.
UpdatesPerParentId GroupValidUpdatesByParentId(
    UpdateResponseDataList* updates) {
  UpdatesPerParentId updates_per_parent_id;

  for (UpdateResponseData& update : *updates) {
    const EntityData& update_entity = update.entity;
    if (update_entity.is_deleted()) {
      continue;
    }
    // No need to associate permanent nodes with their parent (the root node).
    // We start merging from the permanent nodes.
    if (!update_entity.server_defined_unique_tag.empty()) {
      continue;
    }
    if (!syncer::UniquePosition::FromProto(update_entity.unique_position)
             .IsValid()) {
      // Ignore updates with invalid positions.
      DLOG(ERROR)
          << "Remote update with invalid position: "
          << update_entity.specifics.notes().legacy_canonicalized_title();
      continue;
    }
    if (!IsValidNotesSpecifics(update_entity.specifics.notes(),
                               update_entity.is_folder)) {
      // Ignore updates with invalid specifics.
      DLOG(ERROR) << "Remote update with invalid specifics";
      continue;
    }
    if (!HasExpectedNoteGuid(update_entity.specifics.notes(),
                             update_entity.originator_cache_guid,
                             update_entity.originator_client_item_id)) {
      // Ignore updates with an unexpected GUID.
      DLOG(ERROR) << "Remote update with unexpected GUID";
      continue;
    }

    updates_per_parent_id[update_entity.parent_id].push_back(std::move(update));
  }

  return updates_per_parent_id;
}

}  // namespace

NoteModelMerger::RemoteTreeNode::RemoteTreeNode() = default;

NoteModelMerger::RemoteTreeNode::~RemoteTreeNode() = default;

NoteModelMerger::RemoteTreeNode::RemoteTreeNode(
    NoteModelMerger::RemoteTreeNode&&) = default;
NoteModelMerger::RemoteTreeNode& NoteModelMerger::RemoteTreeNode::operator=(
    NoteModelMerger::RemoteTreeNode&&) = default;

void NoteModelMerger::RemoteTreeNode::EmplaceSelfAndDescendantsByGUID(
    std::unordered_map<std::string, const RemoteTreeNode*>*
        guid_to_remote_node_map) const {
  DCHECK(guid_to_remote_node_map);

  if (entity().server_defined_unique_tag.empty()) {
    const std::string& guid = entity().specifics.notes().guid();
    DCHECK(base::IsValidGUIDOutputString(guid));

    // Duplicate GUIDs have been sorted out before.
    bool success = guid_to_remote_node_map->emplace(guid, this).second;
    DCHECK(success);
  }

  for (const RemoteTreeNode& child : children_) {
    child.EmplaceSelfAndDescendantsByGUID(guid_to_remote_node_map);
  }
}

// static
bool NoteModelMerger::RemoteTreeNode::UniquePositionLessThan(
    const RemoteTreeNode& lhs,
    const RemoteTreeNode& rhs) {
  const syncer::UniquePosition a_pos =
      syncer::UniquePosition::FromProto(lhs.entity().unique_position);
  const syncer::UniquePosition b_pos =
      syncer::UniquePosition::FromProto(rhs.entity().unique_position);
  return a_pos.LessThan(b_pos);
}

// static
NoteModelMerger::RemoteTreeNode NoteModelMerger::RemoteTreeNode::BuildTree(
    UpdateResponseData update,
    size_t max_depth,
    UpdatesPerParentId* updates_per_parent_id) {
  DCHECK(updates_per_parent_id);

  RemoteTreeNode node;
  node.update_ = std::move(update);

  // Ensure we have not reached the maximum tree depth to guard against stack
  // overflows.
  if (max_depth == 0) {
    return node;
  }

  // Only folders may have descendants (ignore them otherwise). Treat
  // permanent nodes as folders explicitly.
  if (!node.update_.entity.is_folder &&
      node.update_.entity.server_defined_unique_tag.empty()) {
    return node;
  }

  // Populate descendants recursively.
  for (UpdateResponseData& child_update :
       (*updates_per_parent_id)[node.entity().id]) {
    DCHECK_EQ(child_update.entity.parent_id, node.entity().id);
    DCHECK(IsValidNotesSpecifics(child_update.entity.specifics.notes(),
                                 child_update.entity.is_folder));

    node.children_.push_back(BuildTree(std::move(child_update), max_depth - 1,
                                       updates_per_parent_id));
  }

  // Sort the children according to their unique position.
  std::stable_sort(node.children_.begin(), node.children_.end(),
                   UniquePositionLessThan);

  return node;
}

NoteModelMerger::NoteModelMerger(UpdateResponseDataList updates,
                                 vivaldi::NotesModel* notes_model,
                                 SyncedNoteTracker* note_tracker)
    : notes_model_(notes_model),
      note_tracker_(note_tracker),
      remote_forest_(BuildRemoteForest(std::move(updates))),
      guid_to_match_map_(
          FindGuidMatchesOrReassignLocal(remote_forest_, notes_model_)) {
  DCHECK(note_tracker_->IsEmpty());
}

NoteModelMerger::~NoteModelMerger() {}

void NoteModelMerger::Merge() {
  // Algorithm description:
  // Match up the roots and recursively do the following:
  // * For each remote node for the current remote (sync) parent node, either
  //   find a local node with equal GUID anywhere throughout the tree or find
  //   the best matching note node under the corresponding local note
  //   parent node using semantics. If the found node has the same GUID as a
  //   different remote note, we do not consider it a semantics match, as
  //   GUID matching takes precedence. If no matching node is found, create a
  //   new note node in the same position as the corresponding remote node.
  //   If a matching node is found, update the properties of it from the
  //   corresponding remote node.
  // * When all children remote nodes are done, add the extra children note
  //   nodes to the remote (sync) parent node, unless they will be later matched
  //   by GUID.
  //
  // The semantics best match algorithm uses folder title or note title, content
  // and url to perform the primary match. If there are multiple match
  // candidates it selects the first one.
  // Associate permanent folders.
  for (const auto& tree_tag_and_root : remote_forest_) {
    const vivaldi::NoteNode* permanent_folder =
        GetPermanentFolder(notes_model_, tree_tag_and_root.first);
    if (!permanent_folder) {
      continue;
    }
    MergeSubtree(/*local_subtree_root=*/permanent_folder,
                 /*remote_node=*/tree_tag_and_root.second);
  }

  if (base::FeatureList::IsEnabled(switches::kSyncReuploadBookmarkFullTitles)) {
    // When the reupload feature is enabled, all new empty trackers are
    // automatically reuploaded (since there are no entities to reupload). This
    // is used to disable reupload after initial merge.
    note_tracker_->SetNotesFullTitleReuploaded();
  }
}

// static
NoteModelMerger::RemoteForest NoteModelMerger::BuildRemoteForest(
    syncer::UpdateResponseDataList updates) {
  // Filter out invalid remote updates and group the valid ones by the server ID
  // of their parent.
  UpdatesPerParentId updates_per_parent_id =
      GroupValidUpdatesByParentId(&updates);

  DeduplicateValidUpdatesByGUID(&updates_per_parent_id);

  // Construct one tree per permanent entity.
  RemoteForest update_forest;
  for (UpdateResponseData& update : updates) {
    if (update.entity.server_defined_unique_tag.empty()) {
      continue;
    }

    // Make a copy of the string to avoid relying on argument evaluation order.
    const std::string server_defined_unique_tag =
        update.entity.server_defined_unique_tag;

    update_forest.emplace(
        server_defined_unique_tag,
        RemoteTreeNode::BuildTree(std::move(update), kMaxNoteTreeDepth,
                                  &updates_per_parent_id));
  }

  // TODO(crbug.com/978430): Add UMA to record the number of orphan nodes.

  return update_forest;
}

// static
std::unordered_map<std::string, NoteModelMerger::GuidMatch>
NoteModelMerger::FindGuidMatchesOrReassignLocal(
    const RemoteForest& remote_forest,
    vivaldi::NotesModel* notes_model) {
  DCHECK(notes_model);

  // Build a temporary lookup table for remote GUIDs.
  std::unordered_map<std::string, const RemoteTreeNode*>
      guid_to_remote_node_map;
  for (const auto& tree_tag_and_root : remote_forest) {
    tree_tag_and_root.second.EmplaceSelfAndDescendantsByGUID(
        &guid_to_remote_node_map);
  }

  // Iterate through all local notes to find matches by GUID.
  std::unordered_map<std::string, NoteModelMerger::GuidMatch> guid_to_match_map;
  // Because ReplaceNoteNodeGUID() cannot be used while iterating the local
  // notes model, a temporary list is constructed first to reassign later.
  std::vector<const vivaldi::NoteNode*> nodes_to_replace_guid;
  ui::TreeNodeIterator<const vivaldi::NoteNode> iterator(
      notes_model->root_node());
  while (iterator.has_next()) {
    const vivaldi::NoteNode* const node = iterator.Next();
    DCHECK(base::IsValidGUIDOutputString(node->guid()));

    const auto remote_it = guid_to_remote_node_map.find(node->guid());
    if (remote_it == guid_to_remote_node_map.end()) {
      continue;
    }

    const RemoteTreeNode* const remote_node = remote_it->second;
    const syncer::EntityData& remote_entity = remote_node->entity();

    // Permanent nodes don't match by GUID but by |server_defined_unique_tag|.
    // As extra precaution, specially with remote GUIDs in mind, let's ignore
    // them explicitly here.
    if (node->is_permanent_node() ||
        GetPermanentFolder(
            notes_model, remote_entity.server_defined_unique_tag) != nullptr) {
      continue;
    }

    if (node->is_folder() != remote_entity.is_folder ||
        (node->is_separator() &&
         remote_entity.specifics.notes().special_node_type() !=
             sync_pb::NotesSpecifics::SEPARATOR) ||
        (node->is_note() &&
         node->GetContent() !=
             base::UTF8ToUTF16(remote_entity.specifics.notes().content())) ||
        !base::FeatureList::IsEnabled(switches::kMergeBookmarksUsingGUIDs)) {
      // If local node and its remote node match are conflicting in node type or
      // content, replace local GUID with a random GUID. The logic is applied
      // unconditionally if kMergeBookmarksUsingGUIDs is disabled, since no
      // GUID-based matches take place and GUIDs need to be reassigned to avoid
      // collisions (they will be reassigned once again if there is a semantic
      // match).
      nodes_to_replace_guid.push_back(node);
      continue;
    }

    bool success =
        guid_to_match_map.emplace(node->guid(), GuidMatch{node, remote_node})
            .second;

    // Insertion must have succeeded unless there were duplicate GUIDs in the
    // local NotesModel (invariant violation that gets resolved upon
    // restart).
    // TODO(crbug.com/516866): The below CHECK is added to debug some crashes.
    // Should be converted to a DCHECK after the root cause if found.
    CHECK(success);
  }

  for (const vivaldi::NoteNode* node : nodes_to_replace_guid) {
    ReplaceNoteNodeGUID(node, base::GenerateGUID(), notes_model);
  }

  return guid_to_match_map;
}

void NoteModelMerger::MergeSubtree(const vivaldi::NoteNode* local_subtree_root,
                                   const RemoteTreeNode& remote_node) {
  const EntityData& remote_update_entity = remote_node.entity();
  const SyncedNoteTracker::Entity* entity = note_tracker_->Add(
      local_subtree_root, remote_update_entity.id,
      remote_node.response_version(), remote_update_entity.creation_time,
      remote_update_entity.unique_position, remote_update_entity.specifics);
  if (!local_subtree_root->is_permanent_node() &&
      IsNoteEntityReuploadNeeded(remote_update_entity)) {
    note_tracker_->IncrementSequenceNumber(entity);
  }

  // If there are remote child updates, try to match them.
  for (size_t remote_index = 0; remote_index < remote_node.children().size();
       ++remote_index) {
    // TODO(crbug.com/1050776): change to DCHECK after investigating.
    // Here is expected that all nodes to the left of current |remote_index| are
    // filled with remote updates. All local nodes which are not merged will be
    // added later.
    CHECK_LE(remote_index, local_subtree_root->children().size());
    const RemoteTreeNode& remote_child =
        remote_node.children().at(remote_index);
    const vivaldi::NoteNode* matching_local_node =
        FindMatchingLocalNode(remote_child, local_subtree_root, remote_index);
    // If no match found, create a corresponding local node.
    if (!matching_local_node) {
      ProcessRemoteCreation(remote_child, local_subtree_root, remote_index);
      continue;
    }
    DCHECK(!local_subtree_root->HasAncestor(matching_local_node));
    // Move if required, no-op otherwise.
    notes_model_->Move(matching_local_node, local_subtree_root, remote_index);
    // Since nodes are matching, their subtrees should be merged as well.
    matching_local_node = UpdateNoteNodeFromSpecificsIncludingGUID(
        matching_local_node, remote_child);
    MergeSubtree(matching_local_node, remote_child);
  }

  // At this point all the children of |remote_node| have corresponding local
  // nodes under |local_subtree_root| and they are all in the right positions:
  // from 0 to remote_node.children().size() - 1.
  //
  // This means, the children starting from remote_node.children().size() in
  // the parent note node are the ones that are not present in the parent
  // sync node and not tracked yet. So create all of the remaining local nodes.
  DCHECK_LE(remote_node.children().size(),
            local_subtree_root->children().size());

  for (size_t i = remote_node.children().size();
       i < local_subtree_root->children().size(); ++i) {
    // If local node has been or will be matched by GUID, skip it.
    if (FindMatchingRemoteNodeByGUID(local_subtree_root->children()[i].get())) {
      continue;
    }
    ProcessLocalCreation(local_subtree_root, i);
  }
}

const vivaldi::NoteNode* NoteModelMerger::FindMatchingLocalNode(
    const RemoteTreeNode& remote_child,
    const vivaldi::NoteNode* local_parent,
    size_t local_child_start_index) const {
  DCHECK(local_parent);
  // Try to match child by GUID. If we can't, try to match child by semantics.
  const vivaldi::NoteNode* matching_local_node_by_guid =
      FindMatchingLocalNodeByGUID(remote_child);
  if (matching_local_node_by_guid) {
    return matching_local_node_by_guid;
  }

  // All local nodes up to |remote_index-1| have processed already. Look for a
  // matching local node starting with the local node at position
  // |local_child_start_index|. FindMatchingChildBySemanticsStartingAt()
  // returns kInvalidIndex in the case where no semantics match was found or
  // the semantics match found is GUID-matchable to a different node.
  const size_t local_index = FindMatchingChildBySemanticsStartingAt(
      /*remote_node=*/remote_child,
      /*local_parent=*/local_parent,
      /*starting_child_index=*/local_child_start_index);
  if (local_index == kInvalidIndex) {
    // If no match found, return.
    return nullptr;
  }

  // The child at |local_index| has matched by semantics, which also means it
  // does not match by GUID to any other remote node.
  const vivaldi::NoteNode* matching_local_node_by_semantics =
      local_parent->children()[local_index].get();
  DCHECK(!FindMatchingRemoteNodeByGUID(matching_local_node_by_semantics));
  return matching_local_node_by_semantics;
}

const vivaldi::NoteNode*
NoteModelMerger::UpdateNoteNodeFromSpecificsIncludingGUID(
    const vivaldi::NoteNode* local_node,
    const RemoteTreeNode& remote_node) {
  DCHECK(local_node);
  DCHECK(!local_node->is_permanent_node());
  // Ensure notes have the same content, otherwise they would not have been
  // matched.
  DCHECK(
      local_node->is_folder() || local_node->is_separator() ||
      local_node->GetContent() ==
          base::UTF8ToUTF16(remote_node.entity().specifics.notes().content()));
  const EntityData& remote_update_entity = remote_node.entity();
  const sync_pb::NotesSpecifics& specifics =
      remote_update_entity.specifics.notes();

  // Update the local GUID if necessary for semantic matches (it's obviously not
  // needed for GUID-based matches).
  const vivaldi::NoteNode* possibly_replaced_local_node = local_node;
  if (!specifics.guid().empty() && specifics.guid() != local_node->guid()) {
    // If it's a semantic match, neither of the nodes should be involved in any
    // GUID-based match.
    DCHECK(!FindMatchingLocalNodeByGUID(remote_node));
    DCHECK(!FindMatchingRemoteNodeByGUID(local_node));

    possibly_replaced_local_node =
        ReplaceNoteNodeGUID(local_node, specifics.guid(), notes_model_);

    // TODO(rushans): remove the code below since DCHECKs above guarantee that
    // |guid_to_match_map_| has no such GUID.
    //
    // Update |guid_to_match_map_| to avoid pointing to a deleted node. This
    // should not be required in practice, because the algorithm processes each
    // GUID once, but let's update nevertheless to avoid future issues.
    const auto it =
        guid_to_match_map_.find(possibly_replaced_local_node->guid());
    if (it != guid_to_match_map_.end() && it->second.local_node == local_node) {
      it->second.local_node = possibly_replaced_local_node;
    }
  }

  // Update all fields, where no-op changes are handled well.
  UpdateNoteNodeFromSpecifics(specifics, possibly_replaced_local_node,
                              notes_model_);

  return possibly_replaced_local_node;
}

void NoteModelMerger::ProcessRemoteCreation(
    const RemoteTreeNode& remote_node,
    const vivaldi::NoteNode* local_parent,
    size_t index) {
  DCHECK(!FindMatchingLocalNodeByGUID(remote_node));

  const EntityData& remote_update_entity = remote_node.entity();
  DCHECK(IsValidNotesSpecifics(remote_update_entity.specifics.notes(),
                               remote_update_entity.is_folder));

  const sync_pb::EntitySpecifics& specifics = remote_node.entity().specifics;
  const vivaldi::NoteNode* note_node =
      CreateNoteNodeFromSpecifics(specifics.notes(), local_parent, index,
                                  remote_update_entity.is_folder, notes_model_);
  DCHECK(note_node);
  const SyncedNoteTracker::Entity* entity = note_tracker_->Add(
      note_node, remote_update_entity.id, remote_node.response_version(),
      remote_update_entity.creation_time, remote_update_entity.unique_position,
      specifics);
  if (IsNoteEntityReuploadNeeded(remote_node.entity())) {
    note_tracker_->IncrementSequenceNumber(entity);
  }

  // Recursively, match by GUID or, if not possible, create local node for all
  // child remote nodes.
  size_t i = 0;
  for (const RemoteTreeNode& remote_child : remote_node.children()) {
    // TODO(crbug.com/1050776): change to DCHECK after investigating of some
    // crashes.
    CHECK_LE(i, note_node->children().size());
    const vivaldi::NoteNode* local_child =
        FindMatchingLocalNodeByGUID(remote_child);
    if (!local_child) {
      ProcessRemoteCreation(remote_child, note_node, i++);
      continue;
    }
    notes_model_->Move(local_child, note_node, i++);
    local_child =
        UpdateNoteNodeFromSpecificsIncludingGUID(local_child, remote_child);
    MergeSubtree(local_child, remote_child);
  }
}

void NoteModelMerger::ProcessLocalCreation(const vivaldi::NoteNode* parent,
                                           size_t index) {
  DCHECK_LE(index, parent->children().size());
  const SyncedNoteTracker::Entity* parent_entity =
      note_tracker_->GetEntityForNoteNode(parent);
  // Since we are merging top down, parent entity must be tracked.
  DCHECK(parent_entity);

  // Assign a temp server id for the entity. Will be overridden by the actual
  // server id upon receiving commit response.
  const vivaldi::NoteNode* node = parent->children()[index].get();
  DCHECK(!FindMatchingRemoteNodeByGUID(node));
  DCHECK(base::IsValidGUIDOutputString(node->guid()));

  // The node's GUID cannot run into collisions because
  // FindGuidMatchesOrReassignLocal() takes care of reassigning local GUIDs if
  // they won't actually be merged with the remote note with the same GUID
  // (e.g. incompatible types).
  const std::string sync_id = node->guid();
  const int64_t server_version = syncer::kUncommittedVersion;
  const base::Time creation_time = base::Time::Now();
  const std::string& suffix = syncer::GenerateSyncableNotesHash(
      note_tracker_->model_type_state().cache_guid(), sync_id);
  // Locally created nodes aren't tracked and hence don't have a unique position
  // yet so we need to produce new ones.
  const syncer::UniquePosition pos =
      GenerateUniquePositionForLocalCreation(parent, index, suffix);
  const sync_pb::EntitySpecifics specifics =
      CreateSpecificsFromNoteNode(node, notes_model_, /*include_guid=*/true);
  const SyncedNoteTracker::Entity* entity = note_tracker_->Add(
      node, sync_id, server_version, creation_time, pos.ToProto(), specifics);
  // Mark the entity that it needs to be committed.
  note_tracker_->IncrementSequenceNumber(entity);
  for (size_t i = 0; i < node->children().size(); ++i) {
    // If a local node hasn't matched with any remote entity, its descendants
    // will neither, unless they have been or will be matched by GUID, in which
    // case we skip them for now.
    if (FindMatchingRemoteNodeByGUID(node->children()[i].get())) {
      continue;
    }
    ProcessLocalCreation(/*parent=*/node, i);
  }
}

size_t NoteModelMerger::FindMatchingChildBySemanticsStartingAt(
    const RemoteTreeNode& remote_node,
    const vivaldi::NoteNode* local_parent,
    size_t starting_child_index) const {
  DCHECK(local_parent);
  const auto& children = local_parent->children();
  DCHECK_LE(starting_child_index, children.size());
  const EntityData& remote_entity = remote_node.entity();
  const auto it =
      std::find_if(children.cbegin() + starting_child_index, children.cend(),
                   [this, &remote_entity](const auto& child) {
                     return NodeSemanticsMatch(child.get(), remote_entity) &&
                            !FindMatchingRemoteNodeByGUID(child.get());
                   });
  return (it == children.cend()) ? kInvalidIndex : (it - children.cbegin());
}

const NoteModelMerger::RemoteTreeNode*
NoteModelMerger::FindMatchingRemoteNodeByGUID(
    const vivaldi::NoteNode* local_node) const {
  DCHECK(local_node);

  const auto it = guid_to_match_map_.find(local_node->guid());
  if (it == guid_to_match_map_.end()) {
    return nullptr;
  }

  DCHECK_EQ(it->second.local_node, local_node);
  return it->second.remote_node;
}

const vivaldi::NoteNode* NoteModelMerger::FindMatchingLocalNodeByGUID(
    const RemoteTreeNode& remote_node) const {
  const syncer::EntityData& remote_entity = remote_node.entity();
  const auto it =
      guid_to_match_map_.find(remote_entity.specifics.notes().guid());
  if (it == guid_to_match_map_.end()) {
    return nullptr;
  }

  DCHECK_EQ(it->second.remote_node, &remote_node);
  return it->second.local_node;
}

syncer::UniquePosition NoteModelMerger::GenerateUniquePositionForLocalCreation(
    const vivaldi::NoteNode* parent,
    size_t index,
    const std::string& suffix) const {
  // Try to find last tracked preceding entity. It is not always the previous
  // one as it might be skipped if it has unprocessed remote matching by GUID
  // update.
  for (size_t i = index; i > 0; --i) {
    const SyncedNoteTracker::Entity* predecessor_entity =
        note_tracker_->GetEntityForNoteNode(parent->children()[i - 1].get());
    if (predecessor_entity != nullptr) {
      return syncer::UniquePosition::After(
          syncer::UniquePosition::FromProto(
              predecessor_entity->metadata()->unique_position()),
          suffix);
    }
    DCHECK(FindMatchingRemoteNodeByGUID(parent->children()[i - 1].get()));
  }
  return syncer::UniquePosition::InitialPosition(suffix);
}

}  // namespace sync_notes
