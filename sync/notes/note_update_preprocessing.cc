// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notes/note_update_preprocessing.h"

#include <array>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/uuid.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/protocol/sync.pb.h"
#include "sync/vivaldi_hash_util.h"

namespace syncer {

namespace {

std::string ComputeUuidFromBytes(base::span<const uint8_t> bytes) {
  DCHECK_GE(bytes.size(), 16U);

  // This implementation is based on the equivalent logic in base/guid.cc.

  // Set the UUID to version 4 as described in RFC 4122, section 4.4.
  // The format of UUID version 4 must be xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx,
  // where y is one of [8, 9, A, B].

  // Clear the version bits and set the version to 4:
  const uint8_t byte6 = (bytes[6] & 0x0fU) | 0xf0U;

  // Set the two most significant bits (bits 6 and 7) of the
  // clock_seq_hi_and_reserved to zero and one, respectively:
  const uint8_t byte8 = (bytes[8] & 0x3fU) | 0x80U;

  return base::StringPrintf(
      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], byte6,
      bytes[7], byte8, bytes[9], bytes[10], bytes[11], bytes[12], bytes[13],
      bytes[14], bytes[15]);
}

// Notes created before 2015 (https://codereview.chromium.org/1136953013)
// have an originator client item ID that is NOT a UUID. Hence, an alternative
// method must be used to infer a UUID deterministically from a combination of
// sync fields that is known to be a) immutable and b) unique per synced
// note.
std::string InferGuidForLegacyNote(
    const std::string& originator_cache_guid,
    const std::string& originator_client_item_id) {
  DCHECK(
      !base::Uuid::ParseCaseInsensitive(originator_client_item_id).is_valid());

  const std::string unique_tag =
      base::StrCat({originator_cache_guid, originator_client_item_id});
  const base::SHA1Digest hash = base::SHA1Hash(base::as_byte_span(unique_tag));

  static_assert(base::kSHA1Length >= 16, "16 bytes needed to infer UUID");

  const std::string guid = ComputeUuidFromBytes(base::make_span(hash));
  DCHECK(base::Uuid::ParseLowercase(guid).is_valid());
  return guid;
}

// Legacy method to calculate unique position suffix for the notes which did
// not have client tag hash.
UniquePosition::Suffix GenerateUniquePositionSuffixForNote(
    const std::string& originator_cache_guid,
    const std::string& originator_client_item_id) {
  // Blank PB with just the field in it has termination symbol,
  // handy for delimiter.
  sync_pb::EntitySpecifics serialized_type;
  AddDefaultFieldValue(NOTES, &serialized_type);
  std::string hash_input;
  serialized_type.AppendToString(&hash_input);
  hash_input.append(originator_cache_guid + originator_client_item_id);
  UniquePosition::Suffix suffix;
  std::string suffix_str =
      base::Base64Encode(base::SHA1Hash(base::as_byte_span(hash_input)));
  CHECK_EQ(suffix.size(), suffix_str.size());
  base::ranges::copy(suffix_str, suffix.begin());
  return suffix;
}

sync_pb::UniquePosition GetUniquePositionFromSyncEntity(
    const sync_pb::SyncEntity& update_entity) {
  if (update_entity.has_unique_position()) {
    return update_entity.unique_position();
  }

  syncer::UniquePosition::Suffix suffix;
  if (update_entity.has_originator_cache_guid() &&
      update_entity.has_originator_client_item_id()) {
    suffix = GenerateUniquePositionSuffixForNote(
        update_entity.originator_cache_guid(),
        update_entity.originator_client_item_id());
  } else {
    suffix = UniquePosition::RandomSuffix();
  }

  if (update_entity.has_position_in_parent()) {
    return UniquePosition::FromInt64(update_entity.position_in_parent(), suffix)
        .ToProto();
  }

  if (update_entity.has_insert_after_item_id()) {
    return UniquePosition::FromInt64(0, suffix).ToProto();
  }

  // No positioning information whatsoever, which should be unreachable today.
  // For future-compatibility in case the fields in SyncEntity get removed,
  // let's use a random position, which is better than dropping the whole
  // update.
  return UniquePosition::InitialPosition(suffix).ToProto();
}

}  // namespace

