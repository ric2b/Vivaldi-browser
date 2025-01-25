// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/fake_server/notes_entity_builder.h"

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/entity_builder_factory.h"
#include "sync/test/fake_server/notes_entity.h"
#include "sync/vivaldi_hash_util.h"
#include "url/gurl.h"

using std::string;
using syncer::GenerateSyncableNotesHash;
using syncer::LoopbackServerEntity;

// A version must be passed when creating a FakeServerEntity, but this value
// is overrideen immediately when saving the entity in FakeServer.
const int64_t kUnusedVersion = 0L;

// Default time (creation and last modified) used when creating entities.
const int64_t kDefaultTime = 1234L;

namespace fake_server {

NotesEntityBuilder::NotesEntityBuilder(const string& title,
                                       const GURL& url,
                                       const string& content,
                                       const string& originator_cache_guid,
                                       const string& originator_client_item_id)
    : url_(url),
      title_(title),
      content_(content),
      originator_cache_guid_(originator_cache_guid),
      originator_client_item_id_(originator_client_item_id) {}

NotesEntityBuilder::NotesEntityBuilder(const NotesEntityBuilder& old) = default;

NotesEntityBuilder::~NotesEntityBuilder() {}

void NotesEntityBuilder::SetParentId(const std::string& parent_id) {
  parent_id_ = parent_id;
}

std::unique_ptr<LoopbackServerEntity> NotesEntityBuilder::Build() {
  if (!url_.is_valid()) {
    return base::WrapUnique<LoopbackServerEntity>(NULL);
  }

  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::NotesSpecifics* notes_specifics = entity_specifics.mutable_notes();
  notes_specifics->set_legacy_canonicalized_title(title_);
  notes_specifics->set_url(url_.spec());
  notes_specifics->set_content(content_);

  if (parent_id_.empty()) {
    parent_id_ = LoopbackServerEntity::CreateId(syncer::NOTES, "main_notes");
  }

  sync_pb::UniquePosition unique_position;
  // TODO(pvalenzuela): Allow caller customization of the position integer.
  syncer::UniquePosition::Suffix suffix = GenerateSyncableNotesHash(
      originator_cache_guid_,
                                            originator_client_item_id_);
  unique_position = syncer::UniquePosition::FromInt64(0, suffix).ToProto();

  const string id =
      LoopbackServerEntity::CreateId(syncer::NOTES, base::Uuid::GenerateRandomV4().AsLowercaseString());

  return base::WrapUnique<LoopbackServerEntity>(
      new syncer::PersistentNotesEntity(
          id, kUnusedVersion, title_, originator_cache_guid_,
          originator_client_item_id_, unique_position, entity_specifics, false,
          parent_id_, kDefaultTime, kDefaultTime));
}

NotesEntityBuilder EntityBuilderFactory::NewNotesEntityBuilder(
    const string& title,
    const GURL& url,
    const string& content) {
  std::string originator_client_item_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  NotesEntityBuilder builder(title, url, content, cache_guid_,
                             originator_client_item_id);
  return builder;
}

}  // namespace fake_server
