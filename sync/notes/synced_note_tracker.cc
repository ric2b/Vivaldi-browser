// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notes/synced_note_tracker.h"

#include <algorithm>
#include <set>
#include <unordered_map>

#include "app/vivaldi_apptools.h"
#include "base/base64.h"
#include "base/hash/sha1.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/time.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/protocol/proto_memory_estimations.h"
#include "components/sync_bookmarks/switches.h"
#include "notes/note_node.h"
#include "notes/notes_model.h"
#include "ui/base/models/tree_node_iterator.h"

namespace sync_notes {

namespace {

void HashSpecifics(const sync_pb::EntitySpecifics& specifics,
                   std::string* hash) {
  DCHECK_GT(specifics.ByteSize(), 0);
  base::Base64Encode(base::SHA1HashString(specifics.SerializeAsString()), hash);
}

// Returns a map from id to node for all nodes in |model|.
std::unordered_map<int64_t, const vivaldi::NoteNode*> BuildIdToNoteNodeMap(
    const vivaldi::NotesModel* model) {
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

SyncedNoteTracker::Entity::Entity(
    const vivaldi::NoteNode* note_node,
    std::unique_ptr<sync_pb::EntityMetadata> metadata)
    : note_node_(note_node), metadata_(std::move(metadata)) {
  // TODO(crbug.com/516866): The below CHECK is added to debug some crashes.
  // Should be removed after figuring out the reason for the crash.
  CHECK(metadata_);
  if (note_node) {
    DCHECK(!metadata_->is_deleted());
  } else {
    DCHECK(metadata_->is_deleted());
  }
}

SyncedNoteTracker::Entity::~Entity() = default;

bool SyncedNoteTracker::Entity::IsUnsynced() const {
  return metadata_->sequence_number() > metadata_->acked_sequence_number();
}

bool SyncedNoteTracker::Entity::MatchesDataIgnoringParent(
    const syncer::EntityData& data) const {
  if (metadata_->is_deleted() || data.is_deleted()) {
    // In case of deletion, no need to check the specifics.
    return metadata_->is_deleted() == data.is_deleted();
  }
  if (!syncer::UniquePosition::FromProto(metadata_->unique_position())
           .Equals(syncer::UniquePosition::FromProto(data.unique_position))) {
    return false;
  }
  return MatchesSpecificsHash(data.specifics);
}

bool SyncedNoteTracker::Entity::MatchesSpecificsHash(
    const sync_pb::EntitySpecifics& specifics) const {
  DCHECK(!metadata_->is_deleted());
  DCHECK_GT(specifics.ByteSize(), 0);
  std::string hash;
  HashSpecifics(specifics, &hash);
  return hash == metadata_->specifics_hash();
}

bool SyncedNoteTracker::Entity::has_final_guid() const {
  return metadata_->has_client_tag_hash();
}

bool SyncedNoteTracker::Entity::final_guid_matches(
    const std::string& guid) const {
  return metadata_->has_client_tag_hash() &&
         metadata_->client_tag_hash() ==
             syncer::ClientTagHash::FromUnhashed(syncer::NOTES, guid).value();
}

void SyncedNoteTracker::Entity::set_final_guid(const std::string& guid) {
  metadata_->set_client_tag_hash(
      syncer::ClientTagHash::FromUnhashed(syncer::NOTES, guid).value());
}

size_t SyncedNoteTracker::Entity::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  size_t memory_usage = 0;
  // Include the size of the pointer to the note node.
  memory_usage += sizeof(note_node_);
  memory_usage += EstimateMemoryUsage(metadata_);
  return memory_usage;
}

// static
std::unique_ptr<SyncedNoteTracker> SyncedNoteTracker::CreateEmpty(
    sync_pb::ModelTypeState model_type_state) {
  // base::WrapUnique() used because the constructor is private.
  auto tracker = base::WrapUnique(new SyncedNoteTracker(
      std::move(model_type_state), /*notes_full_title_reuploaded=*/false));
  return tracker;
}

// static
std::unique_ptr<SyncedNoteTracker>
SyncedNoteTracker::CreateFromNotesModelAndMetadata(
    const vivaldi::NotesModel* model,
    sync_pb::NotesModelMetadata model_metadata) {
  DCHECK(model);

  if (!model_metadata.model_type_state().initial_sync_done()) {
    return nullptr;
  }

  const bool notes_full_title_reuploaded =
      model_metadata.notes_full_title_reuploaded();

  auto tracker = base::WrapUnique(new SyncedNoteTracker(
      std::move(*model_metadata.mutable_model_type_state()),
      notes_full_title_reuploaded));

  bool is_not_corrupted = tracker->InitEntitiesFromModelAndMetadata(
      model, std::move(model_metadata));

  if (!is_not_corrupted) {
    return nullptr;
  }

  tracker->ReuploadNotesOnLoadIfNeeded();

  return tracker;
}

SyncedNoteTracker::~SyncedNoteTracker() = default;

const SyncedNoteTracker::Entity* SyncedNoteTracker::GetEntityForSyncId(
    const std::string& sync_id) const {
  auto it = sync_id_to_entities_map_.find(sync_id);
  return it != sync_id_to_entities_map_.end() ? it->second.get() : nullptr;
}

const SyncedNoteTracker::Entity* SyncedNoteTracker::GetTombstoneEntityForGuid(
    const std::string& guid) const {
  const syncer::ClientTagHash client_tag_hash =
      syncer::ClientTagHash::FromUnhashed(syncer::NOTES, guid);

  for (const Entity* tombstone_entity : ordered_local_tombstones_) {
    if (tombstone_entity->metadata()->client_tag_hash() ==
        client_tag_hash.value()) {
      return tombstone_entity;
    }
  }

  return nullptr;
}

SyncedNoteTracker::Entity* SyncedNoteTracker::AsMutableEntity(
    const Entity* entity) {
  DCHECK(entity);
  DCHECK_EQ(entity, GetEntityForSyncId(entity->metadata()->server_id()));

  // As per DCHECK above, this tracker owns |*entity|, so it's legitimate to
  // return non-const pointer.
  return const_cast<Entity*>(entity);
}

const SyncedNoteTracker::Entity* SyncedNoteTracker::GetEntityForNoteNode(
    const vivaldi::NoteNode* node) const {
  auto it = note_node_to_entities_map_.find(node);
  return it != note_node_to_entities_map_.end() ? it->second : nullptr;
}

const SyncedNoteTracker::Entity* SyncedNoteTracker::Add(
    const vivaldi::NoteNode* note_node,
    const std::string& sync_id,
    int64_t server_version,
    base::Time creation_time,
    const sync_pb::UniquePosition& unique_position,
    const sync_pb::EntitySpecifics& specifics) {
  DCHECK_GT(specifics.ByteSize(), 0);
  DCHECK(note_node);

  auto metadata = std::make_unique<sync_pb::EntityMetadata>();
  metadata->set_is_deleted(false);
  metadata->set_server_id(sync_id);
  metadata->set_server_version(server_version);
  metadata->set_creation_time(syncer::TimeToProtoTime(creation_time));
  metadata->set_modification_time(syncer::TimeToProtoTime(creation_time));
  metadata->set_sequence_number(0);
  metadata->set_acked_sequence_number(0);
  metadata->mutable_unique_position()->CopyFrom(unique_position);
  // For any newly added note, be it a local creation or a remote one, the
  // authoritative final GUID is known from start.
  metadata->set_client_tag_hash(
      syncer::ClientTagHash::FromUnhashed(syncer::NOTES, note_node->guid())
          .value());
  HashSpecifics(specifics, metadata->mutable_specifics_hash());
  auto entity = std::make_unique<Entity>(note_node, std::move(metadata));
  CHECK_EQ(0U, note_node_to_entities_map_.count(note_node));
  note_node_to_entities_map_[note_node] = entity.get();
  // TODO(crbug.com/516866): The below CHECK is added to debug some crashes.
  // Should be removed after figuring out the reason for the crash.
  CHECK_EQ(0U, sync_id_to_entities_map_.count(sync_id));
  const Entity* raw_entity = entity.get();
  sync_id_to_entities_map_[sync_id] = std::move(entity);
  return raw_entity;
}

void SyncedNoteTracker::Update(const Entity* entity,
                               int64_t server_version,
                               base::Time modification_time,
                               const sync_pb::UniquePosition& unique_position,
                               const sync_pb::EntitySpecifics& specifics) {
  DCHECK_GT(specifics.ByteSize(), 0);
  DCHECK(entity);

  Entity* mutable_entity = AsMutableEntity(entity);
  mutable_entity->metadata()->set_server_version(server_version);
  mutable_entity->metadata()->set_modification_time(
      syncer::TimeToProtoTime(modification_time));
  *mutable_entity->metadata()->mutable_unique_position() = unique_position;
  HashSpecifics(specifics,
                mutable_entity->metadata()->mutable_specifics_hash());
  // TODO(crbug.com/516866): in case of conflict, the entity might exist in
  // |ordered_local_tombstones_| as well if it has been locally deleted.
}

void SyncedNoteTracker::UpdateServerVersion(const Entity* entity,
                                            int64_t server_version) {
  DCHECK(entity);
  AsMutableEntity(entity)->metadata()->set_server_version(server_version);
}

void SyncedNoteTracker::PopulateFinalGuid(const Entity* entity,
                                          const std::string& guid) {
  DCHECK(entity);
  AsMutableEntity(entity)->set_final_guid(guid);
}

void SyncedNoteTracker::MarkCommitMayHaveStarted(const Entity* entity) {
  DCHECK(entity);
  AsMutableEntity(entity)->set_commit_may_have_started(true);
}

void SyncedNoteTracker::MarkDeleted(const Entity* entity) {
  DCHECK(entity);
  DCHECK(!entity->metadata()->is_deleted());
  DCHECK(entity->note_node());
  DCHECK_EQ(1U, note_node_to_entities_map_.count(entity->note_node()));

  Entity* mutable_entity = AsMutableEntity(entity);
  mutable_entity->metadata()->set_is_deleted(true);
  // Clear all references to the deleted note node.
  note_node_to_entities_map_.erase(mutable_entity->note_node());
  mutable_entity->clear_note_node();
  // TODO(crbug.com/516866): The below CHECK is added to debug some crashes.
  // Should be removed after figuring out the reason for the crash.
  CHECK_EQ(0, std::count(ordered_local_tombstones_.begin(),
                         ordered_local_tombstones_.end(), entity));
  ordered_local_tombstones_.push_back(mutable_entity);
}

void SyncedNoteTracker::Remove(const Entity* entity) {
  DCHECK(entity);
  // TODO(rushans): erase only if entity is not a tombstone.
  if (entity->note_node()) {
    DCHECK(!entity->metadata()->is_deleted());
    DCHECK_EQ(0, std::count(ordered_local_tombstones_.begin(),
                            ordered_local_tombstones_.end(), entity));
  } else {
    DCHECK(entity->metadata()->is_deleted());
  }

  note_node_to_entities_map_.erase(entity->note_node());
  base::Erase(ordered_local_tombstones_, entity);
  sync_id_to_entities_map_.erase(entity->metadata()->server_id());
}

void SyncedNoteTracker::IncrementSequenceNumber(const Entity* entity) {
  DCHECK(entity);
  // TODO(crbug.com/516866): Update base hash specifics here if the entity is
  // not already out of sync.
  AsMutableEntity(entity)->metadata()->set_sequence_number(
      entity->metadata()->sequence_number() + 1);
}

sync_pb::NotesModelMetadata SyncedNoteTracker::BuildNoteModelMetadata() const {
  sync_pb::NotesModelMetadata model_metadata;
  model_metadata.set_notes_full_title_reuploaded(notes_full_title_reuploaded_);
  for (const std::pair<const std::string, std::unique_ptr<Entity>>& pair :
       sync_id_to_entities_map_) {
    DCHECK(pair.second) << " for ID " << pair.first;
    DCHECK(pair.second->metadata()) << " for ID " << pair.first;
    if (pair.second->metadata()->is_deleted()) {
      // Deletions will be added later because they need to maintain the same
      // order as in |ordered_local_tombstones_|.
      continue;
    }
    DCHECK(pair.second->note_node());
    sync_pb::NoteMetadata* note_metadata = model_metadata.add_notes_metadata();
    note_metadata->set_id(pair.second->note_node()->id());
    *note_metadata->mutable_metadata() = *pair.second->metadata();
  }
  // Add pending deletions.
  for (const Entity* tombstone_entity : ordered_local_tombstones_) {
    DCHECK(tombstone_entity);
    DCHECK(tombstone_entity->metadata());
    DCHECK(tombstone_entity->metadata()->is_deleted());
    sync_pb::NoteMetadata* note_metadata = model_metadata.add_notes_metadata();
    *note_metadata->mutable_metadata() = *tombstone_entity->metadata();
  }
  *model_metadata.mutable_model_type_state() = model_type_state_;
  return model_metadata;
}

bool SyncedNoteTracker::HasLocalChanges() const {
  for (const std::pair<const std::string, std::unique_ptr<Entity>>& pair :
       sync_id_to_entities_map_) {
    Entity* entity = pair.second.get();
    if (entity->IsUnsynced()) {
      return true;
    }
  }
  return false;
}

std::vector<const SyncedNoteTracker::Entity*>
SyncedNoteTracker::GetAllEntities() const {
  std::vector<const SyncedNoteTracker::Entity*> entities;
  for (const std::pair<const std::string, std::unique_ptr<Entity>>& pair :
       sync_id_to_entities_map_) {
    entities.push_back(pair.second.get());
  }
  return entities;
}

std::vector<const SyncedNoteTracker::Entity*>
SyncedNoteTracker::GetEntitiesWithLocalChanges(size_t max_entries) const {
  std::vector<const SyncedNoteTracker::Entity*> entities_with_local_changes;
  // Entities with local non deletions should be sorted such that parent
  // creation/update comes before child creation/update.
  for (const std::pair<const std::string, std::unique_ptr<Entity>>& pair :
       sync_id_to_entities_map_) {
    Entity* entity = pair.second.get();
    if (entity->metadata()->is_deleted()) {
      // Deletions are stored sorted in |ordered_local_tombstones_| and will be
      // added later.
      continue;
    }
    if (entity->IsUnsynced()) {
      entities_with_local_changes.push_back(entity);
    }
  }
  std::vector<const SyncedNoteTracker::Entity*> ordered_local_changes =
      ReorderUnsyncedEntitiesExceptDeletions(entities_with_local_changes);
  for (const Entity* tombstone_entity : ordered_local_tombstones_) {
    // TODO(crbug.com/516866): The below CHECK is added to debug some crashes.
    // Should be removed after figuring out the reason for the crash.
    CHECK_EQ(0, std::count(ordered_local_changes.begin(),
                           ordered_local_changes.end(), tombstone_entity));
    ordered_local_changes.push_back(tombstone_entity);
  }
  if (ordered_local_changes.size() > max_entries) {
    // TODO(crbug.com/516866): Should be smart and stop building the vector
    // when |max_entries| is reached.
    return std::vector<const SyncedNoteTracker::Entity*>(
        ordered_local_changes.begin(),
        ordered_local_changes.begin() + max_entries);
  }
  return ordered_local_changes;
}

SyncedNoteTracker::SyncedNoteTracker(sync_pb::ModelTypeState model_type_state,
                                     bool notes_full_title_reuploaded)
    : model_type_state_(std::move(model_type_state)),
      notes_full_title_reuploaded_(notes_full_title_reuploaded) {}

bool SyncedNoteTracker::InitEntitiesFromModelAndMetadata(
    const vivaldi::NotesModel* model,
    sync_pb::NotesModelMetadata model_metadata) {
  DCHECK(model_type_state_.initial_sync_done());

  // Build a temporary map to look up note nodes efficiently by node ID.
  std::unordered_map<int64_t, const vivaldi::NoteNode*> id_to_note_node_map =
      BuildIdToNoteNodeMap(model);

  // Collect ids of non-deletion entries in the metadata.
  std::vector<int> metadata_node_ids;

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

      auto tombstone_entity = std::make_unique<Entity>(
          /*node=*/nullptr, std::make_unique<sync_pb::EntityMetadata>(
                                std::move(*note_metadata.mutable_metadata())));
      ordered_local_tombstones_.push_back(tombstone_entity.get());
      sync_id_to_entities_map_[sync_id] = std::move(tombstone_entity);
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

    if (!node->is_permanent_node() &&
        !note_metadata.metadata().has_client_tag_hash()) {
    }

    // The client-tag-hash is optional, but if it does exist, it is expected to
    // be equal to the hash of the note's GUID. This can be hit for example
    // if local note GUIDs were reassigned upon startup due to duplicates
    // (which is a NoteModel invariant violation and should be impossible).
    // TODO(crbug.com/1032052): Simplify this code once all local sync metadata
    // is required to populate the client tag (and be considered invalid
    // otherwise).
    if (note_metadata.metadata().has_client_tag_hash() &&
        !node->is_permanent_node() &&
        note_metadata.metadata().client_tag_hash() !=
            syncer::ClientTagHash::FromUnhashed(syncer::NOTES, node->guid())
                .value()) {
      DLOG(ERROR) << "Note GUID does not match the client tag.";

      if (base::FeatureList::IsEnabled(
              sync_bookmarks::
                  kInvalidateBookmarkSyncMetadataIfMismatchingGuid)) {
        return false;
      } else {
        // Simply clear the field, although it's most likely accurate, since it
        // isn't useful while the actual (unhashed) GUID is unknown.
        note_metadata.mutable_metadata()->clear_client_tag_hash();
      }
    }

    auto entity = std::make_unique<Entity>(
        node, std::make_unique<sync_pb::EntityMetadata>(
                  std::move(*note_metadata.mutable_metadata())));
    entity->set_commit_may_have_started(true);
    CHECK_EQ(0U, note_node_to_entities_map_.count(node));
    note_node_to_entities_map_[node] = entity.get();
    sync_id_to_entities_map_[sync_id] = std::move(entity);
  }