bool AdaptUniquePositionForNote(const sync_pb::SyncEntity& update_entity,
                                sync_pb::EntitySpecifics* specifics) {
  DCHECK(specifics);
  // Nothing to do if the field is set or if it's a deletion.
  if (specifics->notes().has_unique_position() || update_entity.deleted()) {
    return false;
  }

  // Permanent folders don't need positioning information.
  if (update_entity.folder() &&
      !update_entity.server_defined_unique_tag().empty()) {
    return false;
  }

  *specifics->mutable_notes()->mutable_unique_position() =
      GetUniquePositionFromSyncEntity(update_entity);
  return true;
}

void AdaptTypeForNote(const sync_pb::SyncEntity& update_entity,
                      sync_pb::EntitySpecifics* specifics) {
  DCHECK(specifics);
  // Nothing to do if the note is known not to be normal or if it's a deletion.
  if (specifics->notes().special_node_type() !=
          sync_pb::NotesSpecifics::NORMAL ||
      update_entity.deleted()) {
    return;
  }
  DCHECK(specifics->has_notes());
  // For legacy data, SyncEntity.folder is always populated.
  if (update_entity.has_folder()) {
    if (update_entity.folder()) {
      specifics->mutable_notes()->set_special_node_type(
          sync_pb::NotesSpecifics::FOLDER);
    }
    return;
  }
  // Remaining cases should be unreachable today. In case SyncEntity.folder gets
  // removed in the future, with legacy data still being around prior to M94,
  // infer folderness based on the present of field |content| (only populated
  // for normal notes).
  if (!specifics->notes().has_content())
    specifics->mutable_notes()->set_special_node_type(
        sync_pb::NotesSpecifics::FOLDER);
}

void AdaptTitleForNote(const sync_pb::SyncEntity& update_entity,
                       sync_pb::EntitySpecifics* specifics,
                       bool specifics_were_encrypted) {
  DCHECK(specifics);
  if (specifics_were_encrypted || update_entity.deleted()) {
    // If encrypted, the name field is never populated (unencrypted) for privacy
    // reasons. Encryption was also introduced after moving the name out of
    // SyncEntity so this hack is not needed at all.
    return;
  }
  DCHECK(specifics->has_notes());
  // Legacy clients populate the name field in the SyncEntity instead of the
  // title field in the NotesSpecifics.
  if (!specifics->notes().has_legacy_canonicalized_title() &&
      !update_entity.name().empty()) {
    specifics->mutable_notes()->set_legacy_canonicalized_title(
        update_entity.name());
  }
}

void AdaptGuidForNote(const sync_pb::SyncEntity& update_entity,
                      sync_pb::EntitySpecifics* specifics) {
  DCHECK(specifics);
  // Tombstones and permanent entities don't have a UUID.
  if (update_entity.deleted() ||
      !update_entity.server_defined_unique_tag().empty()) {
    return;
  }
  DCHECK(specifics->has_notes());
  // Legacy clients don't populate the guid field in the NotesSpecifics, so
  // we use the originator_client_item_id instead, if it is a valid UUID.
  // Otherwise, we leave the field empty.
  if (specifics->notes().has_guid()) {
    return;
  }
  if (base::Uuid::ParseCaseInsensitive(
          update_entity.originator_client_item_id())
          .is_valid()) {
    // Notes created around 2016, between [M44..M52) use an uppercase UUID
    // as originator client item ID, so it needs to be lowercased to adhere to
    // the invariant that U UIDs in specifics are canonicalized.
    specifics->mutable_notes()->set_guid(
        base::ToLowerASCII(update_entity.originator_client_item_id()));
    DCHECK(base::Uuid::ParseLowercase(specifics->notes().guid()).is_valid());
  } else if (update_entity.originator_cache_guid().empty() &&
             update_entity.originator_client_item_id().empty()) {
    // There's no UUID that could be inferred from empty originator
    // information.
  } else {
    specifics->mutable_notes()->set_guid(
        InferGuidForLegacyNote(update_entity.originator_cache_guid(),
                               update_entity.originator_client_item_id()));
    DCHECK(base::Uuid::ParseLowercase(specifics->notes().guid()).is_valid());
  }
}

std::string InferGuidForLegacyNoteForTesting(
    const std::string& originator_cache_guid,
    const std::string& originator_client_item_id) {
  return InferGuidForLegacyNote(originator_cache_guid,
                                originator_client_item_id);
}
}  // namespace syncer
