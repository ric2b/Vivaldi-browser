// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notes/parent_guid_preprocessing.h"

#include <memory>
#include <string_view>
#include <unordered_map>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "components/notes/note_node.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/notes_specifics.pb.h"
#include "sync/notes/synced_note_tracker.h"
#include "sync/notes/synced_note_tracker_entity.h"

namespace sync_notes {

namespace {

// The tag used in the sync protocol to identity well-known permanent folders.
const char kMainNotesTag[] = "main_notes";
const char kOtherNotesTag[] = "other_notes";
const char kTrashNotesTag[] = "trash_notes";

// Fake GUID used to populate field |NotesSpecifics.parent_guid| for the case
// where a parent is specified in |SyncEntity.parent_id| but the parent's
// precise GUID could not be determined. Doing this is mostly relevant for UMA
// metrics. The precise GUID used in this string was generated using the same
// technique as the well-known GUIDs in vivaldi::NoteNode, using the name
// "unknown_parent_guid". The precise value is irrelevant though and can be
// changed since all updates using the parent GUID will be ignored in practice.
const char kInvalidParentGuid[] = "6792ca27-fde4-5142-a2b8-22d0bca99227";

bool NeedsParentGuidInSpecifics(const syncer::UpdateResponseData& update) {
  return !update.entity.is_deleted() &&
         update.entity.legacy_parent_id != std::string("0") &&
         update.entity.server_defined_unique_tag.empty() &&
         !update.entity.specifics.notes().has_parent_guid();
}

// Tried to use the information known by |tracker| to determine the GUID of the
// parent folder, for the entity updated in |update|. Returns an invalid GUID
// if the GUID could not be determined. |tracker| must not be null.
base::Uuid TryGetParentGuidFromTracker(
    const SyncedNoteTracker* tracker,
    const syncer::UpdateResponseData& update) {
  DCHECK(tracker);
  DCHECK(!update.entity.is_deleted());
  DCHECK(!update.entity.legacy_parent_id.empty());
  DCHECK(update.entity.server_defined_unique_tag.empty());
  DCHECK(!update.entity.specifics.notes().has_parent_guid());

  const SyncedNoteTrackerEntity* const tracked_parent =
      tracker->GetEntityForSyncId(update.entity.legacy_parent_id);
  if (!tracked_parent) {
    // Parent not known by tracker.
    return base::Uuid();
  }

  if (!tracked_parent->note_node()) {
    // Parent is a tombstone; cannot determine the GUID.
    return base::Uuid();
  }

  return tracked_parent->note_node()->uuid();
}

std::string_view GetGuidForEntity(const syncer::EntityData& entity) {
  // Special-case permanent folders, which may not include a GUID in specifics.
  if (entity.server_defined_unique_tag == kMainNotesTag) {
    return vivaldi::NoteNode::kMainNodeUuid;
  }
  if (entity.server_defined_unique_tag == kOtherNotesTag) {
    return vivaldi::NoteNode::kOtherNotesNodeUuid;
  }
  if (entity.server_defined_unique_tag == kTrashNotesTag) {
    return vivaldi::NoteNode::kTrashNodeUuid;
  }
  // Fall back to the regular case, i.e. GUID in specifics, or an empty value
  // if not present (including tombstones).
  return entity.specifics.notes().guid();
}

// Map from sync IDs (server-provided entity IDs) to GUIDs. The
// returned map uses std::string_view that rely on the lifetime of the strings
// in |updates|. |updates| must not be null.
class LazySyncIdToGuidMapInUpdates {
 public:
  // |updates| must not be null and must outlive this object.
  explicit LazySyncIdToGuidMapInUpdates(
      const syncer::UpdateResponseDataList* updates)
      : updates_(updates) {
    DCHECK(updates_);
  }

  LazySyncIdToGuidMapInUpdates(const LazySyncIdToGuidMapInUpdates&) = delete;
  LazySyncIdToGuidMapInUpdates& operator=(const LazySyncIdToGuidMapInUpdates&) =
      delete;

  std::string_view GetGuidForSyncId(const std::string& sync_id) {
    InitializeIfNeeded();
    auto it = sync_id_to_guid_map_.find(sync_id);
    if (it == sync_id_to_guid_map_.end()) {
      return std::string_view();
    }
    return it->second;
  }

 private:
  void InitializeIfNeeded() {
    if (initialized_) {
      return;
    }
    initialized_ = true;
    for (const syncer::UpdateResponseData& update : *updates_) {
      std::string_view guid = GetGuidForEntity(update.entity);
      if (!update.entity.id.empty() && !guid.empty()) {
        const bool success =
            sync_id_to_guid_map_.emplace(update.entity.id, guid).second;
        DCHECK(success);
      }
    }
  }

