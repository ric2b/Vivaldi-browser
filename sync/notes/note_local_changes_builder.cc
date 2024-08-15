// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notes/note_local_changes_builder.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/notes/note_node.h"
#include "components/sync/base/time.h"
#include "components/sync/base/unique_position.h"
#include "sync/notes/note_model_view.h"
#include "sync/notes/note_specifics_conversions.h"
#include "sync/notes/synced_note_tracker.h"
#include "sync/notes/synced_note_tracker_entity.h"

namespace sync_notes {

NoteLocalChangesBuilder::NoteLocalChangesBuilder(
    SyncedNoteTracker* const note_tracker,
    NoteModelView* notes_model)
    : note_tracker_(note_tracker), notes_model_(notes_model) {
  DCHECK(note_tracker);
  DCHECK(notes_model);
}

syncer::CommitRequestDataList NoteLocalChangesBuilder::BuildCommitRequests(
    size_t max_entries) const {
  DCHECK(note_tracker_);

  const std::vector<const SyncedNoteTrackerEntity*>
      entities_with_local_changes =
          note_tracker_->GetEntitiesWithLocalChanges();

  syncer::CommitRequestDataList commit_requests;
  for (const SyncedNoteTrackerEntity* entity : entities_with_local_changes) {
    if (commit_requests.size() >= max_entries) {
      break;
    }

    DCHECK(entity);
    DCHECK(entity->IsUnsynced());
    const sync_pb::EntityMetadata& metadata = entity->metadata();

    auto data = std::make_unique<syncer::EntityData>();
    data->id = metadata.server_id();
    data->creation_time = syncer::ProtoTimeToTime(metadata.creation_time());
    data->modification_time =
        syncer::ProtoTimeToTime(metadata.modification_time());

    DCHECK(!metadata.client_tag_hash().empty());
    data->client_tag_hash =
        syncer::ClientTagHash::FromHashed(metadata.client_tag_hash());
    // Earlier vivaldi versions were mistakenly using the BOOKMARKS type to
    // verify the type, so we temporarily produce tags using the BOOKMARKS
    // type. Change this to NOTES in a few version. 07-2021
    DCHECK(metadata.is_deleted() ||
           data->client_tag_hash ==
               syncer::ClientTagHash::FromUnhashed(
                   syncer::BOOKMARKS,
                   entity->note_node()->uuid().AsLowercaseString()));

    if (metadata.is_deleted()) {
      // Absence of deletion origin is primarily needed for pre-existing
      // tombstones in storage before this field was introduced. Nevertheless,
      // it seems best to treat it as optional here, in case some codepaths
      // don't provide it in the future.
      if (metadata.has_deletion_origin()) {
        data->deletion_origin = metadata.deletion_origin();
      }
    } else {
      const vivaldi::NoteNode* node = entity->note_node();
      DCHECK(node);
      DCHECK(!node->is_permanent_node());

      // Earlier vivaldi versions were mistakenly using the BOOKMARKS type to
      // verify the type, so we temporarily produce tags using the BOOKMARKS
      // type. Change this to NOTES in a few version. 07-2021
      DCHECK_EQ(syncer::ClientTagHash::FromUnhashed(
                    syncer::BOOKMARKS, node->uuid().AsLowercaseString()),
                syncer::ClientTagHash::FromHashed(metadata.client_tag_hash()));

      const vivaldi::NoteNode* parent = node->parent();
      const SyncedNoteTrackerEntity* parent_entity =
          note_tracker_->GetEntityForNoteNode(parent);
      DCHECK(parent_entity);
      data->legacy_parent_id = parent_entity->metadata().server_id();
      // Assign specifics only for the non-deletion case. In case of deletion,
      // EntityData should contain empty specifics to indicate deletion.
      data->specifics = CreateSpecificsFromNoteNode(node, notes_model_,
                                                    metadata.unique_position());
      // TODO(crbug.com/40677937): check after finishing if we need to use full
      // title instead of legacy canonicalized one.
      data->name = data->specifics.notes().legacy_canonicalized_title();
    }

    auto request = std::make_unique<syncer::CommitRequestData>();
    request->entity = std::move(data);
    request->sequence_number = metadata.sequence_number();
    request->base_version = metadata.server_version();
    // Specifics hash has been computed in the tracker when this entity has been
    // added/updated.
    request->specifics_hash = metadata.specifics_hash();

    if (!metadata.is_deleted()) {
      const vivaldi::NoteNode* node = entity->note_node();
      CHECK(node);
      request->deprecated_note_folder = node->is_folder();
      request->deprecated_note_unique_position =
          syncer::UniquePosition::FromProto(metadata.unique_position());
    }

    note_tracker_->MarkCommitMayHaveStarted(entity);

    commit_requests.push_back(std::move(request));
  }
  return commit_requests;
}

}  // namespace sync_notes
