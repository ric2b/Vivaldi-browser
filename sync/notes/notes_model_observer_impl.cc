// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notes/notes_model_observer_impl.h"

#include <utility>

#include "base/check.h"
#include "base/no_destructor.h"
#include "components/notes/note_node.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "sync/notes/note_model_view.h"
#include "sync/notes/note_specifics_conversions.h"
#include "sync/notes/synced_note_tracker.h"
#include "sync/notes/synced_note_tracker_entity.h"
#include "sync/vivaldi_hash_util.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace sync_notes {

namespace {

// A helper wrapper used to compare UniquePosition with positions before the
// first and after the last elements.
class UniquePositionWrapper {
 public:
  static UniquePositionWrapper Min() {
    return UniquePositionWrapper(MinUniquePosition{});
  }

  static UniquePositionWrapper Max() {
    return UniquePositionWrapper(MaxUniquePosition{});
  }

  // |unique_position| must be valid.
  static UniquePositionWrapper ForValidUniquePosition(
      syncer::UniquePosition unique_position) {
    DCHECK(unique_position.IsValid());
    return UniquePositionWrapper(std::move(unique_position));
  }

  UniquePositionWrapper(UniquePositionWrapper&&) = default;
  UniquePositionWrapper& operator=(UniquePositionWrapper&&) = default;

  // Returns valid UniquePosition or invalid one for Min() and Max().
  const syncer::UniquePosition& GetUniquePosition() const {
    static const base::NoDestructor<syncer::UniquePosition>
        kEmptyUniquePosition;
    if (HoldsUniquePosition()) {
      return absl::get<syncer::UniquePosition>(value_);
    }
    return *kEmptyUniquePosition;
  }

  bool LessThan(const UniquePositionWrapper& other) const {
    if (value_.index() != other.value_.index()) {
      return value_.index() < other.value_.index();
    }
    if (!HoldsUniquePosition()) {
      // Both arguments are MinUniquePosition or MaxUniquePosition, in both
      // cases they are equal.
      return false;
    }
    return GetUniquePosition().LessThan(other.GetUniquePosition());
  }

 private:
  struct MinUniquePosition {};
  struct MaxUniquePosition {};

  explicit UniquePositionWrapper(absl::variant<MinUniquePosition,
                                               syncer::UniquePosition,
                                               MaxUniquePosition> value)
      : value_(std::move(value)) {}

  bool HoldsUniquePosition() const {
    return absl::holds_alternative<syncer::UniquePosition>(value_);
  }

  // The order is used to compare positions.
  absl::variant<MinUniquePosition, syncer::UniquePosition, MaxUniquePosition>
      value_;
};

}  // namespace

NotesModelObserverImpl::NotesModelObserverImpl(
    NoteModelView* note_model,
    const base::RepeatingClosure& nudge_for_commit_closure,
    base::OnceClosure on_notes_model_being_deleted_closure,
    SyncedNoteTracker* note_tracker)
    : note_model_(note_model),
      note_tracker_(note_tracker),
      nudge_for_commit_closure_(nudge_for_commit_closure),
      on_notes_model_being_deleted_closure_(
          std::move(on_notes_model_being_deleted_closure)) {
  CHECK(note_model_);
  DCHECK(note_tracker_);
}

NotesModelObserverImpl::~NotesModelObserverImpl() = default;

void NotesModelObserverImpl::NotesModelLoaded(bool ids_reassigned) {
  // This class isn't responsible for any loading-related logic.
}

void NotesModelObserverImpl::NotesModelBeingDeleted() {
  std::move(on_notes_model_being_deleted_closure_).Run();
}

