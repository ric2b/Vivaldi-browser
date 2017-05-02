// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/fake_server/notes_entity.h"

#include <string>

#include "base/guid.h"
#include "components/sync/base/model_type.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/fake_server/fake_server_entity.h"

using std::string;

namespace fake_server {

namespace {

// Returns true if and only if |client_entity| is a notes.
bool IsNotes(const sync_pb::SyncEntity& client_entity) {
  return syncer::GetModelType(client_entity) == syncer::NOTES;
}

}  // namespace

NotesEntity::~NotesEntity() {}

// static
std::unique_ptr<FakeServerEntity> NotesEntity::CreateNew(
    const sync_pb::SyncEntity& client_entity,
    const string& parent_id,
    const string& client_guid) {
  CHECK(client_entity.version() == 0) << "New entities must have version = 0.";
  CHECK(IsNotes(client_entity)) << "The given entity must be a notes.";

  string id = FakeServerEntity::CreateId(syncer::NOTES, base::GenerateGUID());
  string originator_cache_guid = client_guid;
  string originator_client_item_id = client_entity.id_string();

  return std::unique_ptr<FakeServerEntity>(new NotesEntity(
      id, client_entity.version(), client_entity.name(), originator_cache_guid,
      originator_client_item_id, client_entity.unique_position(),
      client_entity.specifics(), client_entity.folder(), parent_id,
      client_entity.ctime(), client_entity.mtime()));
}

// static
std::unique_ptr<FakeServerEntity> NotesEntity::CreateUpdatedVersion(
    const sync_pb::SyncEntity& client_entity,
    FakeServerEntity& current_server_entity,
    const string& parent_id) {
  CHECK(client_entity.version() != 0) << "Existing entities must not have a "
                                      << "version = 0.";
  CHECK(IsNotes(client_entity)) << "The given entity must be a notes.";

  NotesEntity* current_notes_entity =
      static_cast<NotesEntity*>(&current_server_entity);
  string originator_cache_guid = current_notes_entity->originator_cache_guid_;
  string originator_client_item_id =
      current_notes_entity->originator_client_item_id_;

  return std::unique_ptr<FakeServerEntity>(new NotesEntity(
      client_entity.id_string(), client_entity.version(), client_entity.name(),
      originator_cache_guid, originator_client_item_id,
      client_entity.unique_position(), client_entity.specifics(),
      client_entity.folder(), parent_id, client_entity.ctime(),
      client_entity.mtime()));
}

NotesEntity::NotesEntity(const string& id,
                         int64_t version,
                         const string& name,
                         const string& originator_cache_guid,
                         const string& originator_client_item_id,
                         const sync_pb::UniquePosition& unique_position,
                         const sync_pb::EntitySpecifics& specifics,
                         bool is_folder,
                         const string& parent_id,
                         int64_t creation_time,
                         int64_t last_modified_time)
    : FakeServerEntity(id, string(), syncer::NOTES, version, name),
      originator_cache_guid_(originator_cache_guid),
      originator_client_item_id_(originator_client_item_id),
      unique_position_(unique_position),
      specifics_(specifics),
      is_folder_(is_folder),
      parent_id_(parent_id),
      creation_time_(creation_time),
      last_modified_time_(last_modified_time) {}

bool NotesEntity::RequiresParentId() const {
  return true;
}

string NotesEntity::GetParentId() const {
  return parent_id_;
}

void NotesEntity::SerializeAsProto(sync_pb::SyncEntity* sync_entity) const {
  FakeServerEntity::SerializeBaseProtoFields(sync_entity);

  sync_pb::EntitySpecifics* specifics = sync_entity->mutable_specifics();
  specifics->CopyFrom(specifics_);

  sync_entity->set_originator_cache_guid(originator_cache_guid_);
  sync_entity->set_originator_client_item_id(originator_client_item_id_);

  sync_entity->set_parent_id_string(parent_id_);
  sync_entity->set_ctime(creation_time_);
  sync_entity->set_mtime(last_modified_time_);

  sync_pb::UniquePosition* unique_position =
      sync_entity->mutable_unique_position();
  unique_position->CopyFrom(unique_position_);
}

bool NotesEntity::IsDeleted() const {
  return false;
}

bool NotesEntity::IsFolder() const {
  return is_folder_;
}

}  // namespace fake_server
