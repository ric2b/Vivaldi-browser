// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_hash_util.h"

#include "base/base64.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/protocol/proto_enum_conversions.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

syncer::UniquePosition::Suffix GenerateSyncableNotesHash(
    const std::string& originator_cache_guid,
    const std::string& originator_client_item_id) {
  // Blank PB with just the field in it has termination symbol,
  // handy for delimiter.
  sync_pb::EntitySpecifics serialized_type;
  AddDefaultFieldValue(NOTES, &serialized_type);
  std::string hash_input;
  serialized_type.AppendToString(&hash_input);
  hash_input.append(originator_cache_guid + originator_client_item_id);

  return syncer::UniquePosition::GenerateSuffix(
      syncer::ClientTagHash::FromUnhashed(
          NOTES, originator_cache_guid + originator_client_item_id));
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
                     ATTACHMENT);
  switch (special_type) {
    ENUM_CASE(sync_pb::NotesSpecifics, NORMAL);
    ENUM_CASE(sync_pb::NotesSpecifics, SEPARATOR);
    ENUM_CASE(sync_pb::NotesSpecifics, FOLDER);
    ENUM_CASE(sync_pb::NotesSpecifics, ATTACHMENT);
  }
  NOTREACHED();
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
}

}  // namespace syncer