void NotesModelObserverImpl::NotesNodeMoved(const vivaldi::NoteNode* old_parent,
                                            size_t old_index,
                                            const vivaldi::NoteNode* new_parent,
                                            size_t new_index) {
  const vivaldi::NoteNode* node = new_parent->children()[new_index].get();

  // We shouldn't see changes to the top-level nodes.
  DCHECK(!note_model_->is_permanent_node(node));

  // Handle moves that make a node newly syncable.
  if (!note_model_->IsNodeSyncable(old_parent) &&
      note_model_->IsNodeSyncable(new_parent)) {
    NotesNodeAdded(new_parent, new_index);
    return;
  }

  // Handle moves that make a node non-syncable.
  if (note_model_->IsNodeSyncable(old_parent) &&
      !note_model_->IsNodeSyncable(new_parent)) {
    // OnWillRemoveNotes() cannot be invoked here because |node| is already
    // moved and unsyncable, whereas OnWillRemoveNotes() assumes the change
    // hasn't happened yet.
    ProcessDelete(node, FROM_HERE);
    nudge_for_commit_closure_.Run();
    note_tracker_->CheckAllNodesTracked(note_model_);
    return;
  }

  // Ignore changes to non-syncable nodes (e.g. managed nodes).
  if (!note_model_->IsNodeSyncable(node)) {
    return;
  }

  const SyncedNoteTrackerEntity* entity =
      note_tracker_->GetEntityForNoteNode(node);
  CHECK(entity);

  const base::Time modification_time = base::Time::Now();
  const syncer::UniquePosition unique_position =
      ComputePosition(*new_parent, new_index);

  sync_pb::EntitySpecifics specifics =
      CreateSpecificsFromNoteNode(node, note_model_, unique_position.ToProto());

  note_tracker_->Update(entity, entity->metadata().server_version(),
                        modification_time, specifics);
  // Mark the entity that it needs to be committed.
  note_tracker_->IncrementSequenceNumber(entity);
  nudge_for_commit_closure_.Run();
  note_tracker_->CheckAllNodesTracked(note_model_);
}

void NotesModelObserverImpl::NotesNodeAdded(const vivaldi::NoteNode* parent,
                                            size_t index) {
  const vivaldi::NoteNode* node = parent->children()[index].get();

  // Ignore changes to non-syncable nodes (e.g. managed nodes).
  if (!note_model_->IsNodeSyncable(node)) {
    return;
  }

  const SyncedNoteTrackerEntity* parent_entity =
      note_tracker_->GetEntityForNoteNode(parent);
  DCHECK(parent_entity);

  const syncer::UniquePosition unique_position =
      ComputePosition(*parent, index);

  sync_pb::EntitySpecifics specifics =
      CreateSpecificsFromNoteNode(node, note_model_, unique_position.ToProto());

  // It is possible that a created note was restored after deletion and
  // the tombstone was not committed yet. In that case the existing entity
  // should be updated.
  const SyncedNoteTrackerEntity* entity =
      note_tracker_->GetEntityForUuid(node->uuid());
  const base::Time creation_time = base::Time::Now();
  if (entity) {
    // If there is a tracked entity with the same client tag hash (effectively
    // the same note GUID), it must be a tombstone. Otherwise it means
    // the note model contains to notes with the same GUID.
    DCHECK(!entity->note_node()) << "Added note with duplicate GUID";
    note_tracker_->UndeleteTombstoneForNoteNode(entity, node);
    note_tracker_->Update(entity, entity->metadata().server_version(),
                          creation_time, specifics);
  } else {
    entity = note_tracker_->Add(node, node->uuid().AsLowercaseString(),
                                syncer::kUncommittedVersion, creation_time,
                                specifics);
  }

  // Mark the entity that it needs to be committed.
  note_tracker_->IncrementSequenceNumber(entity);
  nudge_for_commit_closure_.Run();

  // Do not check if all nodes are tracked because it's still possible that some
  // nodes are untracked, e.g. if current node has been just restored and its
  // children will be added soon.
}

