// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notes/synced_note_tracker.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "app/vivaldi_apptools.h"
#include "base/base64.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/uuid.h"
#include "components/notes/note_node.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/protocol/data_type_state_helper.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/notes_model_metadata.pb.h"
#include "components/sync/protocol/proto_memory_estimations.h"
#include "components/sync_bookmarks/switches.h"
#include "components/version_info/version_info.h"
#include "sync/file_sync/file_store.h"
#include "sync/notes/note_model_view.h"
#include "sync/notes/synced_note_tracker_entity.h"
#include "ui/base/models/tree_node_iterator.h"

namespace sync_notes {

namespace {

void HashSpecifics(const sync_pb::EntitySpecifics& specifics,
                   std::string* hash) {
  DCHECK_GT(specifics.ByteSize(), 0);
  *hash =
      base::Base64Encode(base::SHA1HashString(specifics.SerializeAsString()));
}

// Returns a map from id to node for all nodes in |model|.
std::unordered_map<int64_t, const vivaldi::NoteNode*> BuildIdToNoteNodeMap(
    const NoteModelView* model) {
  std::unordered_map<int64_t, const vivaldi::NoteNode*> id_to_note_node_map;

  // The TreeNodeIterator used below doesn't include the node itself, and hence
  // add the root node separately.
  id_to_note_node_map[model->root_node()->id()] = model->root_node();

  ui::TreeNodeIterator<const vivaldi::NoteNode> iterator(model->root_node());
  while (iterator.has_next()) {
    const vivaldi::NoteNode* node = iterator.Next();
    id_to_note_node_map[node->id()] = node;
  }
  return id_to_note_node_map;
}
}  // namespace

// static
syncer::ClientTagHash SyncedNoteTracker::GetClientTagHashFromUuid(
    const base::Uuid& uuid) {
  // Earlier vivaldi versions were mistakenly using the BOOKMARKS type to verify
  // the type, so we temporarily produce tags using the BOOKMARKS type.
  // Change this to NOTES in a few version. 07-2021
  return syncer::ClientTagHash::FromUnhashed(syncer::BOOKMARKS,
                                             uuid.AsLowercaseString());
}

// static
std::unique_ptr<SyncedNoteTracker> SyncedNoteTracker::CreateEmpty(
    sync_pb::DataTypeState data_type_state,
    file_sync::SyncedFileStore* synced_file_store) {
  // base::WrapUnique() used because the constructor is private.
  return base::WrapUnique(new SyncedNoteTracker(
      std::move(data_type_state), /*notes_reuploaded=*/false,
      /*num_ignored_updates_due_to_missing_parent=*/std::optional<int64_t>(0),
      /*max_version_among_ignored_updates_due_to_missing_parent=*/
      std::nullopt, synced_file_store));
}

// static
std::unique_ptr<SyncedNoteTracker>
SyncedNoteTracker::CreateFromNotesModelAndMetadata(
    const NoteModelView* model,
    sync_pb::NotesModelMetadata model_metadata,
    file_sync::SyncedFileStore* synced_file_store) {
  DCHECK(model);

  if (!syncer::IsInitialSyncDone(
          model_metadata.data_type_state().initial_sync_state())) {
    return nullptr;
  }

  if (!model_metadata.notes_reset_for_attachment_suport()) {
    // When updating from a version of vivaldi that didn't support attachment,
    // we need to redownload all notes. This is because those older versions
    // would just throw away all updates to attachment nodes, since they were
    // not children of folders. The only way to ensure we receive those again
    // from the server is to request everything.
    return nullptr;
  }

  // When the reupload feature is enabled and disabled again, there may occur
  // new entities which weren't reuploaded.
  const bool notes_reuploaded =
      model_metadata.notes_hierarchy_fields_reuploaded() &&
      base::FeatureList::IsEnabled(switches::kSyncReuploadBookmarks);

  std::optional<int64_t> num_ignored_updates_due_to_missing_parent;
  if (model_metadata.has_num_ignored_updates_due_to_missing_parent()) {
    num_ignored_updates_due_to_missing_parent =
        model_metadata.num_ignored_updates_due_to_missing_parent();
  }

  std::optional<int64_t>
      max_version_among_ignored_updates_due_to_missing_parent;
  if (model_metadata
          .has_max_version_among_ignored_updates_due_to_missing_parent()) {
    max_version_among_ignored_updates_due_to_missing_parent =
        model_metadata
            .max_version_among_ignored_updates_due_to_missing_parent();
  }

  // base::WrapUnique() used because the constructor is private.
  auto tracker = base::WrapUnique(new SyncedNoteTracker(
      model_metadata.data_type_state(), notes_reuploaded,
      num_ignored_updates_due_to_missing_parent,
      max_version_among_ignored_updates_due_to_missing_parent,
      synced_file_store));

  bool is_not_corrupted = tracker->InitEntitiesFromModelAndMetadata(
      model, std::move(model_metadata));

  if (!is_not_corrupted) {
    return nullptr;
  }

  return tracker;
}

SyncedNoteTracker::~SyncedNoteTracker() = default;

void SyncedNoteTracker::SetNotesReuploaded() {
  notes_reuploaded_ = true;
}

const SyncedNoteTrackerEntity* SyncedNoteTracker::GetEntityForSyncId(
    const std::string& sync_id) const {
  auto it = sync_id_to_entities_map_.find(sync_id);
  return it != sync_id_to_entities_map_.end() ? it->second.get() : nullptr;
}

const SyncedNoteTrackerEntity* SyncedNoteTracker::GetEntityForClientTagHash(
    const syncer::ClientTagHash& client_tag_hash) const {
  auto it = client_tag_hash_to_entities_map_.find(client_tag_hash);
  return it != client_tag_hash_to_entities_map_.end() ? it->second : nullptr;
}

const SyncedNoteTrackerEntity* SyncedNoteTracker::GetEntityForUuid(
    const base::Uuid& uuid) const {
  return GetEntityForClientTagHash(GetClientTagHashFromUuid(uuid));
}

SyncedNoteTrackerEntity* SyncedNoteTracker::AsMutableEntity(
    const SyncedNoteTrackerEntity* entity) {
  DCHECK(entity);
  DCHECK_EQ(entity, GetEntityForSyncId(entity->metadata().server_id()));

  // As per DCHECK above, this tracker owns |*entity|, so it's legitimate to
  // return non-const pointer.
  return const_cast<SyncedNoteTrackerEntity*>(entity);
}

const SyncedNoteTrackerEntity* SyncedNoteTracker::GetEntityForNoteNode(
    const vivaldi::NoteNode* node) const {
  auto it = note_node_to_entities_map_.find(node);
  return it != note_node_to_entities_map_.end() ? it->second : nullptr;
}

const SyncedNoteTrackerEntity* SyncedNoteTracker::Add(
    const vivaldi::NoteNode* note_node,
    const std::string& sync_id,
    int64_t server_version,
    base::Time creation_time,
    const sync_pb::EntitySpecifics& specifics) {
  DCHECK_GT(specifics.ByteSize(), 0);
  DCHECK(note_node);
  DCHECK(specifics.has_notes());
  DCHECK(note_node->is_permanent_node() ||
         specifics.notes().has_unique_position());

  // Note that this gets computed for permanent nodes too.
  syncer::ClientTagHash client_tag_hash =
      GetClientTagHashFromUuid(note_node->uuid());

  sync_pb::EntityMetadata metadata;
  metadata.set_is_deleted(false);
  metadata.set_server_id(sync_id);
  metadata.set_server_version(server_version);
  metadata.set_creation_time(syncer::TimeToProtoTime(creation_time));
  metadata.set_modification_time(syncer::TimeToProtoTime(creation_time));
  metadata.set_sequence_number(0);
  metadata.set_acked_sequence_number(0);
  *metadata.mutable_unique_position() = specifics.notes().unique_position();
  metadata.set_client_tag_hash(client_tag_hash.value());
  HashSpecifics(specifics, metadata.mutable_specifics_hash());
  auto entity =
      std::make_unique<SyncedNoteTrackerEntity>(note_node, std::move(metadata));

  CHECK_EQ(0U, note_node_to_entities_map_.count(note_node));
  note_node_to_entities_map_[note_node] = entity.get();
  CHECK_EQ(0U, client_tag_hash_to_entities_map_.count(client_tag_hash));
  client_tag_hash_to_entities_map_.emplace(std::move(client_tag_hash),
                                           entity.get());
  DCHECK_EQ(0U, sync_id_to_entities_map_.count(sync_id));
  const SyncedNoteTrackerEntity* raw_entity = entity.get();
  sync_id_to_entities_map_[sync_id] = std::move(entity);
  DCHECK_EQ(sync_id_to_entities_map_.size(),
            client_tag_hash_to_entities_map_.size());
  if (server_version != syncer::kUncommittedVersion &&
      note_node->is_attachment()) {
    synced_file_store_->SetSyncFileRef(
        sync_id, syncer::NOTES, base::UTF16ToASCII(note_node->GetContent()));
  }
  return raw_entity;
}

void SyncedNoteTracker::Update(const SyncedNoteTrackerEntity* entity,
                               int64_t server_version,
                               base::Time modification_time,
                               const sync_pb::EntitySpecifics& specifics) {
  DCHECK_GT(specifics.ByteSize(), 0);
  DCHECK(entity);
  DCHECK(specifics.has_notes());
  DCHECK(specifics.notes().has_unique_position());

  SyncedNoteTrackerEntity* mutable_entity = AsMutableEntity(entity);
  mutable_entity->MutableMetadata()->set_server_version(server_version);
  mutable_entity->MutableMetadata()->set_modification_time(
      syncer::TimeToProtoTime(modification_time));
  *mutable_entity->MutableMetadata()->mutable_unique_position() =
      specifics.notes().unique_position();
  HashSpecifics(specifics,
                mutable_entity->MutableMetadata()->mutable_specifics_hash());
}

void SyncedNoteTracker::UpdateServerVersion(
    const SyncedNoteTrackerEntity* entity,
    int64_t server_version) {
  DCHECK(entity);
  AsMutableEntity(entity)->MutableMetadata()->set_server_version(
      server_version);
}

void SyncedNoteTracker::MarkCommitMayHaveStarted(
    const SyncedNoteTrackerEntity* entity) {
  DCHECK(entity);
  AsMutableEntity(entity)->set_commit_may_have_started(true);
}

void SyncedNoteTracker::MarkDeleted(const SyncedNoteTrackerEntity* entity,
                                    const base::Location& location) {
  DCHECK(entity);
  DCHECK(!entity->metadata().is_deleted());
  DCHECK(entity->note_node());
  DCHECK_EQ(1U, note_node_to_entities_map_.count(entity->note_node()));

  SyncedNoteTrackerEntity* mutable_entity = AsMutableEntity(entity);
  mutable_entity->MutableMetadata()->set_is_deleted(true);
  *mutable_entity->MutableMetadata()->mutable_deletion_origin() =
      syncer::DeletionOrigin::FromLocation(location).ToProto(
          version_info::GetVersionNumber());

  // Clear all references to the deleted note node.
  note_node_to_entities_map_.erase(mutable_entity->note_node());
  mutable_entity->clear_note_node();
  DCHECK_EQ(0, base::ranges::count(ordered_local_tombstones_, entity));
  ordered_local_tombstones_.push_back(mutable_entity);
}

void SyncedNoteTracker::Remove(const SyncedNoteTrackerEntity* entity) {
  DCHECK(entity);
  DCHECK_EQ(entity, GetEntityForSyncId(entity->metadata().server_id()));
  DCHECK_EQ(entity, GetEntityForClientTagHash(entity->GetClientTagHash()));
  DCHECK_EQ(sync_id_to_entities_map_.size(),
            client_tag_hash_to_entities_map_.size());

  if (entity->note_node()) {
    DCHECK(!entity->metadata().is_deleted());
    DCHECK_EQ(0, std::count(ordered_local_tombstones_.begin(),
                            ordered_local_tombstones_.end(), entity));
    note_node_to_entities_map_.erase(entity->note_node());
  } else {
    DCHECK(entity->metadata().is_deleted());
  }

  // We don't need to check if this is an attachment. If it isn't, there will
  // just be nothing to remove for the provided sync id.
  synced_file_store_->RemoveSyncRef(entity->metadata().server_id(),
                                    syncer::NOTES);

  client_tag_hash_to_entities_map_.erase(entity->GetClientTagHash());

  std::erase(ordered_local_tombstones_, entity);
  sync_id_to_entities_map_.erase(entity->metadata().server_id());
  DCHECK_EQ(sync_id_to_entities_map_.size(),
            client_tag_hash_to_entities_map_.size());
}

void SyncedNoteTracker::IncrementSequenceNumber(
    const SyncedNoteTrackerEntity* entity) {
  DCHECK(entity);
  DCHECK(!entity->note_node() || !entity->note_node()->is_permanent_node());

  AsMutableEntity(entity)->MutableMetadata()->set_sequence_number(
      entity->metadata().sequence_number() + 1);
}

sync_pb::NotesModelMetadata SyncedNoteTracker::BuildNoteModelMetadata() const {
  sync_pb::NotesModelMetadata model_metadata;
  model_metadata.set_notes_hierarchy_fields_reuploaded(notes_reuploaded_);

  if (num_ignored_updates_due_to_missing_parent_.has_value()) {
    model_metadata.set_num_ignored_updates_due_to_missing_parent(
        *num_ignored_updates_due_to_missing_parent_);
  }

  if (max_version_among_ignored_updates_due_to_missing_parent_.has_value()) {
    model_metadata.set_max_version_among_ignored_updates_due_to_missing_parent(
        *max_version_among_ignored_updates_due_to_missing_parent_);
  }

  for (const auto& [sync_id, entity] : sync_id_to_entities_map_) {
    DCHECK(entity) << " for ID " << sync_id;
    if (entity->metadata().is_deleted()) {
      // Deletions will be added later because they need to maintain the same
      // order as in |ordered_local_tombstones_|.
      continue;
    }
    DCHECK(entity->note_node());
    sync_pb::NoteMetadata* note_metadata = model_metadata.add_notes_metadata();
    note_metadata->set_id(entity->note_node()->id());
    *note_metadata->mutable_metadata() = entity->metadata();
  }
  // Add pending deletions.
  for (const SyncedNoteTrackerEntity* tombstone_entity :
       ordered_local_tombstones_) {
    DCHECK(tombstone_entity);
    DCHECK(tombstone_entity->metadata().is_deleted());
    sync_pb::NoteMetadata* note_metadata = model_metadata.add_notes_metadata();
    *note_metadata->mutable_metadata() = tombstone_entity->metadata();
  }
  *model_metadata.mutable_data_type_state() = data_type_state_;
  // This is always true for all trackers that were allowed to initialize.
  model_metadata.set_notes_reset_for_attachment_suport(true);
  return model_metadata;
}

bool SyncedNoteTracker::HasLocalChanges() const {
  for (const auto& [sync_id, entity] : sync_id_to_entities_map_) {
    if (entity->IsUnsynced()) {
      return true;
    }
  }
  return false;
}

std::vector<const SyncedNoteTrackerEntity*> SyncedNoteTracker::GetAllEntities()
    const {
  std::vector<const SyncedNoteTrackerEntity*> entities;
  for (const auto& [sync_id, entity] : sync_id_to_entities_map_) {
    entities.push_back(entity.get());
  }
  return entities;
}

std::vector<const SyncedNoteTrackerEntity*>
SyncedNoteTracker::GetEntitiesWithLocalChanges() const {
  std::vector<const SyncedNoteTrackerEntity*> entities_with_local_changes;
  // Entities with local non deletions should be sorted such that parent
  // creation/update comes before child creation/update.
  for (const auto& [sync_id, entity] : sync_id_to_entities_map_) {
    if (entity->metadata().is_deleted()) {
      // Deletions are stored sorted in |ordered_local_tombstones_| and will be
      // added later.
      continue;
    }
    if (entity->IsUnsynced()) {
      entities_with_local_changes.push_back(entity.get());
    }
  }
  std::vector<const SyncedNoteTrackerEntity*> ordered_local_changes =
      ReorderUnsyncedEntitiesExceptDeletions(entities_with_local_changes);

  for (const SyncedNoteTrackerEntity* tombstone_entity :
       ordered_local_tombstones_) {
    DCHECK_EQ(0, base::ranges::count(ordered_local_changes, tombstone_entity));
    ordered_local_changes.push_back(tombstone_entity);
  }
  return ordered_local_changes;
}

SyncedNoteTracker::SyncedNoteTracker(
    sync_pb::DataTypeState data_type_state,
    bool notes_reuploaded,
    std::optional<int64_t> num_ignored_updates_due_to_missing_parent,
    std::optional<int64_t>
        max_version_among_ignored_updates_due_to_missing_parent,
    file_sync::SyncedFileStore* synced_file_store)
    : synced_file_store_(synced_file_store),
      data_type_state_(std::move(data_type_state)),
      notes_reuploaded_(notes_reuploaded),
      num_ignored_updates_due_to_missing_parent_(
          num_ignored_updates_due_to_missing_parent),
      max_version_among_ignored_updates_due_to_missing_parent_(
          max_version_among_ignored_updates_due_to_missing_parent) {}

bool SyncedNoteTracker::InitEntitiesFromModelAndMetadata(
    const NoteModelView* model,
    sync_pb::NotesModelMetadata model_metadata) {
  DCHECK(syncer::IsInitialSyncDone(data_type_state_.initial_sync_state()));

  // Build a temporary map to look up note nodes efficiently by node ID.
  std::unordered_map<int64_t, const vivaldi::NoteNode*> id_to_note_node_map =
      BuildIdToNoteNodeMap(model);

  for (sync_pb::NoteMetadata& note_metadata :
       *model_metadata.mutable_notes_metadata()) {
    if (!note_metadata.metadata().has_server_id()) {
      DLOG(ERROR) << "Error when decoding sync metadata: Entities must contain "
                     "server id.";
      return false;
    }

    const std::string sync_id = note_metadata.metadata().server_id();
    if (sync_id_to_entities_map_.count(sync_id) != 0) {
      DLOG(ERROR) << "Error when decoding sync metadata: Duplicated server id.";
      return false;
    }

    // Handle tombstones.
    if (note_metadata.metadata().is_deleted()) {
      if (note_metadata.has_id()) {
        DLOG(ERROR) << "Error when decoding sync metadata: Tombstones "
                       "shouldn't have a note id.";
        return false;
      }

      if (!note_metadata.metadata().has_client_tag_hash()) {
        DLOG(ERROR) << "Error when decoding sync metadata: "
                    << "Tombstone client tag hash is missing.";
        return false;
      }

      const syncer::ClientTagHash client_tag_hash =
          syncer::ClientTagHash::FromHashed(
              note_metadata.metadata().client_tag_hash());

      auto tombstone_entity = std::make_unique<SyncedNoteTrackerEntity>(
          /*node=*/nullptr, std::move(*note_metadata.mutable_metadata()));

      if (!client_tag_hash_to_entities_map_
               .emplace(client_tag_hash, tombstone_entity.get())
               .second) {
        DLOG(ERROR) << "Error when decoding sync metadata: Duplicated client "
                       "tag hash.";
        return false;
      }

      ordered_local_tombstones_.push_back(tombstone_entity.get());
      DCHECK_EQ(0U, sync_id_to_entities_map_.count(sync_id));
      sync_id_to_entities_map_[sync_id] = std::move(tombstone_entity);
      DCHECK_EQ(sync_id_to_entities_map_.size(),
                client_tag_hash_to_entities_map_.size());
      continue;
    }

    // Non-tombstones.
    DCHECK(!note_metadata.metadata().is_deleted());

    if (!note_metadata.has_id()) {
      DLOG(ERROR) << "Error when decoding sync metadata: Note id is missing.";
      return false;
    }

    const vivaldi::NoteNode* node = id_to_note_node_map[note_metadata.id()];

    if (!node) {
      DLOG(ERROR) << "Error when decoding sync metadata: unknown Note id.";
      return false;
    }

    // Note that currently the client tag hash is persisted for permanent nodes
    // too, although it's irrelevant (and even subject to change value upon
    // restart if the code changes).
    if (!note_metadata.metadata().has_client_tag_hash() &&
        !node->is_permanent_node()) {
      DLOG(ERROR) << "Error when decoding sync metadata: "
                  << "Note client tag hash is missing.";
      return false;
    }

    // The client-tag-hash is expected to be equal to the hash of the note's
    // UUID. This can be hit for example if local note UUIDs were
    // reassigned upon startup due to duplicates (which is a NoteModel
    // invariant violation and should be impossible).
    const syncer::ClientTagHash client_tag_hash =
        GetClientTagHashFromUuid(node->uuid());
    if (client_tag_hash != syncer::ClientTagHash::FromHashed(
                               note_metadata.metadata().client_tag_hash())) {
      if (node->is_permanent_node()) {
        // For permanent nodes the client tag hash is irrelevant and subject to
        // change if the constants in notes change and adopt
        // different UUID constants. To avoid treating such state as corrupt
        // metadata, let's fix it automatically.
        note_metadata.mutable_metadata()->set_client_tag_hash(
            client_tag_hash.value());
      } else {
        DLOG(ERROR) << "Note Uuid does not match the client tag.";
        return false;
      }
    }

    auto entity = std::make_unique<SyncedNoteTrackerEntity>(
        node, std::move(*note_metadata.mutable_metadata()));

    if (!client_tag_hash_to_entities_map_.emplace(client_tag_hash, entity.get())
             .second) {
      DLOG(ERROR) << "Error when decoding sync metadata: Duplicated client "
                     "tag hash.";
      return false;
    }

    entity->set_commit_may_have_started(true);
    CHECK_EQ(0U, note_node_to_entities_map_.count(node));
    note_node_to_entities_map_[node] = entity.get();
    DCHECK_EQ(0U, sync_id_to_entities_map_.count(sync_id));
    sync_id_to_entities_map_[sync_id] = std::move(entity);
    DCHECK_EQ(sync_id_to_entities_map_.size(),
              client_tag_hash_to_entities_map_.size());
  }

  // See if there are untracked entities in the NotesModel.
  std::vector<int> model_node_ids;
  ui::TreeNodeIterator<const vivaldi::NoteNode> iterator(model->root_node());
  while (iterator.has_next()) {
    const vivaldi::NoteNode* node = iterator.Next();
    if (!model->IsNodeSyncable(node)) {
      continue;
    }
    if (note_node_to_entities_map_.count(node) == 0) {
      return false;
    }
  }

  CheckAllNodesTracked(model);
  return true;
}

std::vector<const SyncedNoteTrackerEntity*>
SyncedNoteTracker::ReorderUnsyncedEntitiesExceptDeletions(
    const std::vector<const SyncedNoteTrackerEntity*>& entities) const {
  // This method sorts the entities with local non deletions such that parent
  // creation/update comes before child creation/update.

  // The algorithm works by constructing a forest of all non-deletion updates
  // and then traverses each tree in the forest recursively:
  // 1. Iterate over all entities and collect all nodes in |nodes|.
  // 2. Iterate over all entities again and node that is a child of another
  //    node. What's left in |nodes| are the roots of the forest.
  // 3. Start at each root in |nodes|, emit the update and recurse over its
  //    children.
  std::unordered_set<const vivaldi::NoteNode*> nodes;
  // Collect nodes with updates
  for (const SyncedNoteTrackerEntity* entity : entities) {
    DCHECK(entity->IsUnsynced());
    DCHECK(!entity->metadata().is_deleted());
    DCHECK(entity->note_node());
    nodes.insert(entity->note_node());
  }
  // Remove those who are direct children of another node.
  for (const SyncedNoteTrackerEntity* entity : entities) {
    const vivaldi::NoteNode* node = entity->note_node();
    for (const auto& child : node->children()) {
      nodes.erase(child.get());
    }
  }
  // |nodes| contains only roots of all trees in the forest all of which are
  // ready to be processed because their parents have no pending updates.
  std::vector<const SyncedNoteTrackerEntity*> ordered_entities;
  for (const vivaldi::NoteNode* node : nodes) {
    TraverseAndAppend(node, &ordered_entities);
  }
  return ordered_entities;
}

bool SyncedNoteTracker::ReuploadNotesOnLoadIfNeeded() {
  if (notes_reuploaded_ ||
      !base::FeatureList::IsEnabled(switches::kSyncReuploadBookmarks)) {
    return false;
  }
  for (const auto& [sync_id, entity] : sync_id_to_entities_map_) {
    if (entity->IsUnsynced() || entity->metadata().is_deleted()) {
      continue;
    }
    if (entity->note_node()->is_permanent_node()) {
      continue;
    }
    IncrementSequenceNumber(entity.get());
  }
  SetNotesReuploaded();
  return true;
}

void SyncedNoteTracker::RecordIgnoredServerUpdateDueToMissingParent(
    int64_t server_version) {
  if (num_ignored_updates_due_to_missing_parent_.has_value()) {
    ++(*num_ignored_updates_due_to_missing_parent_);
  }

  if (max_version_among_ignored_updates_due_to_missing_parent_.has_value()) {
    *max_version_among_ignored_updates_due_to_missing_parent_ =
        std::max(*max_version_among_ignored_updates_due_to_missing_parent_,
                 server_version);
  } else {
    max_version_among_ignored_updates_due_to_missing_parent_ = server_version;
  }
}

std::optional<int64_t>
SyncedNoteTracker::GetNumIgnoredUpdatesDueToMissingParentForTest() const {
  return num_ignored_updates_due_to_missing_parent_;
}

std::optional<int64_t>
SyncedNoteTracker::GetMaxVersionAmongIgnoredUpdatesDueToMissingParentForTest()
    const {
  return max_version_among_ignored_updates_due_to_missing_parent_;
}

void SyncedNoteTracker::TraverseAndAppend(
    const vivaldi::NoteNode* node,
    std::vector<const SyncedNoteTrackerEntity*>* ordered_entities) const {
  const SyncedNoteTrackerEntity* entity = GetEntityForNoteNode(node);
  DCHECK(entity);
  DCHECK(entity->IsUnsynced());
  DCHECK(!entity->metadata().is_deleted());
  ordered_entities->push_back(entity);
  // Recurse for all children.
  for (const auto& child : node->children()) {
    const SyncedNoteTrackerEntity* child_entity =
        GetEntityForNoteNode(child.get());
    DCHECK(child_entity);
    if (!child_entity->IsUnsynced()) {
      // If the entity has no local change, no need to check its children. If
      // any of the children would have a pending commit, it would be a root for
      // a separate tree in the forest built in
      // ReorderEntitiesWithLocalNonDeletions() and will be handled by another
      // call to TraverseAndAppend().
      continue;
    }
    if (child_entity->metadata().is_deleted()) {
      // Deletion are stored sorted in |ordered_local_tombstones_| and will be
      // added later.
      continue;
    }
    TraverseAndAppend(child.get(), ordered_entities);
  }
}

void SyncedNoteTracker::UpdateUponCommitResponse(
    const SyncedNoteTrackerEntity* entity,
    const std::string& sync_id,
    int64_t server_version,
    int64_t acked_sequence_number) {
  DCHECK(entity);

  SyncedNoteTrackerEntity* mutable_entity = AsMutableEntity(entity);
  mutable_entity->MutableMetadata()->set_acked_sequence_number(
      acked_sequence_number);
  mutable_entity->MutableMetadata()->set_server_version(server_version);
  // If there are no pending commits, remove tombstones.
  if (!mutable_entity->IsUnsynced() &&
      mutable_entity->metadata().is_deleted()) {
    Remove(mutable_entity);
    return;
  }

  UpdateSyncIdIfNeeded(mutable_entity, sync_id);
}

void SyncedNoteTracker::UpdateSyncIdIfNeeded(
    const SyncedNoteTrackerEntity* entity,
    const std::string& sync_id) {
  DCHECK(entity);

  const std::string old_id = entity->metadata().server_id();
  if (old_id == sync_id) {
    return;
  }
  DCHECK_EQ(1U, sync_id_to_entities_map_.count(old_id));
  DCHECK_EQ(0U, sync_id_to_entities_map_.count(sync_id));

  std::unique_ptr<SyncedNoteTrackerEntity> owned_entity =
      std::move(sync_id_to_entities_map_.at(old_id));
  DCHECK_EQ(entity, owned_entity.get());
  owned_entity->MutableMetadata()->set_server_id(sync_id);
  sync_id_to_entities_map_[sync_id] = std::move(owned_entity);
  sync_id_to_entities_map_.erase(old_id);

  if (entity->note_node() && entity->note_node()->is_attachment()) {
    synced_file_store_->SetSyncFileRef(
        sync_id, syncer::NOTES,
        base::UTF16ToASCII(entity->note_node()->GetContent()));
  }
}

void SyncedNoteTracker::UndeleteTombstoneForNoteNode(
    const SyncedNoteTrackerEntity* entity,
    const vivaldi::NoteNode* node) {
  DCHECK(entity);
  DCHECK(node);
  DCHECK(entity->metadata().is_deleted());
  const syncer::ClientTagHash client_tag_hash =
      GetClientTagHashFromUuid(node->uuid());
  // The same entity must be used only for the same note node.
  DCHECK_EQ(entity->metadata().client_tag_hash(), client_tag_hash.value());
  DCHECK(note_node_to_entities_map_.find(node) ==
         note_node_to_entities_map_.end());
  DCHECK_EQ(GetEntityForSyncId(entity->metadata().server_id()), entity);

  std::erase(ordered_local_tombstones_, entity);
  SyncedNoteTrackerEntity* mutable_entity = AsMutableEntity(entity);
  mutable_entity->MutableMetadata()->set_is_deleted(false);
  mutable_entity->set_note_node(node);
  note_node_to_entities_map_[node] = mutable_entity;
}

void SyncedNoteTracker::AckSequenceNumber(
    const SyncedNoteTrackerEntity* entity) {
  DCHECK(entity);
  AsMutableEntity(entity)->MutableMetadata()->set_acked_sequence_number(
      entity->metadata().sequence_number());
}

bool SyncedNoteTracker::IsEmpty() const {
  return sync_id_to_entities_map_.empty();
}

size_t SyncedNoteTracker::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  size_t memory_usage = 0;
  memory_usage += EstimateMemoryUsage(sync_id_to_entities_map_);
  memory_usage += EstimateMemoryUsage(note_node_to_entities_map_);
  memory_usage += EstimateMemoryUsage(ordered_local_tombstones_);
  memory_usage += EstimateMemoryUsage(data_type_state_);
  return memory_usage;
}

size_t SyncedNoteTracker::TrackedNotesCount() const {
  return note_node_to_entities_map_.size();
}

size_t SyncedNoteTracker::TrackedUncommittedTombstonesCount() const {
  return ordered_local_tombstones_.size();
}

size_t SyncedNoteTracker::TrackedEntitiesCountForTest() const {
  return sync_id_to_entities_map_.size();
}

void SyncedNoteTracker::CheckAllNodesTracked(
    const NoteModelView* notes_model) const {
#if DCHECK_IS_ON()
  DCHECK(GetEntityForNoteNode(notes_model->main_node()));
  DCHECK(GetEntityForNoteNode(notes_model->other_node()));
  DCHECK(GetEntityForNoteNode(notes_model->trash_node()));

  ui::TreeNodeIterator<const vivaldi::NoteNode> iterator(
      notes_model->root_node());
  while (iterator.has_next()) {
    const vivaldi::NoteNode* node = iterator.Next();
    if (!notes_model->IsNodeSyncable(node)) {
      DCHECK(!GetEntityForNoteNode(node));
      continue;
    }
    DCHECK(GetEntityForNoteNode(node));
  }
#endif  // DCHECK_IS_ON()
}

}  // namespace sync_notes
