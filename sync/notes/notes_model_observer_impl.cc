// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notes/notes_model_observer_impl.h"

#include <utility>

#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync_bookmarks/switches.h"
#include "notes/note_node.h"
#include "notes/notes_model.h"
#include "sync/notes/note_specifics_conversions.h"
#include "sync/notes/synced_note_tracker.h"
#include "sync/vivaldi_hash_util.h"

namespace sync_notes {

NotesModelObserverImpl::NotesModelObserverImpl(
    const base::RepeatingClosure& nudge_for_commit_closure,
    base::OnceClosure on_notes_model_being_deleted_closure,
    SyncedNoteTracker* note_tracker)
    : note_tracker_(note_tracker),
      nudge_for_commit_closure_(nudge_for_commit_closure),
      on_notes_model_being_deleted_closure_(
          std::move(on_notes_model_being_deleted_closure)) {
  DCHECK(note_tracker_);
}

NotesModelObserverImpl::~NotesModelObserverImpl() = default;

void NotesModelObserverImpl::NotesModelLoaded(vivaldi::NotesModel* model,
                                              bool ids_reassigned) {
  // This class isn't responsible for any loading-related logic.
}

void NotesModelObserverImpl::NotesModelBeingDeleted(
    vivaldi::NotesModel* model) {
  std::move(on_notes_model_being_deleted_closure_).Run();
}

void NotesModelObserverImpl::NotesNodeMoved(vivaldi::NotesModel* model,
                                            const vivaldi::NoteNode* old_parent,
                                            size_t old_index,
                                            const vivaldi::NoteNode* new_parent,
                                            size_t new_index) {
  const vivaldi::NoteNode* node = new_parent->children()[new_index].get();

  // We shouldn't see changes to the top-level nodes.
  DCHECK(!model->is_permanent_node(node));

  const SyncedNoteTracker::Entity* entity =
      note_tracker_->GetEntityForNoteNode(node);
  DCHECK(entity);

  const std::string& sync_id = entity->metadata()->server_id();
  const base::Time modification_time = base::Time::Now();
  const sync_pb::UniquePosition unique_position =
      ComputePosition(*new_parent, new_index, sync_id).ToProto();

  sync_pb::EntitySpecifics specifics =
      CreateSpecificsFromNoteNode(node, model, entity->has_final_guid());

  note_tracker_->Update(entity, entity->metadata()->server_version(),
                        modification_time, unique_position, specifics);
  // Mark the entity that it needs to be committed.
  note_tracker_->IncrementSequenceNumber(entity);
  nudge_for_commit_closure_.Run();
}

void NotesModelObserverImpl::NotesNodeAdded(vivaldi::NotesModel* model,
                                            const vivaldi::NoteNode* parent,
                                            size_t index) {
  const vivaldi::NoteNode* node = parent->children()[index].get();

  // Assign a temp server id for the entity. Will be overriden by the actual
  // server id upon receiving commit response.
  // Local note creations should have used a random GUID so it's safe to
  // use it as originator client item ID, without the risk for collision.
  const sync_pb::UniquePosition unique_position =
      ComputePosition(*parent, index, node->guid().AsLowercaseString())
          .ToProto();

  sync_pb::EntitySpecifics specifics =
      CreateSpecificsFromNoteNode(node, model, /*include_guid=*/true);

  // It is possible that a created note was restored after deletion and
  // the tombstone was not committed yet. In that case the existing entity
  // should be updated.
  const SyncedNoteTracker::Entity* entity =
      note_tracker_->GetTombstoneEntityForGuid(node->guid());
  const base::Time creation_time = base::Time::Now();
  if (entity) {
    note_tracker_->UndeleteTombstoneForNoteNode(entity, node);
    note_tracker_->Update(entity, entity->metadata()->server_version(),
                          creation_time, unique_position, specifics);
  } else {
    entity = note_tracker_->Add(node, node->guid().AsLowercaseString(),
                                syncer::kUncommittedVersion, creation_time,
                                unique_position, specifics);
  }

  // Mark the entity that it needs to be committed.
  note_tracker_->IncrementSequenceNumber(entity);
  nudge_for_commit_closure_.Run();
}

void NotesModelObserverImpl::OnWillRemoveNotes(vivaldi::NotesModel* model,
                                               const vivaldi::NoteNode* parent,
                                               size_t old_index,
                                               const vivaldi::NoteNode* node) {
  note_tracker_->CheckAllNodesTracked(model);
  ProcessDelete(parent, node);
  nudge_for_commit_closure_.Run();
}

void NotesModelObserverImpl::NotesNodeRemoved(vivaldi::NotesModel* model,
                                              const vivaldi::NoteNode* parent,
                                              size_t old_index,
                                              const vivaldi::NoteNode* node) {
  // All the work should have already been done in OnWillRemoveNotes.
  DCHECK(note_tracker_->GetEntityForNoteNode(node) == nullptr);
  note_tracker_->CheckAllNodesTracked(model);
}

void NotesModelObserverImpl::OnWillRemoveAllNotes(vivaldi::NotesModel* model) {
  const vivaldi::NoteNode* root_node = model->root_node();
  for (const auto& permanent_node : root_node->children()) {
    for (const auto& child : permanent_node->children()) {
      ProcessDelete(permanent_node.get(), child.get());
    }
    nudge_for_commit_closure_.Run();
  }
}

void NotesModelObserverImpl::NotesAllNodesRemoved(vivaldi::NotesModel* model) {
  // All the work should have already been done in
  // OnWillRemoveAllUserNotes.
}

void NotesModelObserverImpl::NotesNodeChanged(vivaldi::NotesModel* model,
                                              const vivaldi::NoteNode* node) {
  // We shouldn't see changes to the top-level nodes.
  DCHECK(!model->is_permanent_node(node));

  const SyncedNoteTracker::Entity* entity =
      note_tracker_->GetEntityForNoteNode(node);
  if (!entity) {
    // If the node hasn't been added to the tracker yet, we do nothing. It
    // will be added later. It's how NotesModel currently notifies observers,
    // if further changes are triggered *during* observer notification.
    // Consider the following scenario:
    // 1. New note added.
    // 2. NotesModel notifies all the observers about the new node.
    // 3. One observer A get's notified before us.
    // 4. Observer A decided to update the note node.
    // 5. NoteskModel notifies all observers about the update.
    // 6. We received the notification about the update before the creation.
    // 7. We will get the notification about the addition later and then we
    // can
    //    start tracking the node.
    return;
  }

  const base::Time modification_time = base::Time::Now();
  sync_pb::EntitySpecifics specifics =
      CreateSpecificsFromNoteNode(node, model, entity->has_final_guid());

  // TODO(crbug.com/516866): The below CHECKs are added to debug some crashes.
  // Should be removed after figuring out the reason for the crash.
  CHECK_EQ(entity, note_tracker_->GetEntityForNoteNode(node));
  if (entity->MatchesSpecificsHash(specifics)) {
    // We should push data to the server only if there is an actual change in
    // the data. We could hit this code path without having actual changes
    return;
  }
  note_tracker_->Update(entity, entity->metadata()->server_version(),
                        modification_time,
                        entity->metadata()->unique_position(), specifics);
  // Mark the entity that it needs to be committed.
  note_tracker_->IncrementSequenceNumber(entity);
  nudge_for_commit_closure_.Run();
}

void NotesModelObserverImpl::NotesNodeAttachmentChanged(
    vivaldi::NotesModel* model,
    const vivaldi::NoteNode* node) {
  NotesNodeChanged(model, node);
}

void NotesModelObserverImpl::NotesNodeChildrenReordered(
    vivaldi::NotesModel* model,
    const vivaldi::NoteNode* node) {
  // The given node's children got reordered. We need to reorder all the
  // corresponding sync node.

  // TODO(crbug/com/516866): Make sure that single-move case doesn't produce
  // unnecessary updates. One approach would be something like:
  // 1. Find a subsequence of elements in the beginning of the vector that is
  //    already sorted.
  // 2. The same for the end.
  // 3. If the two overlap, adjust so they don't.
  // 4. Sort the middle, using Between (e.g. recursive implementation).

  syncer::UniquePosition position;
  for (const auto& child : node->children()) {
    const SyncedNoteTracker::Entity* entity =
        note_tracker_->GetEntityForNoteNode(child.get());
    DCHECK(entity);

    const std::string& sync_id = entity->metadata()->server_id();
    const std::string suffix = syncer::GenerateSyncableNotesHash(
        note_tracker_->model_type_state().cache_guid(), sync_id);
    const base::Time modification_time = base::Time::Now();

    position = (child == node->children().front())
                   ? syncer::UniquePosition::InitialPosition(suffix)
                   : syncer::UniquePosition::After(position, suffix);

    const sync_pb::EntitySpecifics specifics = CreateSpecificsFromNoteNode(
        child.get(), model, entity->has_final_guid());

    note_tracker_->Update(entity, entity->metadata()->server_version(),
                          modification_time, position.ToProto(), specifics);
    // Mark the entity that it needs to be committed.
    note_tracker_->IncrementSequenceNumber(entity);
  }
  nudge_for_commit_closure_.Run();
}

syncer::UniquePosition NotesModelObserverImpl::ComputePosition(
    const vivaldi::NoteNode& parent,
    size_t index,
    const std::string& sync_id) {
  const std::string& suffix = syncer::GenerateSyncableNotesHash(
      note_tracker_->model_type_state().cache_guid(), sync_id);
  DCHECK(!parent.children().empty());
  const SyncedNoteTracker::Entity* predecessor_entity = nullptr;
  const SyncedNoteTracker::Entity* successor_entity = nullptr;

  // Look for the first tracked predecessor.
  for (auto i = parent.children().crend() - index;
       i != parent.children().crend(); ++i) {
    const vivaldi::NoteNode* predecessor_node = i->get();
    predecessor_entity = note_tracker_->GetEntityForNoteNode(predecessor_node);
    if (predecessor_entity) {
      break;
    }
  }

  // Look for the first tracked successor.
  for (auto i = parent.children().cbegin() + index + 1;
       i != parent.children().cend(); ++i) {
    const vivaldi::NoteNode* successor_node = i->get();
    successor_entity = note_tracker_->GetEntityForNoteNode(successor_node);
    if (successor_entity) {
      break;
    }
  }

  if (!predecessor_entity && !successor_entity) {
    // No tracked siblings.
    return syncer::UniquePosition::InitialPosition(suffix);
  }

  if (!predecessor_entity && successor_entity) {
    // No predecessor, insert before the successor.
    return syncer::UniquePosition::Before(
        syncer::UniquePosition::FromProto(
            successor_entity->metadata()->unique_position()),
        suffix);
  }

  if (predecessor_entity && !successor_entity) {
    // No successor, insert after the predecessor
    return syncer::UniquePosition::After(
        syncer::UniquePosition::FromProto(
            predecessor_entity->metadata()->unique_position()),
        suffix);
  }

  // Both predecessor and successor, insert in the middle.
  return syncer::UniquePosition::Between(
      syncer::UniquePosition::FromProto(
          predecessor_entity->metadata()->unique_position()),
      syncer::UniquePosition::FromProto(
          successor_entity->metadata()->unique_position()),
      suffix);
}

void NotesModelObserverImpl::ProcessDelete(const vivaldi::NoteNode* parent,
                                           const vivaldi::NoteNode* node) {
  // If not a leaf node, process all children first.
  for (const auto& child : node->children())
    ProcessDelete(node, child.get());
  // Process the current node.
  const SyncedNoteTracker::Entity* entity =
      note_tracker_->GetEntityForNoteNode(node);
  // Shouldn't try to delete untracked entities.
  DCHECK(entity);
  // If the entity hasn't been committed and doesn't have an inflight commit
  // request, simply remove it from the tracker.
  if (entity->metadata()->server_version() == syncer::kUncommittedVersion &&
      !entity->commit_may_have_started()) {
    note_tracker_->Remove(entity);
    return;
  }
  note_tracker_->MarkDeleted(entity);
  // Mark the entity that it needs to be committed.
  note_tracker_->IncrementSequenceNumber(entity);
}

}  // namespace sync_notes
