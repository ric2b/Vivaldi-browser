// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/fake_server/notes_entity.h"

#include <memory>
#include <string>

#include "base/uuid.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/loopback_server.pb.h"
#include "components/sync/protocol/sync.pb.h"

using std::string;

namespace syncer {

namespace {

// Returns true if and only if |client_entity| is a notes.
bool IsNotes(const sync_pb::SyncEntity& client_entity) {
  return syncer::GetDataTypeFromSpecifics(client_entity.specifics()) ==
         syncer::NOTES;
}

}  // namespace

PersistentNotesEntity::~PersistentNotesEntity() {}

// static
std::unique_ptr<LoopbackServerEntity> PersistentNotesEntity::CreateNew(
    const sync_pb::SyncEntity& client_entity,
    const string& parent_id,
    const string& client_guid) {
  CHECK(client_entity.version() == 0) << "New entities must have version = 0.";
  CHECK(IsNotes(client_entity)) << "The given entity must be a notes.";

  string id =
      LoopbackServerEntity::CreateId(syncer::NOTES, base::Uuid::GenerateRandomV4().AsLowercaseString());
  string originator_cache_guid = client_guid;
  string originator_client_item_id = client_entity.id_string();

  return std::unique_ptr<LoopbackServerEntity>(new PersistentNotesEntity(
      id, client_entity.version(), client_entity.name(), originator_cache_guid,
      originator_client_item_id, client_entity.unique_position(),
      client_entity.specifics(), client_entity.folder(), parent_id,
      client_entity.ctime(), client_entity.mtime()));
}

// static
std::unique_ptr<LoopbackServerEntity>
PersistentNotesEntity::CreateUpdatedVersion(
    const sync_pb::SyncEntity& client_entity,
    const LoopbackServerEntity& current_server_entity,
    const string& parent_id) {
  CHECK(client_entity.version() != 0) << "Existing entities must not have a "
                                      << "version = 0.";
  CHECK(IsNotes(client_entity)) << "The given entity must be a notes.";

  const PersistentNotesEntity* current_notes_entity =
      static_cast<const PersistentNotesEntity*>(&current_server_entity);
  string originator_cache_guid = current_notes_entity->originator_cache_guid_;
  string originator_client_item_id =
      current_notes_entity->originator_client_item_id_;

  return std::unique_ptr<LoopbackServerEntity>(new PersistentNotesEntity(
      client_entity.id_string(), client_entity.version(), client_entity.name(),
      originator_cache_guid, originator_client_item_id,
      client_entity.unique_position(), client_entity.specifics(),
      client_entity.folder(), parent_id, client_entity.ctime(),
      client_entity.mtime()));
}

// static
std::unique_ptr<LoopbackServerEntity> PersistentNotesEntity::CreateFromEntity(
    const sync_pb::SyncEntity& client_entity) {
  CHECK(IsNotes(client_entity)) << "The given entity must be a notes.";

  return std::unique_ptr<LoopbackServerEntity>(new PersistentNotesEntity(
      client_entity.id_string(), client_entity.version(), client_entity.name(),
      client_entity.originator_cache_guid(),
      client_entity.originator_client_item_id(),
      client_entity.unique_position(), client_entity.specifics(),
      client_entity.folder(), client_entity.parent_id_string(),
      client_entity.ctime(), client_entity.mtime()));
}

PersistentNotesEntity::PersistentNotesEntity(
    const string& id,
    int64_t version,
    const string& name,
    const string& originator_cache_guid,
    const string& originator_client_item_id,
    const sync_pb::UniquePosition& unique_position,
    const sync_pb::EntitySpecifics& specifics,
    bool is_folder,
    const string& parent_id,
    int64_t creation_time,
    int64_t last_modification_time)
    : LoopbackServerEntity(id, syncer::NOTES, version, name),
      originator_cache_guid_(originator_cache_guid),
      originator_client_item_id_(originator_client_item_id),
      unique_position_(unique_position),
      is_folder_(is_folder),
      parent_id_(parent_id),
      creation_time_(creation_time),
      last_modification_time_(last_modification_time) {
  SetSpecifics(specifics);
}

bool PersistentNotesEntity::RequiresParentId() const {
  return true;
}

string PersistentNotesEntity::GetParentId() const {
  return parent_id_;
}

sync_pb::LoopbackServerEntity_Type
PersistentNotesEntity::GetLoopbackServerEntityType() const {
  return sync_pb::LoopbackServerEntity_Type_NOTES;
}

void PersistentNotesEntity::SerializeAsProto(
    sync_pb::SyncEntity* sync_entity) const {
  LoopbackServerEntity::SerializeBaseProtoFields(sync_entity);

  sync_entity->set_originator_cache_guid(originator_cache_guid_);
  sync_entity->set_originator_client_item_id(originator_client_item_id_);

  sync_entity->set_parent_id_string(parent_id_);
  sync_entity->set_ctime(creation_time_);
  sync_entity->set_mtime(last_modification_time_);

  sync_pb::UniquePosition* unique_position =
      sync_entity->mutable_unique_position();
  unique_position->CopyFrom(unique_position_);
}

bool PersistentNotesEntity::IsDeleted() const {
  return false;
}

bool PersistentNotesEntity::IsFolder() const {
  return is_folder_;
}

}  // namespace syncer