void NotesModelObserverImpl::OnWillRemoveNotes(const vivaldi::NoteNode* parent,
                                               size_t old_index,
                                               const vivaldi::NoteNode* node,
                                               const base::Location& location) {
  // Ignore changes to non-syncable nodes (e.g. managed nodes).
  if (!note_model_->IsNodeSyncable(node)) {
    return;
  }
  note_tracker_->CheckAllNodesTracked(note_model_);
  ProcessDelete(node, location);
  nudge_for_commit_closure_.Run();
}

void NotesModelObserverImpl::NotesNodeRemoved(const vivaldi::NoteNode* parent,
                                              size_t old_index,
                                              const vivaldi::NoteNode* node,
                                              const base::Location& location) {
  // All the work should have already been done in OnWillRemoveNotes.
  DCHECK(note_tracker_->GetEntityForNoteNode(node) == nullptr);
  note_tracker_->CheckAllNodesTracked(note_model_);
}

void NotesModelObserverImpl::OnWillRemoveAllNotes(
    const base::Location& location) {
  note_tracker_->CheckAllNodesTracked(note_model_);
  const vivaldi::NoteNode* root_node = note_model_->root_node();
  for (const auto& permanent_node : root_node->children()) {
    for (const auto& child : permanent_node->children()) {
      if (note_model_->IsNodeSyncable(child.get())) {
        ProcessDelete(child.get(), location);
      }
    }
    nudge_for_commit_closure_.Run();
  }
}

void NotesModelObserverImpl::NotesAllNodesRemoved(
    const base::Location& location) {
  // All the work should have already been done in
  // OnWillRemoveAllUserNotes.
  note_tracker_->CheckAllNodesTracked(note_model_);
}

void NotesModelObserverImpl::NotesNodeChanged(const vivaldi::NoteNode* node) {
  // Ignore changes to non-syncable nodes (e.g. managed nodes).
  if (!note_model_->IsNodeSyncable(node)) {
    return;
  }

  // We shouldn't see changes to the top-level nodes.
  DCHECK(!note_model_->is_permanent_node(node));

  const SyncedNoteTrackerEntity* entity =
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

  sync_pb::EntitySpecifics specifics = CreateSpecificsFromNoteNode(
      node, note_model_, entity->metadata().unique_position());

  // Data should be committed to the server only if there is an actual change,
  // determined here by comparing hashes.
  if (entity->MatchesSpecificsHash(specifics)) {
    // Specifics haven't actually changed, so the local change can be ignored.
    return;
  }
  note_tracker_->Update(entity, entity->metadata().server_version(),
                        /*modification_time=*/base::Time::Now(), specifics);
  // Mark the entity that it needs to be committed.
  note_tracker_->IncrementSequenceNumber(entity);
  nudge_for_commit_closure_.Run();
}