  const raw_ptr<const syncer::UpdateResponseDataList> updates_;
  bool initialized_ = false;
  std::unordered_map<std::string_view, std::string_view> sync_id_to_guid_map_;
};

base::Uuid GetParentGuidForUpdate(
    const syncer::UpdateResponseData& update,
    const SyncedNoteTracker* tracker,
    LazySyncIdToGuidMapInUpdates* sync_id_to_guid_map_in_updates) {
  DCHECK(tracker);
  DCHECK(sync_id_to_guid_map_in_updates);

  if (update.entity.legacy_parent_id.empty()) {
    // Without the |SyncEntity.parent_id| field set, there is no information
    // available to determine the parent and/or its GUID.
    return base::Uuid();
  }

  // If a tracker is available, i.e. initial sync already done, it may know
  // parent's GUID already.
  base::Uuid uuid = TryGetParentGuidFromTracker(tracker, update);
  if (uuid.is_valid()) {
    return uuid;
  }

  // Otherwise, fall back to checking if the parent is included in the full list
  // of updates, represented here by |sync_id_to_guid_map_in_updates|. This
  // codepath is most crucial for initial sync, where |tracker| is empty, but is
  // also useful for non-initial sync, if the same incoming batch creates both
  // parent and child, none of which would be known by |tracker|.
  uuid = base::Uuid::ParseLowercase(
      sync_id_to_guid_map_in_updates->GetGuidForSyncId(
          update.entity.legacy_parent_id));
  if (uuid.is_valid()) {
    return uuid;
  }

  // At this point the parent's GUID couldn't be determined, but actually
  // the |SyncEntity.parent_id| was non-empty. The update will be ignored
  // regardless, but to avoid behavioral differences in UMA metrics, a fake
  // parent GUID is used here, which is known to never match an existing entity.
  uuid = base::Uuid::ParseLowercase(kInvalidParentGuid);
  DCHECK(uuid.is_valid());
  DCHECK(!tracker->GetEntityForUuid(uuid));
  return uuid;
}

// Same as PopulateParentGuidInSpecifics(), but |tracker| must not be null.
void PopulateParentGuidInSpecificsWithTracker(
    const SyncedNoteTracker* tracker,
    syncer::UpdateResponseDataList* updates) {
  DCHECK(tracker);
  DCHECK(updates);

  LazySyncIdToGuidMapInUpdates sync_id_to_guid_map(updates);

  for (syncer::UpdateResponseData& update : *updates) {
    // Only legacy data, without the parent GUID in specifics populated,
    // requires work. This also excludes tombstones and permanent folders.
    if (!NeedsParentGuidInSpecifics(update)) {
      // No work needed.
      continue;
    }

    const base::Uuid uuid =
        GetParentGuidForUpdate(update, tracker, &sync_id_to_guid_map);
    if (uuid.is_valid()) {
      update.entity.specifics.mutable_notes()->set_parent_guid(
          uuid.AsLowercaseString());
    }
  }
}

}  // namespace

void PopulateParentGuidInSpecifics(const SyncedNoteTracker* tracker,
                                   syncer::UpdateResponseDataList* updates) {
  DCHECK(updates);

  if (tracker) {
    // The code in this file assumes permanent folders are tracked in
    // SyncedNoteTracker. Since this is prone to change in the future, the
    // DCHECK below is added to avoid subtle bugs, without relying exclusively
    // on integration tests that exercise legacy data..
    DCHECK(tracker->GetEntityForUuid(
        base::Uuid::ParseLowercase(vivaldi::NoteNode::kMainNodeUuid)));
    DCHECK(tracker->GetEntityForUuid(
        base::Uuid::ParseLowercase(vivaldi::NoteNode::kOtherNotesNodeUuid)));
    DCHECK(tracker->GetEntityForUuid(
        base::Uuid::ParseLowercase(vivaldi::NoteNode::kTrashNodeUuid)));

    PopulateParentGuidInSpecificsWithTracker(tracker, updates);
    return;
  }

  // No tracker provided, so use an empty tracker instead where all lookups will
  // fail.
  std::unique_ptr<SyncedNoteTracker> empty_tracker =
      SyncedNoteTracker::CreateEmpty(sync_pb::DataTypeState(), nullptr);
  PopulateParentGuidInSpecificsWithTracker(empty_tracker.get(), updates);
}

std::string GetGuidForSyncIdInUpdatesForTesting(  // IN-TEST
    const syncer::UpdateResponseDataList& updates,
    const std::string& sync_id) {
  LazySyncIdToGuidMapInUpdates sync_id_to_guid_map(&updates);
  return std::string(sync_id_to_guid_map.GetGuidForSyncId(sync_id));
}

}  // namespace sync_notes
