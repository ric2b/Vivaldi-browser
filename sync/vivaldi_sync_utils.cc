// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_hash_util.h"

#include "base/logging.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/engine_impl/syncer_util.h"
#include "components/sync/protocol/proto_enum_conversions.h"
#include "components/sync/syncable/base_transaction.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/model_neutral_mutable_entry.h"
#include "components/sync/syncable/read_node.h"

namespace syncer {

std::string GenerateSyncableNotesHash(
    const std::string& originator_cache_guid,
    const std::string& originator_client_item_id) {
  return GenerateSyncableHash(
      NOTES, originator_cache_guid + originator_client_item_id);
}

std::string GetUniqueNotesTagFromUpdate(const sync_pb::SyncEntity& update) {
  if (!update.has_originator_cache_guid() ||
      !update.has_originator_client_item_id()) {
    LOG(ERROR) << "Update is missing requirements for notes position."
               << " This is a server bug.";
    return UniquePosition::RandomSuffix();
  }

  return GenerateSyncableNotesHash(update.originator_cache_guid(),
                                   update.originator_client_item_id());
}

void UpdateNotesPositioning(const sync_pb::SyncEntity& update,
                            syncable::ModelNeutralMutableEntry* local_entry) {
  // Update our unique notes tag.  In many cases this will be identical to
  // the tag we already have.  However, clients that have recently upgraded to
  // versions that support unique positions will have incorrect tags.  See the
  // v86 migration logic in directory_backing_store.cc for more information.
  //
  // Both the old and new values are unique to this element.  Applying this
  // update will not risk the creation of conflicting unique tags.
  std::string notes_tag = GetUniqueNotesTagFromUpdate(update);
  if (UniquePosition::IsValidSuffix(notes_tag)) {
    local_entry->PutUniqueNotesTag(notes_tag);
  }

  // Update our position.
  UniquePosition update_pos =
      GetUpdatePosition(update, local_entry->GetUniqueNotesTag());
  if (update_pos.IsValid()) {
    local_entry->PutServerUniquePosition(update_pos);
  }
}

// SyncClient

Profile* SyncClient::GetProfile() {
  return nullptr;
}

vivaldi::Notes_Model* SyncClient::GetNotesModel() {
  return nullptr;
}

// ProtoEnumToString
// Defines From chromium
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#define ASSERT_ENUM_BOUNDS(enum_parent, enum_type, enum_min, enum_max) \
  static_assert(enum_parent::enum_type##_MIN == enum_parent::enum_min, \
                #enum_type "_MIN should be " #enum_min);               \
  static_assert(enum_parent::enum_type##_MAX == enum_parent::enum_max, \
                #enum_type "_MAX should be " #enum_max);

#define ENUM_CASE(enum_parent, enum_value) \
  case enum_parent::enum_value:            \
    return #enum_value
// End chromium

const char* ProtoEnumToString(
    sync_pb::NotesSpecifics::VivaldiSpecialNotesType special_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::NotesSpecifics, VivaldiSpecialNotesType, NORMAL,
                     SEPARATOR);
  switch (special_type) {
    ENUM_CASE(sync_pb::NotesSpecifics, NORMAL);
    ENUM_CASE(sync_pb::NotesSpecifics, TRASH_NODE);
    ENUM_CASE(sync_pb::NotesSpecifics, SEPARATOR);
  }
  NOTREACHED();
  return "";
}

const char* ProtoEnumToString(
    sync_pb::BookmarkSpecifics::VivaldiSpecialBookmarkType special_type) {
  ASSERT_ENUM_BOUNDS(sync_pb::BookmarkSpecifics, VivaldiSpecialBookmarkType,
                     NORMAL, TRASH_NODE);
  switch (special_type) {
    ENUM_CASE(sync_pb::BookmarkSpecifics, NORMAL);
    ENUM_CASE(sync_pb::BookmarkSpecifics, TRASH_NODE);
  }
  NOTREACHED();
  return "";
}

// ModelNeutralMutableEntry
namespace syncable {
void ModelNeutralMutableEntry::PutUniqueNotesTag(const std::string& tag) {
  // This unique tag will eventually be used as the unique suffix when adjusting
  // this bookmark's position.  Let's make sure it's a valid suffix.
  if (!UniquePosition::IsValidSuffix(tag)) {
    NOTREACHED();
    return;
  }

  if (!kernel_->ref(UNIQUE_NOTES_TAG).empty() &&
      tag != kernel_->ref(UNIQUE_NOTES_TAG)) {
    // There is only one scenario where our tag is expected to change.  That
    // scenario occurs when our current tag is a non-correct tag assigned during
    // the UniquePosition migration.
    std::string migration_generated_tag = GenerateSyncableNotesHash(
        std::string(), kernel_->ref(ID).GetServerId());
    DCHECK_EQ(migration_generated_tag, kernel_->ref(UNIQUE_NOTES_TAG));
  }

  kernel_->put(UNIQUE_NOTES_TAG, tag);
  kernel_->mark_dirty(&dir()->kernel()->dirty_metahandles);
}
}  // namespace syncable

// ReadNode
BaseNode::InitByLookupResult ReadNode::InitByTagLookupForNotes(
    const std::string& tag) {
  DCHECK(!entry_) << "Init called twice";
  if (tag.empty())
    return INIT_FAILED_PRECONDITION;
  syncable::BaseTransaction* trans = transaction_->GetWrappedTrans();
  entry_ = new syncable::Entry(trans, syncable::GET_BY_SERVER_TAG, tag);
  if (!entry_->good())
    return INIT_FAILED_ENTRY_NOT_GOOD;
  if (entry_->GetIsDel())
    return INIT_FAILED_ENTRY_IS_DEL;
  ModelType model_type = GetModelType();
  DCHECK_EQ(model_type, NOTES)
      << "InitByTagLookup deprecated for all types except notes.";
  return DecryptIfNecessary() ? INIT_OK : INIT_FAILED_DECRYPT_IF_NECESSARY;
}

}  // namespace syncer