  // See if there are untracked entities in the NotesModel.
  std::vector<int> model_node_ids;
  ui::TreeNodeIterator<const vivaldi::NoteNode> iterator(model->root_node());
  while (iterator.has_next()) {
    const vivaldi::NoteNode* node = iterator.Next();
    if (note_node_to_entities_map_.count(node) == 0) {
      return false;
    }
  }

  CheckAllNodesTracked(model);
  return true;
}

std::vector<const SyncedNoteTracker::Entity*>
SyncedNoteTracker::ReorderUnsyncedEntitiesExceptDeletions(
    const std::vector<const SyncedNoteTracker::Entity*>& entities) const {
  // This method sorts the entities with local non deletions such that parent
  // creation/update comes before child creation/update.

  // The algorithm works by constructing a forest of all non-deletion updates
  // and then traverses each tree in the forest recursively:
  // 1. Iterate over all entities and collect all nodes in |nodes|.
  // 2. Iterate over all entities again and node that is a child of another
  //    node. What's left in |nodes| are the roots of the forest.
  // 3. Start at each root in |nodes|, emit the update and recurse over its
  //    children.
  std::set<const vivaldi::NoteNode*> nodes;
  // Collect nodes with updates
  for (const SyncedNoteTracker::Entity* entity : entities) {
    DCHECK(entity->IsUnsynced());
    DCHECK(!entity->metadata()->is_deleted());
    DCHECK(entity->note_node());
    nodes.insert(entity->note_node());
  }
  // Remove those who are direct children of another node.
  for (const SyncedNoteTracker::Entity* entity : entities) {
    const vivaldi::NoteNode* node = entity->note_node();
    for (const auto& child : node->children())
      nodes.erase(child.get());
  }
  // |nodes| contains only roots of all trees in the forest all of which are
  // ready to be processed because their parents have no pending updates.
  std::vector<const SyncedNoteTracker::Entity*> ordered_entities;
  for (const vivaldi::NoteNode* node : nodes) {
    TraverseAndAppend(node, &ordered_entities);
  }
  return ordered_entities;
}

void SyncedNoteTracker::ReuploadNotesOnLoadIfNeeded() {
  if (notes_full_title_reuploaded_ ||
      !base::FeatureList::IsEnabled(
          switches::kSyncReuploadBookmarkFullTitles)) {
    return;
  }
  for (const auto& sync_id_and_entity : sync_id_to_entities_map_) {
    const SyncedNoteTracker::Entity* entity = sync_id_and_entity.second.get();
    if (entity->IsUnsynced() || entity->metadata()->is_deleted()) {
      continue;
    }
    if (entity->note_node()->is_permanent_node()) {
      continue;
    }
    IncrementSequenceNumber(entity);
  }
  notes_full_title_reuploaded_ = true;
}

void SyncedNoteTracker::TraverseAndAppend(
    const vivaldi::NoteNode* node,
    std::vector<const SyncedNoteTracker::Entity*>* ordered_entities) const {
  const SyncedNoteTracker::Entity* entity = GetEntityForNoteNode(node);
  DCHECK(entity);
  DCHECK(entity->IsUnsynced());
  DCHECK(!entity->metadata()->is_deleted());
  ordered_entities->push_back(entity);
  // Recurse for all children.
  for (const auto& child : node->children()) {
    const SyncedNoteTracker::Entity* child_entity =
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
    if (child_entity->metadata()->is_deleted()) {
      // Deletion are stored sorted in |ordered_local_tombstones_| and will be
      // added later.
      continue;
    }
    TraverseAndAppend(child.get(), ordered_entities);
  }
}

void SyncedNoteTracker::UpdateUponCommitResponse(
    const Entity* entity,
    const std::string& sync_id,
    int64_t server_version,
    int64_t acked_sequence_number) {
  // TODO(crbug.com/516866): Update specifics if we decide to keep it.
  DCHECK(entity);

  Entity* mutable_entity = AsMutableEntity(entity);
  mutable_entity->metadata()->set_acked_sequence_number(acked_sequence_number);
  mutable_entity->metadata()->set_server_version(server_version);
  // If there are no pending commits, remove tombstones.
  if (!mutable_entity->IsUnsynced() &&
      mutable_entity->metadata()->is_deleted()) {
    Remove(mutable_entity);
    return;
  }

  UpdateSyncIdForLocalCreationIfNeeded(mutable_entity, sync_id);
}

void SyncedNoteTracker::UpdateSyncIdForLocalCreationIfNeeded(
    const Entity* entity,
    const std::string& sync_id) {
  DCHECK(entity);

  const std::string old_id = entity->metadata()->server_id();
  if (old_id == sync_id) {
    return;
  }
  // TODO(crbug.com/516866): The below CHECK is added to debug some crashes.
  // Should be removed after figuring out the reason for the crash.
  CHECK_EQ(1U, sync_id_to_entities_map_.count(old_id));
  CHECK_EQ(0U, sync_id_to_entities_map_.count(sync_id));

  std::unique_ptr<Entity> owned_entity =
      std::move(sync_id_to_entities_map_.at(old_id));
  DCHECK_EQ(entity, owned_entity.get());
  owned_entity->metadata()->set_server_id(sync_id);
  sync_id_to_entities_map_[sync_id] = std::move(owned_entity);
  sync_id_to_entities_map_.erase(old_id);
}

void SyncedNoteTracker::UpdateNoteNodePointer(
    const vivaldi::NoteNode* old_node,
    const vivaldi::NoteNode* new_node) {
  if (old_node == new_node) {
    return;
  }

  CHECK_EQ(0U, note_node_to_entities_map_.count(new_node));
  CHECK_EQ(1U, note_node_to_entities_map_.count(old_node));

  note_node_to_entities_map_[new_node] = note_node_to_entities_map_[old_node];
  note_node_to_entities_map_[new_node]->set_note_node(new_node);
  note_node_to_entities_map_.erase(old_node);
}

void SyncedNoteTracker::UndeleteTombstoneForNoteNode(
    const Entity* entity,
    const vivaldi::NoteNode* node) {
  DCHECK(entity);
  DCHECK(node);
  DCHECK(entity->metadata()->is_deleted());
  const syncer::ClientTagHash client_tag_hash =
      syncer::ClientTagHash::FromUnhashed(syncer::NOTES, node->guid());
  // The same entity must be used only for the same note node.
  DCHECK_EQ(entity->metadata()->client_tag_hash(), client_tag_hash.value());
  DCHECK_EQ(GetTombstoneEntityForGuid(node->guid()), entity);
  DCHECK(note_node_to_entities_map_.find(node) ==
         note_node_to_entities_map_.end());
  DCHECK_EQ(GetEntityForSyncId(entity->metadata()->server_id()), entity);

  base::Erase(ordered_local_tombstones_, entity);
  Entity* mutable_entity = AsMutableEntity(entity);
  mutable_entity->metadata()->set_is_deleted(false);
  mutable_entity->set_note_node(node);
  note_node_to_entities_map_[node] = mutable_entity;
}

void SyncedNoteTracker::AckSequenceNumber(const Entity* entity) {
  DCHECK(entity);
  AsMutableEntity(entity)->metadata()->set_acked_sequence_number(
      entity->metadata()->sequence_number());
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
  memory_usage += EstimateMemoryUsage(model_type_state_);
  return memory_usage;
}

size_t SyncedNoteTracker::TrackedEntitiesCountForTest() const {
  return sync_id_to_entities_map_.size();
}

size_t SyncedNoteTracker::TrackedNotesCountForDebugging() const {
  return note_node_to_entities_map_.size();
}

size_t SyncedNoteTracker::TrackedUncommittedTombstonesCountForDebugging()
    const {
  return ordered_local_tombstones_.size();
}

void SyncedNoteTracker::CheckAllNodesTracked(
    const vivaldi::NotesModel* notes_model) const {
  // TODO(crbug.com/516866): The method is added to debug some crashes.
  // Since it's relatively expensive, it should run on debug enabled
  // builds only after the root cause is found.
  CHECK(GetEntityForNoteNode(notes_model->main_node()));
  CHECK(GetEntityForNoteNode(notes_model->other_node()));
  CHECK(GetEntityForNoteNode(notes_model->trash_node()));

  ui::TreeNodeIterator<const vivaldi::NoteNode> iterator(
      notes_model->root_node());
  while (iterator.has_next()) {
    const vivaldi::NoteNode* node = iterator.Next();
    // Root node is usually tracked, unless the sync data has been provided by
    // the USS migrator.
    if (node == notes_model->root_node()) {
      continue;
    }
    // TODO(crbug.com/516866): The below CHECK is added to debug some crashes.
    // Should be converted to a DCHECK after the root cause if found.
    CHECK(GetEntityForNoteNode(node));
  }
}

}  // namespace sync_notes