void NotesModelObserverImpl::NotesNodeChildrenReordered(
    const vivaldi::NoteNode* node) {
  // Ignore changes to non-syncable nodes (e.g. managed nodes).
  if (!note_model_->IsNodeSyncable(node)) {
    return;
  }

  if (node->children().size() <= 1) {
    // There is no real change in the order of |node|'s children.
    return;
  }

  // The given node's children got reordered, all the corresponding sync nodes
  // need to be reordered. The code is optimized to detect move of only one
  // note (which is the case on Android platform). There are 2 main cases:
  // a note moved to left or to right. Moving a note to the first and
  // last positions are two more special cases. The algorithm iterates over each
  // note and compares it to the left and right nodes to determine whether
  // it's ordered or not.
  //
  // Each digit below represents note's original position.
  //
  // Moving a note to the left: 0123456 -> 0612345.
  // When processing '6', its unique position is greater than both left and
  // right nodes.
  //
  // Moving a note to the right: 0123456 -> 0234516.
  // When processing '1', its unique position is less than both left and right
  // nodes.
  //
  // Note that in both cases left node is less than right node. This condition
  // is checked when iterating over notes and if it's violated, the
  // algorithm falls back to generating positions for all the following nodes.
  //
  // For example, if two nodes are moved to one place: 0123456 -> 0156234 (nodes
  // '5' and '6' are moved together). In this case, 0156 will remain and when
  // processing '2', algorithm will fall back to generating unique positions for
  // nodes '2', '3' and '4'. It will be detected by comparing the next node '3'
  // with the previous '6'.

  // Store |cur| outside of the loop to prevent parsing UniquePosition twice.
  UniquePositionWrapper cur = UniquePositionWrapper::ForValidUniquePosition(
      GetUniquePositionForNode(node->children().front().get()));
  UniquePositionWrapper prev = UniquePositionWrapper::Min();
  for (size_t current_index = 0; current_index < node->children().size();
       ++current_index) {
    UniquePositionWrapper next = UniquePositionWrapper::Max();
    if (current_index + 1 < node->children().size()) {
      next = UniquePositionWrapper::ForValidUniquePosition(
          GetUniquePositionForNode(node->children()[current_index + 1].get()));
    }

    // |prev| is the last ordered node. Compare |cur| and |next| with it to
    // decide whether current node needs to be updated. Consider the following
    // cases: 0. |prev| < |cur| < |next|: all elements are ordered.
    // 1. |cur| < |prev| < |next|: update current node and put it between |prev|
    //                             and |next|.
    // 2. |cur| < |next| < |prev|: both |cur| and |next| are out of order, fall
    //                             back to simple approach.
    // 3. |next| < |cur| < |prev|: same as #2.
    // 4. |prev| < |next| < |cur|: update current node and put it between |prev|
    //                             and |next|.
    // 5. |next| < |prev| < |cur|: consider current node ordered, |next| will be
    //                             updated on the next step.
    //
    // In the following code only cases where current node needs to be updated
    // are considered (#0 and #5 are omitted because there is nothing to do).

    bool update_current_position = false;
    if (cur.LessThan(prev)) {
      // |cur| < |prev|, cases #1, #2 and #3.
      if (next.LessThan(prev)) {
        // There are two consecutive nodes which both are out of order (#2, #3):
        // |prev| > |cur| and |prev| > |next|.
        // It means that more than one note has been reordered, fall back to
        // generating unique positions for all the remaining children.
        //
        // |current_index| is always not 0 because |prev| cannot be
        // UniquePositionWrapper::Min() if |next| < |prev|.
        DCHECK_GT(current_index, 0u);
        UpdateAllUniquePositionsStartingAt(node, current_index);
        break;
      }
      update_current_position = true;
    } else if (next.LessThan(cur) && prev.LessThan(next)) {
      // |prev| < |next| < |cur| (case #4).
      update_current_position = true;
    }

    if (update_current_position) {
      cur = UniquePositionWrapper::ForValidUniquePosition(
          UpdateUniquePositionForNode(node->children()[current_index].get(),
                                      prev.GetUniquePosition(),
                                      next.GetUniquePosition()));
    }

    DCHECK(prev.LessThan(cur));
    prev = std::move(cur);
    cur = std::move(next);
  }

  nudge_for_commit_closure_.Run();
}

syncer::UniquePosition NotesModelObserverImpl::ComputePosition(
    const vivaldi::NoteNode& parent,
    size_t index) const {
  CHECK_LT(index, parent.children().size());

  const vivaldi::NoteNode* node = parent.children()[index].get();
  const syncer::UniquePosition::Suffix suffix =
      syncer::UniquePosition::GenerateSuffix(
          SyncedNoteTracker::GetClientTagHashFromUuid(node->uuid()));

  const SyncedNoteTrackerEntity* predecessor_entity = nullptr;
  const SyncedNoteTrackerEntity* successor_entity = nullptr;

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
            successor_entity->metadata().unique_position()),
        suffix);
  }

  if (predecessor_entity && !successor_entity) {
    // No successor, insert after the predecessor
    return syncer::UniquePosition::After(
        syncer::UniquePosition::FromProto(
            predecessor_entity->metadata().unique_position()),
        suffix);
  }

  // Both predecessor and successor, insert in the middle.
  return syncer::UniquePosition::Between(
      syncer::UniquePosition::FromProto(
          predecessor_entity->metadata().unique_position()),
      syncer::UniquePosition::FromProto(
          successor_entity->metadata().unique_position()),
      suffix);
}

