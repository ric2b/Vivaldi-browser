// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notes/note_model_merger.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/notes/note_node.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/notes_specifics.pb.h"
#include "components/sync_bookmarks/switches.h"
#include "sync/notes/note_model_view.h"
#include "sync/notes/note_specifics_conversions.h"
#include "sync/notes/synced_note_tracker.h"
#include "sync/notes/synced_note_tracker_entity.h"
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
// SyncService can rely on the presence of tagged nodes.
const char kMainNotesTag[] = "main_notes";
const char kOtherNotesTag[] = "other_notes";
const char kTrashNotesTag[] = "trash_notes";

// Maximum depth to sync notes tree to protect against stack overflow.
// Keep in sync with |base::internal::kAbsoluteMaxDepth| in json_common.h.
const size_t kMaxNoteTreeDepth = 200;

// The value must be a list since there is a container using pointers to its
// elements.
using UpdatesPerParentUuid =
    std::unordered_map<base::Uuid,
                       std::list<syncer::UpdateResponseData>,
                       base::UuidHash>;

// Gets the note node corresponding to a permanent folder identified by
// |server_defined_unique_tag| or null of the tag is unknown. |notes_model|
// must not be null and |server_defined_unique_tag| must not be empty.
const vivaldi::NoteNode* GetPermanentFolderForServerDefinedUniqueTag(
    const NoteModelView* notes_model,
    const std::string& server_defined_unique_tag) {
  DCHECK(notes_model);
  DCHECK(!server_defined_unique_tag.empty());

  // WARNING: Keep this logic consistent with the analogous in
  // GetPermanentFolderUuidForServerDefinedUniqueTag().
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

// Gets the note UUID corresponding to a permanent folder identified by
// |served_defined_unique_tag| or an invalid UUID if the tag is unknown.
// |server_defined_unique_tag| must not be empty.
base::Uuid GetPermanentFolderUuidForServerDefinedUniqueTag(
    const std::string& server_defined_unique_tag) {
  DCHECK(!server_defined_unique_tag.empty());

  // WARNING: Keep this logic consistent with the analogous in
  // GetPermanentFolderForServerDefinedUniqueTag().
  if (server_defined_unique_tag == kMainNotesTag) {
    return base::Uuid::ParseLowercase(vivaldi::NoteNode::kMainNodeUuid);
  }
  if (server_defined_unique_tag == kOtherNotesTag) {
    return base::Uuid::ParseLowercase(vivaldi::NoteNode::kOtherNotesNodeUuid);
  }
  if (server_defined_unique_tag == kTrashNotesTag) {
    return base::Uuid::ParseLowercase(vivaldi::NoteNode::kTrashNodeUuid);
  }

  return base::Uuid();
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
bool NodeSemanticsMatch(
    const vivaldi::NoteNode* local_node,
    const std::string& remote_canonicalized_title,
    const GURL& remote_url,
    const std::u16string& remote_content,
    sync_pb::NotesSpecifics::VivaldiSpecialNotesType remote_type) {
  if (GetProtoTypeFromNoteNode(local_node) != remote_type) {
    return false;
  }

  if ((remote_type == sync_pb::NotesSpecifics::NORMAL ||
       remote_type == sync_pb::NotesSpecifics::ATTACHMENT) &&
      (local_node->GetURL() != remote_url ||
       local_node->GetContent() != remote_content)) {
    return false;
  }

  const std::string local_title = base::UTF16ToUTF8(local_node->GetTitle());
  // Titles match if they are identical or the remote one is the canonical form
  // of the local one. The latter is the case when a legacy client has
  // canonicalized the same local title before committing it. Modern clients
  // don't canonicalize titles anymore.
  // TODO(rushans): the comment above is off.
  if (local_title != remote_canonicalized_title &&
      sync_notes::FullTitleToLegacyCanonicalizedTitle(local_title) !=
          remote_canonicalized_title) {
    return false;
  }
  return true;
}

// Returns true the |next_update| is selected to keep and the |previous_update|
// should be removed. False is returned otherwise. |next_update| and
// |previous_update| must have the same UUID.
bool CompareDuplicateUpdates(const UpdateResponseData& next_update,
                             const UpdateResponseData& previous_update) {
  DCHECK_EQ(next_update.entity.specifics.notes().guid(),
            previous_update.entity.specifics.notes().guid());
  DCHECK_NE(next_update.entity.id, previous_update.entity.id);

  if (next_update.entity.specifics.notes().special_node_type() !=
      previous_update.entity.specifics.notes().special_node_type()) {
    // There are two entities, one of them is a folder and another one is a
    // regular note or an attachment. Prefer to save the folder as it may
    // contain many notes.
    return next_update.entity.specifics.notes().special_node_type() ==
           sync_pb::NotesSpecifics::FOLDER;
  }
  // Choose the latest element to keep if both updates have the same type.
  return next_update.entity.creation_time >
         previous_update.entity.creation_time;
}

void DeduplicateValidUpdatesByUuid(
    UpdatesPerParentUuid* updates_per_parent_uuid) {
  DCHECK(updates_per_parent_uuid);

  std::unordered_map<base::Uuid, std::list<UpdateResponseData>::iterator,
                     base::UuidHash>
      uuid_to_update;

  for (auto& [parent_uuid, updates] : *updates_per_parent_uuid) {
    auto updates_iter = updates.begin();
    while (updates_iter != updates.end()) {
      const UpdateResponseData& update = *updates_iter;
      DCHECK(!update.entity.is_deleted());
      DCHECK(update.entity.server_defined_unique_tag.empty());

      const base::Uuid uuid_in_specifics =
          base::Uuid::ParseLowercase(update.entity.specifics.notes().guid());
      DCHECK(uuid_in_specifics.is_valid());

      auto [it, success] =
          uuid_to_update.emplace(uuid_in_specifics, updates_iter);
      if (success) {
        ++updates_iter;
        continue;
      }

      const auto& [uuid, previous_update_it] = *it;
      DCHECK_EQ(uuid_in_specifics.AsLowercaseString(),
                previous_update_it->entity.specifics.notes().guid());
      DLOG(ERROR) << "Duplicate uuid for new sync ID " << update.entity.id
                  << " and original sync ID " << previous_update_it->entity.id;

      // Choose the latest element to keep.
      if (CompareDuplicateUpdates(/*next_update=*/update,
                                  /*previous_update=*/*previous_update_it)) {
        updates.erase(previous_update_it);
        uuid_to_update[uuid_in_specifics] = updates_iter;
        ++updates_iter;
      } else {
        updates_iter = updates.erase(updates_iter);
      }
    }
  }
}

// Checks that the |update| is valid and returns false otherwise. It is used to
// verify non-deletion updates. |update| must not be a deletion and a permanent
// node (they are processed in a different way).
bool IsValidUpdate(const UpdateResponseData& update) {
  const EntityData& update_entity = update.entity;

  DCHECK(!update_entity.is_deleted());
  DCHECK(update_entity.server_defined_unique_tag.empty());

  if (!IsValidNotesSpecifics(update_entity.specifics.notes())) {
    // Ignore updates with invalid specifics.
    DLOG(ERROR) << "Remote update with invalid specifics";
    return false;
  }
  if (!HasExpectedNoteGuid(update_entity.specifics.notes(),
                           update_entity.client_tag_hash,
                           update_entity.originator_cache_guid,
                           update_entity.originator_client_item_id)) {
    // Ignore updates with an unexpected UUID.
    DLOG(ERROR) << "Remote update with unexpected Uuid";
    return false;
  }
  return true;
}

// Returns the UUID determined by a remote update, which may be an update for a
// permanent folder or a regular note node.
base::Uuid GetUuidForUpdate(const UpdateResponseData& update) {
  if (!update.entity.server_defined_unique_tag.empty()) {
    return GetPermanentFolderUuidForServerDefinedUniqueTag(
        update.entity.server_defined_unique_tag);
  }

  DCHECK(IsValidUpdate(update));
  return base::Uuid::ParseLowercase(update.entity.specifics.notes().guid());
}

struct GroupedUpdates {
  // |updates_per_parent_uuid| contains all valid updates grouped by their
  // |parent_uuid|. Permanent nodes and deletions are filtered out. Permanent
  // nodes are stored in a dedicated list |permanent_node_updates|.
  UpdatesPerParentUuid updates_per_parent_uuid;
  UpdateResponseDataList permanent_node_updates;
};

// Groups all valid updates by the UUID of their parent. Permanent nodes are
// grouped in a dedicated |permanent_node_updates| list in a returned value.
GroupedUpdates GroupValidUpdates(UpdateResponseDataList updates) {
  GroupedUpdates grouped_updates;
  for (UpdateResponseData& update : updates) {
    const EntityData& update_entity = update.entity;
    if (update_entity.is_deleted()) {
      continue;
    }
    // Special-case the root folder to avoid reporting an error.
    if (update_entity.server_defined_unique_tag ==
        syncer::DataTypeToProtocolRootTag(syncer::NOTES)) {
      continue;
    }
    // Non-root permanent folders don't need further validation.
    if (!update_entity.server_defined_unique_tag.empty()) {
      grouped_updates.permanent_node_updates.push_back(std::move(update));
      continue;
    }
    // Regular (non-permanent) node updates must pass IsValidUpdate().
    if (!IsValidUpdate(update)) {
      continue;
    }

    const base::Uuid parent_uuid = base::Uuid::ParseLowercase(
        update_entity.specifics.notes().parent_guid());
    DCHECK(parent_uuid.is_valid());

    grouped_updates.updates_per_parent_uuid[parent_uuid].push_back(
        std::move(update));
  }

  return grouped_updates;
}

}  // namespace

NoteModelMerger::RemoteTreeNode::RemoteTreeNode() = default;

NoteModelMerger::RemoteTreeNode::~RemoteTreeNode() = default;

NoteModelMerger::RemoteTreeNode::RemoteTreeNode(
    NoteModelMerger::RemoteTreeNode&&) = default;
NoteModelMerger::RemoteTreeNode& NoteModelMerger::RemoteTreeNode::operator=(
    NoteModelMerger::RemoteTreeNode&&) = default;

void NoteModelMerger::RemoteTreeNode::EmplaceSelfAndDescendantsByUuid(
    std::unordered_map<base::Uuid, const RemoteTreeNode*, base::UuidHash>*
        uuid_to_remote_node_map) const {
  DCHECK(uuid_to_remote_node_map);

  if (entity().server_defined_unique_tag.empty()) {
    const base::Uuid uuid =
        base::Uuid::ParseLowercase(entity().specifics.notes().guid());
    DCHECK(uuid.is_valid());

    // Duplicate UUIDs have been sorted out before.
    bool success = uuid_to_remote_node_map->emplace(uuid, this).second;
    DCHECK(success);
  }

  for (const RemoteTreeNode& child : children_) {
    child.EmplaceSelfAndDescendantsByUuid(uuid_to_remote_node_map);
  }
}

// static
bool NoteModelMerger::RemoteTreeNode::UniquePositionLessThan(
    const RemoteTreeNode& lhs,
    const RemoteTreeNode& rhs) {
  return lhs.unique_position_.LessThan(rhs.unique_position_);
}

// static
NoteModelMerger::RemoteTreeNode NoteModelMerger::RemoteTreeNode::BuildTree(
    UpdateResponseData update,
    size_t max_depth,
    UpdatesPerParentUuid* updates_per_parent_uuid) {
  DCHECK(updates_per_parent_uuid);
  DCHECK(!update.entity.server_defined_unique_tag.empty() ||
         IsValidUpdate(update));

  // |uuid| may be invalid for unsupported permanent nodes.
  const base::Uuid uuid = GetUuidForUpdate(update);

  RemoteTreeNode node;
  node.update_ = std::move(update);
  node.unique_position_ = syncer::UniquePosition::FromProto(
      node.update_.entity.specifics.notes().unique_position());

  // Ensure we have not reached the maximum tree depth to guard against stack
  // overflows.
  if (max_depth == 0) {
    return node;
  }

  // Check to prevent creating empty lists in |updates_per_parent_uuid| and
  // unnecessary rehashing.
  auto updates_per_parent_uuid_iter = updates_per_parent_uuid->find(uuid);
  if (updates_per_parent_uuid_iter == updates_per_parent_uuid->end()) {
    return node;
  }
  DCHECK(!updates_per_parent_uuid_iter->second.empty());
  DCHECK(uuid.is_valid());

  // Only folders and regular notes may have descendants (ignore them
  // otherwise). Treat permanent nodes as folders explicitly.
  if (node.update_.entity.specifics.notes().special_node_type() !=
          sync_pb::NotesSpecifics::FOLDER &&
      node.update_.entity.specifics.notes().special_node_type() !=
          sync_pb::NotesSpecifics::NORMAL &&
      node.update_.entity.server_defined_unique_tag.empty()) {
    for (UpdateResponseData& child_update :
         updates_per_parent_uuid_iter->second) {
      // To avoid double-counting later for bucket |kMissingParentEntity|,
      // clear the update from the list as if it would have been moved.
      child_update.entity = EntityData();
    }
    return node;
  }

  // Populate descendants recursively.
  node.children_.reserve(updates_per_parent_uuid_iter->second.size());
  for (UpdateResponseData& child_update :
       updates_per_parent_uuid_iter->second) {
    DCHECK_EQ(child_update.entity.specifics.notes().parent_guid(),
              uuid.AsLowercaseString());
    DCHECK(IsValidNotesSpecifics(child_update.entity.specifics.notes()));

    if ((node.update_.entity.specifics.notes().special_node_type() ==
             sync_pb::NotesSpecifics::FOLDER &&
         child_update.entity.specifics.notes().special_node_type() ==
             sync_pb::NotesSpecifics::ATTACHMENT) ||
        (node.update_.entity.specifics.notes().special_node_type() ==
             sync_pb::NotesSpecifics::NORMAL &&
         child_update.entity.specifics.notes().special_node_type() !=
             sync_pb::NotesSpecifics::ATTACHMENT)) {
      // Ignore children of the wrong type.
      child_update.entity = EntityData();
    } else {
      node.children_.push_back(BuildTree(std::move(child_update), max_depth - 1,
                                         updates_per_parent_uuid));
    }
  }

  // Sort the children according to their unique position.
  base::ranges::sort(node.children_, UniquePositionLessThan);

  return node;
}

NoteModelMerger::NoteModelMerger(UpdateResponseDataList updates,
                                 NoteModelView* notes_model,
                                 SyncedNoteTracker* note_tracker)
    : notes_model_(notes_model),
      note_tracker_(note_tracker),
      remote_forest_(BuildRemoteForest(std::move(updates), note_tracker)),
      uuid_to_match_map_(
          FindGuidMatchesOrReassignLocal(remote_forest_, notes_model_)) {
  CHECK(note_tracker_->IsEmpty());
  CHECK(notes_model);
  CHECK(notes_model->main_node());
  CHECK(notes_model->other_node());
  CHECK(notes_model->trash_node());
}

NoteModelMerger::~NoteModelMerger() = default;

void NoteModelMerger::Merge() {
  // Algorithm description:
  // Match up the roots and recursively do the following:
  // * For each remote node for the current remote (sync) parent node, either
  //   find a local node with equal UUID anywhere throughout the tree or find
  //   the best matching note node under the corresponding local note
  //   parent node using semantics. If the found node has the same UUID as a
  //   different remote note, we do not consider it a semantics match, as
  //   UUID matching takes precedence. If no matching node is found, create a
  //   new note node in the same position as the corresponding remote node.
  //   If a matching node is found, update the properties of it from the
  //   corresponding remote node.
  // * When all children remote nodes are done, add the extra children note
  //   nodes to the remote (sync) parent node, unless they will be later matched
  //   by UUID.
  //
  // The semantics best match algorithm uses folder title or note title, content
  // and url to perform the primary match. If there are multiple match
  // candidates it selects the first one.
  // Associate permanent folders.
  for (const auto& [server_defined_unique_tag, root] : remote_forest_) {
    DCHECK(!server_defined_unique_tag.empty());

    const vivaldi::NoteNode* permanent_folder =
        GetPermanentFolderForServerDefinedUniqueTag(notes_model_,
                                                    server_defined_unique_tag);

    // Ignore unsupported permanent folders.
    if (!permanent_folder) {
      DCHECK(!GetPermanentFolderUuidForServerDefinedUniqueTag(
                  server_defined_unique_tag)
                  .is_valid());
      continue;
    }

    DCHECK_EQ(permanent_folder->uuid(),
              GetPermanentFolderUuidForServerDefinedUniqueTag(
                  server_defined_unique_tag));
    MergeSubtree(/*local_node=*/permanent_folder,
                 /*remote_node=*/root);
  }

  if (base::FeatureList::IsEnabled(switches::kSyncReuploadBookmarks)) {
    // When the reupload feature is enabled, all new empty trackers are
    // automatically reuploaded (since there are no entities to reupload). This
    // is used to disable reupload after initial merge.
    note_tracker_->SetNotesReuploaded();
  }

  if (base::FeatureList::IsEnabled(
          switches::kSyncMigrateBookmarksWithoutClientTagHash)) {
    for (const auto& [server_defined_unique_tag, root] : remote_forest_) {
      MigrateNotesInSubtreeWithoutClientTagHash(root);
    }
  }
}

// static
NoteModelMerger::RemoteForest NoteModelMerger::BuildRemoteForest(
    syncer::UpdateResponseDataList updates,
    SyncedNoteTracker* tracker_for_recording_ignored_updates) {
  DCHECK(tracker_for_recording_ignored_updates);

  // Filter out invalid remote updates and group the valid ones by the server ID
  // of their parent.
  GroupedUpdates grouped_updates = GroupValidUpdates(std::move(updates));

  DeduplicateValidUpdatesByUuid(&grouped_updates.updates_per_parent_uuid);

  // Construct one tree per permanent entity.
  RemoteForest update_forest;
  for (UpdateResponseData& permanent_node_update :
       grouped_updates.permanent_node_updates) {
    // Make a copy of the string to avoid relying on argument evaluation order.
    const std::string server_defined_unique_tag =
        permanent_node_update.entity.server_defined_unique_tag;
    DCHECK(!server_defined_unique_tag.empty());

    update_forest.emplace(
        server_defined_unique_tag,
        RemoteTreeNode::BuildTree(std::move(permanent_node_update),
                                  kMaxNoteTreeDepth,
                                  &grouped_updates.updates_per_parent_uuid));
  }

  // All remaining entries in |updates_per_parent_uuid| must be unreachable from
  // permanent entities, since otherwise they would have been moved away.
  for (const auto& [parent_uuid, updates_for_uuid] :
       grouped_updates.updates_per_parent_uuid) {
    for (const UpdateResponseData& update : updates_for_uuid) {
      if (update.entity.specifics.has_notes()) {
        tracker_for_recording_ignored_updates
            ->RecordIgnoredServerUpdateDueToMissingParent(
                update.response_version);
      }
    }
  }

  return update_forest;
}

// static
std::unordered_map<base::Uuid, NoteModelMerger::GuidMatch, base::UuidHash>
NoteModelMerger::FindGuidMatchesOrReassignLocal(
    const RemoteForest& remote_forest,
    NoteModelView* notes_model) {
  DCHECK(notes_model);

  // Build a temporary lookup table for remote UUIDs.
  std::unordered_map<base::Uuid, const RemoteTreeNode*, base::UuidHash>
      uuid_to_remote_node_map;
  for (const auto& [server_defined_unique_tag, root] : remote_forest) {
    root.EmplaceSelfAndDescendantsByUuid(&uuid_to_remote_node_map);
  }

  // Iterate through all local notes to find matches by UUID.
  std::unordered_map<base::Uuid, NoteModelMerger::GuidMatch, base::UuidHash>
      uuid_to_match_map;
  // Because ReplaceNoteNodeUUID() cannot be used while iterating the local
  // notes model, a temporary list is constructed first to reassign later.
  std::vector<const vivaldi::NoteNode*> nodes_to_replace_uuid;
  ui::TreeNodeIterator<const vivaldi::NoteNode> iterator(
      notes_model->root_node());
  while (iterator.has_next()) {
    const vivaldi::NoteNode* const node = iterator.Next();
    DCHECK(node->uuid().is_valid());

    // Ignore changes to non-syncable nodes. Managed nodes, which are
    // unsyncable, use a random UUID so they should never match, but this
    // codepath is useful when NoteModelMerger is used together with
    // `NoteModelViewUsingAccountNodes`, which would otherwise match against
    // local nodes. (Doesn't actually exist in Vivaldi either, but doesn't hurt
    // to keep the code in sync with bookmarks.)
    if (!notes_model->IsNodeSyncable(node)) {
      continue;
    }

    const auto remote_it = uuid_to_remote_node_map.find(node->uuid());
    if (remote_it == uuid_to_remote_node_map.end()) {
      continue;
    }

    const RemoteTreeNode* const remote_node = remote_it->second;
    const syncer::EntityData& remote_entity = remote_node->entity();

    // Permanent nodes don't match by UUID but by |server_defined_unique_tag|.
    // As extra precaution, specially with remote UUIDs in mind, let's ignore
    // them explicitly here.
    DCHECK(remote_entity.server_defined_unique_tag.empty());
    if (node->is_permanent_node()) {
      continue;
    }

    if (GetProtoTypeFromNoteNode(node) !=
            remote_entity.specifics.notes().special_node_type() ||
        ((node->is_note() || node->is_attachment()) &&
         node->GetContent() !=
             base::UTF8ToUTF16(remote_entity.specifics.notes().content()))) {
      // If local node and its remote node match are conflicting in node type or
      // content, replace local UUID with a random UUID.
      nodes_to_replace_uuid.push_back(node);
      continue;
    }

    const bool success =
        uuid_to_match_map.emplace(node->uuid(), GuidMatch{node, remote_node})
            .second;

    // Insertion must have succeeded unless there were duplicate UUIDs in the
    // local NotesModel (invariant violation that gets resolved upon
    // restart).
    DCHECK(success);
  }

  for (const vivaldi::NoteNode* node : nodes_to_replace_uuid) {
    ReplaceNoteNodeUuid(node, base::Uuid::GenerateRandomV4(), notes_model);
  }

  return uuid_to_match_map;
}

void NoteModelMerger::MigrateNotesInSubtreeWithoutClientTagHash(
    const RemoteTreeNode& remote_node) {
  // Recursively iterate children first for simplicity, as the order doesn't
  // matter.
  for (const RemoteTreeNode& child : remote_node.children()) {
    MigrateNotesInSubtreeWithoutClientTagHash(child);
  }

  // Nothing to do for permanent folders.
  if (!remote_node.entity().server_defined_unique_tag.empty()) {
    return;
  }

  // Nothing to do if this entity already uses a client tag hash.
  if (!remote_node.entity().client_tag_hash.value().empty()) {
    return;
  }

  // Guaranteed by HasExpectedBookmarkGuid().
  CHECK(!remote_node.entity().originator_cache_guid.empty() ||
        !remote_node.entity().originator_client_item_id.empty());

  const SyncedNoteTrackerEntity* old_entity =
      note_tracker_->GetEntityForSyncId(remote_node.entity().id);
  CHECK(old_entity);
  CHECK(old_entity->note_node());

  const base::Time creation_time =
      syncer::ProtoTimeToTime(old_entity->metadata().creation_time());
  const syncer::UniquePosition pos = syncer::UniquePosition::FromProto(
      old_entity->metadata().unique_position());

  const vivaldi::NoteNode* node = old_entity->note_node();
  note_tracker_->MarkDeleted(old_entity, FROM_HERE);
  note_tracker_->IncrementSequenceNumber(old_entity);

  // TODO(crbug.com/376641665): Consider generating new UUIDs deterministically
  // rather than randomly to guard against concurrent clients or interrupted
  // migrations.
  const base::Uuid new_guid = base::Uuid::GenerateRandomV4();
  node = ReplaceNoteNodeUuid(node, new_guid, notes_model_);

  const sync_pb::EntitySpecifics specifics =
      CreateSpecificsFromNoteNode(node, notes_model_, pos.ToProto());

  const SyncedNoteTrackerEntity* new_entity =
      note_tracker_->Add(node, /*sync_id=*/new_guid.AsLowercaseString(),
                         syncer::kUncommittedVersion, creation_time, specifics);

  // Mark the entity that it needs to be committed.
  note_tracker_->IncrementSequenceNumber(new_entity);

  // Make sure all direct children are marked for commit, because their parent
  // changed.
  for (const RemoteTreeNode& child : remote_node.children()) {
    const SyncedNoteTrackerEntity* child_entity =
        note_tracker_->GetEntityForSyncId(child.entity().id);
    CHECK(child_entity);
    note_tracker_->IncrementSequenceNumber(child_entity);
  }
}

void NoteModelMerger::MergeSubtree(const vivaldi::NoteNode* local_subtree_root,
                                   const RemoteTreeNode& remote_node) {
  const EntityData& remote_update_entity = remote_node.entity();
  const SyncedNoteTrackerEntity* entity = note_tracker_->Add(
      local_subtree_root, remote_update_entity.id,
      remote_node.response_version(), remote_update_entity.creation_time,
      remote_update_entity.specifics);
  const bool is_reupload_needed =
      !local_subtree_root->is_permanent_node() &&
      IsNoteEntityReuploadNeeded(remote_update_entity);
  if (is_reupload_needed) {
    note_tracker_->IncrementSequenceNumber(entity);
  }

  // If there are remote child updates, try to match them.
  for (size_t remote_index = 0; remote_index < remote_node.children().size();
       ++remote_index) {
    // TODO(crbug.com/40118203): change to DCHECK after investigating.
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
    matching_local_node = UpdateNoteNodeFromSpecificsIncludingUuid(
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
    // If local node has been or will be matched by UUID, skip it.
    if (FindMatchingRemoteNodeByUuid(local_subtree_root->children()[i].get())) {
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
  // Try to match child by UUID. If we can't, try to match child by semantics.
  const vivaldi::NoteNode* matching_local_node_by_uuid =
      FindMatchingLocalNodeByUuid(remote_child);
  if (matching_local_node_by_uuid) {
    return matching_local_node_by_uuid;
  }

  // All local nodes up to |remote_index-1| have processed already. Look for a
  // matching local node starting with the local node at position
  // |local_child_start_index|. FindMatchingChildBySemanticsStartingAt()
  // returns kInvalidIndex in the case where no semantics match was found or
  // the semantics match found is UUID-matchable to a different node.
  const size_t local_index = FindMatchingChildBySemanticsStartingAt(
      /*remote_node=*/remote_child,
      /*local_parent=*/local_parent,
      /*starting_child_index=*/local_child_start_index);
  if (local_index == kInvalidIndex) {
    // If no match found, return.
    return nullptr;
  }

  // The child at |local_index| has matched by semantics, which also means it
  // does not match by UUID to any other remote node.
  const vivaldi::NoteNode* matching_local_node_by_semantics =
      local_parent->children()[local_index].get();
  DCHECK(!FindMatchingRemoteNodeByUuid(matching_local_node_by_semantics));
  return matching_local_node_by_semantics;
}

const vivaldi::NoteNode*
NoteModelMerger::UpdateNoteNodeFromSpecificsIncludingUuid(
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

  // Update the local UUID if necessary for semantic matches (it's obviously not
  // needed for UUID-based matches).
  const vivaldi::NoteNode* possibly_replaced_local_node = local_node;
  if (!specifics.guid().empty() &&
      specifics.guid() != local_node->uuid().AsLowercaseString()) {
    // If it's a semantic match, neither of the nodes should be involved in any
    // UUID-based match.
    DCHECK(!FindMatchingLocalNodeByUuid(remote_node));
    DCHECK(!FindMatchingRemoteNodeByUuid(local_node));

    possibly_replaced_local_node = ReplaceNoteNodeUuid(
        local_node, base::Uuid::ParseLowercase(specifics.guid()), notes_model_);

    // TODO(rushans): remove the code below since DCHECKs above guarantee that
    // |uuid_to_match_map_| has no such UUID.
    //
    // Update |uuid_to_match_map_| to avoid pointing to a deleted node. This
    // should not be required in practice, because the algorithm processes each
    // UUID once, but let's update nevertheless to avoid future issues.
    const auto it =
        uuid_to_match_map_.find(possibly_replaced_local_node->uuid());
    if (it != uuid_to_match_map_.end() && it->second.local_node == local_node) {
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
  DCHECK(!FindMatchingLocalNodeByUuid(remote_node));

  const EntityData& remote_update_entity = remote_node.entity();
  DCHECK(IsValidNotesSpecifics(remote_update_entity.specifics.notes()));

  const sync_pb::EntitySpecifics& specifics = remote_node.entity().specifics;
  const vivaldi::NoteNode* note_node = CreateNoteNodeFromSpecifics(
      specifics.notes(), local_parent, index, notes_model_);
  DCHECK(note_node);
  const SyncedNoteTrackerEntity* entity = note_tracker_->Add(
      note_node, remote_update_entity.id, remote_node.response_version(),
      remote_update_entity.creation_time, specifics);
  const bool is_reupload_needed =
      IsNoteEntityReuploadNeeded(remote_node.entity());
  if (is_reupload_needed) {
    note_tracker_->IncrementSequenceNumber(entity);
  }

  // Recursively, match by UUID or, if not possible, create local node for all
  // child remote nodes.
  size_t i = 0;
  for (const RemoteTreeNode& remote_child : remote_node.children()) {
    // TODO(crbug.com/40118203): change to DCHECK after investigating of some
    // crashes.
    CHECK_LE(i, note_node->children().size());
    const vivaldi::NoteNode* local_child =
        FindMatchingLocalNodeByUuid(remote_child);
    if (!local_child) {
      ProcessRemoteCreation(remote_child, note_node, i++);
      continue;
    }
    notes_model_->Move(local_child, note_node, i++);
    local_child =
        UpdateNoteNodeFromSpecificsIncludingUuid(local_child, remote_child);
    MergeSubtree(local_child, remote_child);
  }
}

void NoteModelMerger::ProcessLocalCreation(const vivaldi::NoteNode* parent,
                                           size_t index) {
  DCHECK_LE(index, parent->children().size());
  const SyncedNoteTrackerEntity* parent_entity =
      note_tracker_->GetEntityForNoteNode(parent);
  // Since we are merging top down, parent entity must be tracked.
  DCHECK(parent_entity);

  // Assign a temp server id for the entity. Will be overridden by the actual
  // server id upon receiving commit response.
  const vivaldi::NoteNode* node = parent->children()[index].get();
  DCHECK(!FindMatchingRemoteNodeByUuid(node));

  // The node's UUID cannot run into collisions because
  // FindGuidMatchesOrReassignLocal() takes care of reassigning local UUIDs if
  // they won't actually be merged with the remote note with the same UUID
  // (e.g. incompatible types).
  const base::Time creation_time = base::Time::Now();
  const syncer::UniquePosition::Suffix suffix =
      syncer::UniquePosition::GenerateSuffix(
          SyncedNoteTracker::GetClientTagHashFromUuid(node->uuid()));
  // Locally created nodes aren't tracked and hence don't have a unique position
  // yet so we need to produce new ones.
  const syncer::UniquePosition pos =
      GenerateUniquePositionForLocalCreation(parent, index, suffix);
  const sync_pb::EntitySpecifics specifics =
      CreateSpecificsFromNoteNode(node, notes_model_, pos.ToProto());
  const SyncedNoteTrackerEntity* entity =
      note_tracker_->Add(node, /*sync_id=*/node->uuid().AsLowercaseString(),
                         syncer::kUncommittedVersion, creation_time, specifics);
  // Mark the entity that it needs to be committed.
  note_tracker_->IncrementSequenceNumber(entity);
  for (size_t i = 0; i < node->children().size(); ++i) {
    // If a local node hasn't matched with any remote entity, its descendants
    // will neither, unless they have been or will be matched by UUID, in which
    // case we skip them for now.
    if (FindMatchingRemoteNodeByUuid(node->children()[i].get())) {
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

  // Precompute the remote title, content  and URL before searching for a
  // matching local node.
  const std::string remote_canonicalized_title =
      LegacyCanonicalizedTitleFromSpecifics(remote_entity.specifics.notes());
  const sync_pb::NotesSpecifics::VivaldiSpecialNotesType remote_type =
      remote_entity.specifics.notes().special_node_type();
  GURL remote_url;
  std::u16string remote_content;
  if (remote_type == sync_pb::NotesSpecifics::NORMAL ||
      remote_type == sync_pb::NotesSpecifics::ATTACHMENT) {
    remote_url = GURL(remote_entity.specifics.notes().url());
    remote_content =
        base::UTF8ToUTF16(remote_entity.specifics.notes().content());
  }
  const auto it = std::find_if(
      children.cbegin() + starting_child_index, children.cend(),
      [this, &remote_canonicalized_title, &remote_url, &remote_content,
       remote_type](const auto& child) {
        return !FindMatchingRemoteNodeByUuid(child.get()) &&
               NodeSemanticsMatch(child.get(), remote_canonicalized_title,
                                  remote_url, remote_content, remote_type);
      });
  return (it == children.cend()) ? kInvalidIndex : (it - children.cbegin());
}

const NoteModelMerger::RemoteTreeNode*
NoteModelMerger::FindMatchingRemoteNodeByUuid(
    const vivaldi::NoteNode* local_node) const {
  DCHECK(local_node);

  const auto it = uuid_to_match_map_.find(local_node->uuid());
  if (it == uuid_to_match_map_.end()) {
    return nullptr;
  }

  DCHECK_EQ(it->second.local_node, local_node);
  return it->second.remote_node;
}

const vivaldi::NoteNode* NoteModelMerger::FindMatchingLocalNodeByUuid(
    const RemoteTreeNode& remote_node) const {
  const syncer::EntityData& remote_entity = remote_node.entity();
  const auto it = uuid_to_match_map_.find(
      base::Uuid::ParseLowercase(remote_entity.specifics.notes().guid()));
  if (it == uuid_to_match_map_.end()) {
    return nullptr;
  }

  DCHECK_EQ(it->second.remote_node, &remote_node);
  return it->second.local_node;
}

syncer::UniquePosition NoteModelMerger::GenerateUniquePositionForLocalCreation(
    const vivaldi::NoteNode* parent,
    size_t index,
    const syncer::UniquePosition::Suffix& suffix) const {
  // Try to find last tracked preceding entity. It is not always the previous
  // one as it might be skipped if it has unprocessed remote matching by UUID
  // update.
  for (size_t i = index; i > 0; --i) {
    const SyncedNoteTrackerEntity* predecessor_entity =
        note_tracker_->GetEntityForNoteNode(parent->children()[i - 1].get());
    if (predecessor_entity != nullptr) {
      return syncer::UniquePosition::After(
          syncer::UniquePosition::FromProto(
              predecessor_entity->metadata().unique_position()),
          suffix);
    }
    DCHECK(FindMatchingRemoteNodeByUuid(parent->children()[i - 1].get()));
  }
  return syncer::UniquePosition::InitialPosition(suffix);
}

}  // namespace sync_notes