void NotesModelObserverImpl::ProcessDelete(const vivaldi::NoteNode* node,
                                           const base::Location& location) {
  // If not a leaf node, process all children first.
  for (const auto& child : node->children()) {
    ProcessDelete(child.get(), location);
  }
  // Process the current node.
  const SyncedNoteTrackerEntity* entity =
      note_tracker_->GetEntityForNoteNode(node);
  // Shouldn't try to delete untracked entities.
  DCHECK(entity);
  // If the entity hasn't been committed and doesn't have an inflight commit
  // request, simply remove it from the tracker.
  if (entity->metadata().server_version() == syncer::kUncommittedVersion &&
      !entity->commit_may_have_started()) {
    note_tracker_->Remove(entity);
    return;
  }
  note_tracker_->MarkDeleted(entity, location);
  // Mark the entity that it needs to be committed.
  note_tracker_->IncrementSequenceNumber(entity);
}

syncer::UniquePosition NotesModelObserverImpl::GetUniquePositionForNode(
    const vivaldi::NoteNode* node) const {
  DCHECK(note_tracker_);
  DCHECK(node);
  const SyncedNoteTrackerEntity* entity =
      note_tracker_->GetEntityForNoteNode(node);
  DCHECK(entity);
  return syncer::UniquePosition::FromProto(
      entity->metadata().unique_position());
}

syncer::UniquePosition NotesModelObserverImpl::UpdateUniquePositionForNode(
    const vivaldi::NoteNode* node,
    const syncer::UniquePosition& prev,
    const syncer::UniquePosition& next) {
  CHECK(note_tracker_);
  CHECK(node);

  const SyncedNoteTrackerEntity* entity =
      note_tracker_->GetEntityForNoteNode(node);
  CHECK(entity);
  const syncer::UniquePosition::Suffix suffix =
      syncer::UniquePosition::GenerateSuffix(entity->GetClientTagHash());
  const base::Time modification_time = base::Time::Now();

  syncer::UniquePosition new_unique_position;
  if (prev.IsValid() && next.IsValid()) {
    new_unique_position = syncer::UniquePosition::Between(prev, next, suffix);
  } else if (prev.IsValid()) {
    new_unique_position = syncer::UniquePosition::After(prev, suffix);
  } else {
    new_unique_position = syncer::UniquePosition::Before(next, suffix);
  }

  sync_pb::EntitySpecifics specifics = CreateSpecificsFromNoteNode(
      node, note_model_, new_unique_position.ToProto());
  note_tracker_->Update(entity, entity->metadata().server_version(),
                        modification_time, specifics);

  // Mark the entity that it needs to be committed.
  note_tracker_->IncrementSequenceNumber(entity);
  return new_unique_position;
}

void NotesModelObserverImpl::UpdateAllUniquePositionsStartingAt(
    const vivaldi::NoteNode* parent,
    size_t start_index) {
  DCHECK_GT(start_index, 0u);
  DCHECK_LT(start_index, parent->children().size());

  syncer::UniquePosition prev =
      GetUniquePositionForNode(parent->children()[start_index - 1].get());
  for (size_t current_index = start_index;
       current_index < parent->children().size(); ++current_index) {
    // Right position is unknown because it will also be updated.
    prev = UpdateUniquePositionForNode(parent->children()[current_index].get(),
                                       prev,
                                       /*next=*/syncer::UniquePosition());
  }
}

}  // namespace sync_notes
