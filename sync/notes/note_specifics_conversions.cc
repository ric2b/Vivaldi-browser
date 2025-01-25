// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notes/note_specifics_conversions.h"

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "components/notes/note_node.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/notes_specifics.pb.h"
#include "components/sync_bookmarks/switches.h"
#include "url/gurl.h"
#include "sync/notes/note_model_view.h"

namespace sync_notes {

namespace {

// Maximum number of bytes to allow in a legacy canonicalized title (must
// match sync's internal limits; see write_node.cc).
const int kLegacyCanonicalizedTitleLimitBytes = 255;

// The list of node titles which are reserved for use by the server.
const char* const kForbiddenTitles[] = {"", ".", ".."};

// This is an exact copy of the same code in note_update_preprocessing.cc.
std::string ComputeGuidFromBytes(base::span<const uint8_t> bytes) {
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

// This is an exact copy of the same code in note_update_preprocessing.cc,
// which could be removed if eventually client tags are adapted/inferred in
// DataTypeWorker. The reason why this is non-trivial today is that some users
// are known to contain corrupt data in the sense that several different
// entities (identified by their server-provided ID) use the same client tag
// (and UUID). Currently NoteModelMerger has logic to prefer folders over
// regular URLs and reassign UUIDs.
std::string InferGuidForLegacyNote(
    const std::string& originator_cache_guid,
    const std::string& originator_client_item_id) {
  DCHECK(
      !base::Uuid::ParseCaseInsensitive(originator_client_item_id).is_valid());

  const std::string unique_tag =
      base::StrCat({originator_cache_guid, originator_client_item_id});
  const base::SHA1Digest hash = base::SHA1Hash(base::as_byte_span(unique_tag));

  static_assert(base::kSHA1Length >= 16, "16 bytes needed to infer Uuid");

  const std::string guid = ComputeGuidFromBytes(base::make_span(hash));
  DCHECK(base::Uuid::ParseLowercase(guid).is_valid());
  return guid;
}

bool IsForbiddenTitleWithMaybeTrailingSpaces(const std::string& title) {
  return base::Contains(
      kForbiddenTitles,
      base::TrimWhitespaceASCII(title, base::TrimPositions::TRIM_TRAILING));
}

std::u16string NodeTitleFromSpecifics(
    const sync_pb::NotesSpecifics& specifics) {
  if (specifics.has_full_title()) {
    return base::UTF8ToUTF16(specifics.full_title());
  }
  std::string node_title = specifics.legacy_canonicalized_title();
  if (base::EndsWith(node_title, " ") &&
      IsForbiddenTitleWithMaybeTrailingSpaces(node_title)) {
    // Legacy clients added an extra space to the real title, so remove it here.
    // See also FullTitleToLegacyCanonicalizedTitle().
    node_title.pop_back();
  }
  return base::UTF8ToUTF16(node_title);
}

std::optional<base::Time> NodeLastModificationTimeFromSpecifics(
    const sync_pb::NotesSpecifics& specifics) {
  if (specifics.has_last_modification_time_us()) {
    return base::Time::FromDeltaSinceWindowsEpoch(
        // Use FromDeltaSinceWindowsEpoch because last_modification_time_us
        // has always used the Windows epoch.
        base::Microseconds(specifics.last_modification_time_us()));
  }
  return std::nullopt;
}

void MoveAllChildren(NoteModelView* model,
                     const vivaldi::NoteNode* old_parent,
                     const vivaldi::NoteNode* new_parent) {
  DCHECK(old_parent && (old_parent->is_folder() || old_parent->is_note()));
  DCHECK(new_parent && (new_parent->is_folder() || new_parent->is_note()));
  DCHECK(old_parent->is_folder() == new_parent->is_folder());
  DCHECK(old_parent != new_parent);
  DCHECK(new_parent->children().empty());

  if (old_parent->children().empty()) {
    return;
  }

  // This code relies on the underlying type to store children in the
  // NotesModel which is vector. It moves the last child from |old_parent| to
  // the end of |new_parent| step by step (which reverses the order of
  // children). After that all children must be reordered to keep the original
  // order in |new_parent|.
  // This algorithm is used because of performance reasons.
  std::vector<const vivaldi::NoteNode*> children_order(
      old_parent->children().size(), nullptr);
  for (size_t i = old_parent->children().size(); i > 0; --i) {
    const size_t old_index = i - 1;
    const vivaldi::NoteNode* child_to_move =
        old_parent->children()[old_index].get();
    children_order[old_index] = child_to_move;
    model->Move(child_to_move, new_parent, new_parent->children().size());
  }
  model->ReorderChildren(new_parent, children_order);
}

}  // namespace

std::string FullTitleToLegacyCanonicalizedTitle(const std::string& node_title) {
  // Add an extra space for backward compatibility with legacy clients.
  std::string specifics_title =
      IsForbiddenTitleWithMaybeTrailingSpaces(node_title) ? node_title + " "
                                                          : node_title;
  base::TruncateUTF8ToByteSize(
      specifics_title, kLegacyCanonicalizedTitleLimitBytes, &specifics_title);
  return specifics_title;
}

bool IsNoteEntityReuploadNeeded(const syncer::EntityData& remote_entity_data) {
  DCHECK(remote_entity_data.server_defined_unique_tag.empty());
  // Do not initiate a reupload for a remote deletion.
  if (remote_entity_data.is_deleted()) {
    return false;
  }

  DCHECK(remote_entity_data.specifics.has_notes());
  if (!remote_entity_data.is_note_unique_position_in_specifics_preprocessed) {
    return false;
  }

  return base::FeatureList::IsEnabled(switches::kSyncReuploadBookmarks);
}

sync_pb::EntitySpecifics CreateSpecificsFromNoteNode(
    const vivaldi::NoteNode* node,
    NoteModelView* model,
    const sync_pb::UniquePosition& unique_position) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::NotesSpecifics* notes_specifics = specifics.mutable_notes();

  notes_specifics->set_special_node_type(GetProtoTypeFromNoteNode(node));

  if (!node->is_folder() && !node->is_separator()) {
    notes_specifics->set_url(node->GetURL().spec());
    notes_specifics->set_content(base::UTF16ToUTF8(node->GetContent()));
  }

  DCHECK(node->uuid().is_valid()) << "Actual: " << node->uuid();
  notes_specifics->set_guid(node->uuid().AsLowercaseString());

  DCHECK(node->parent()->uuid().is_valid())
      << "Actual: " << node->parent()->uuid();
  notes_specifics->set_parent_guid(node->parent()->uuid().AsLowercaseString());

  const std::string node_title = base::UTF16ToUTF8(node->GetTitle());
  notes_specifics->set_legacy_canonicalized_title(
      FullTitleToLegacyCanonicalizedTitle(node_title));
  notes_specifics->set_full_title(node_title);
  notes_specifics->set_creation_time_us(
      node->GetCreationTime().ToDeltaSinceWindowsEpoch().InMicroseconds());
  notes_specifics->set_last_modification_time_us(
      node->GetLastModificationTime().ToDeltaSinceWindowsEpoch().InMicroseconds());
  *notes_specifics->mutable_unique_position() = unique_position;

  return specifics;
}

const vivaldi::NoteNode* CreateNoteNodeFromSpecifics(
    const sync_pb::NotesSpecifics& specifics,
    const vivaldi::NoteNode* parent,
    size_t index,
    NoteModelView* model) {
  DCHECK(parent);
  DCHECK(model);
  DCHECK(IsValidNotesSpecifics(specifics));

  const base::Uuid guid = base::Uuid::ParseLowercase(specifics.guid());
  DCHECK(guid.is_valid());

  const base::Uuid parent_guid =
      base::Uuid::ParseLowercase(specifics.parent_guid());
  DCHECK(parent_guid.is_valid());
  DCHECK_EQ(parent_guid, parent->uuid());

  const int64_t creation_time_us = specifics.creation_time_us();
  const base::Time creation_time = base::Time::FromDeltaSinceWindowsEpoch(
      // Use FromDeltaSinceWindowsEpoch because creation_time_us has
      // always used the Windows epoch.
      base::Microseconds(creation_time_us));

  auto last_modification_time =
      NodeLastModificationTimeFromSpecifics(specifics);
  if (!last_modification_time) {
    last_modification_time = creation_time;
  }

  const vivaldi::NoteNode* node;
  switch (specifics.special_node_type()) {
    case sync_pb::NotesSpecifics::NORMAL:
      node = model->AddNote(parent, index, NodeTitleFromSpecifics(specifics),
                            GURL(specifics.url()),
                            base::UTF8ToUTF16(specifics.content()),
                            creation_time, last_modification_time, guid);
      return node;
    case sync_pb::NotesSpecifics::SEPARATOR:
      return model->AddSeparator(parent, index,
                                 NodeTitleFromSpecifics(specifics),
                                 creation_time, guid);
    case sync_pb::NotesSpecifics::ATTACHMENT:
      return model->AddAttachmentFromChecksum(
          parent, index, NodeTitleFromSpecifics(specifics),
          GURL(specifics.url()), specifics.content(), creation_time, guid);
    case sync_pb::NotesSpecifics::FOLDER:
      return model->AddFolder(parent, index, NodeTitleFromSpecifics(specifics),
                              creation_time, last_modification_time, guid);
  }

  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void UpdateNoteNodeFromSpecifics(const sync_pb::NotesSpecifics& specifics,
                                 const vivaldi::NoteNode* node,
                                 NoteModelView* model) {
  DCHECK(node);
  DCHECK(model);
  // We shouldn't try to update the properties of the NoteNode before
  // resolving any conflict in UUID. Either UUIDs are the same, or the UUID in
  // specifics is invalid, and hence we can ignore it.
  base::Uuid guid = base::Uuid::ParseLowercase(specifics.guid());
  DCHECK(!guid.is_valid() || guid == node->uuid());

  if (!node->is_folder() && !node->is_separator()) {
    model->SetURL(node, GURL(specifics.url()));
    model->SetContent(node, base::UTF8ToUTF16(specifics.content()));
  }

  model->SetTitle(node, NodeTitleFromSpecifics(specifics));
  if (const auto last_modification_time =
          NodeLastModificationTimeFromSpecifics(specifics)) {
    model->SetLastModificationTime(node, *last_modification_time);
  }
}

sync_pb::NotesSpecifics::VivaldiSpecialNotesType GetProtoTypeFromNoteNode(
    const vivaldi::NoteNode* node) {
  DCHECK(node);

  switch (node->type()) {
    case vivaldi::NoteNode::NOTE:
      DCHECK(!node->is_folder());
      return sync_pb::NotesSpecifics::NORMAL;
    case vivaldi::NoteNode::SEPARATOR:
      DCHECK(!node->is_folder());
      return sync_pb::NotesSpecifics::SEPARATOR;
    case vivaldi::NoteNode::ATTACHMENT:
      DCHECK(!node->is_folder());
      return sync_pb::NotesSpecifics::ATTACHMENT;
    case vivaldi::NoteNode::FOLDER:
    case vivaldi::NoteNode::MAIN:
    case vivaldi::NoteNode::OTHER:
    case vivaldi::NoteNode::TRASH:
      DCHECK(node->is_folder());
      return sync_pb::NotesSpecifics::FOLDER;
  }
}

const vivaldi::NoteNode* ReplaceNoteNodeUuid(const vivaldi::NoteNode* node,
                                             const base::Uuid& guid,
                                             NoteModelView* model) {
  DCHECK(guid.is_valid());

  if (node->uuid() == guid) {
    // Nothing to do.
    return node;
  }

  const vivaldi::NoteNode* new_node = nullptr;
  if (node->is_folder()) {
    new_node = model->AddFolder(node->parent(),
                                node->parent()->GetIndexOf(node).value(),
                                node->GetTitle(), node->GetCreationTime(),
                                node->GetLastModificationTime(), guid);
    MoveAllChildren(model, node, new_node);
  } else if (node->is_separator()) {
    new_node = model->AddSeparator(
        node->parent(), node->parent()->GetIndexOf(node).value(),
        node->GetTitle(), node->GetCreationTime(), guid);
  } else if (node->is_attachment()) {
    new_node = model->AddAttachmentFromChecksum(
        node->parent(), node->parent()->GetIndexOf(node).value(),
        node->GetTitle(), node->GetURL(),
        base::UTF16ToASCII(node->GetContent()), node->GetCreationTime(), guid);
  } else {
    new_node = model->AddNote(
        node->parent(), node->parent()->GetIndexOf(node).value(),
        node->GetTitle(), node->GetURL(), node->GetContent(),
        node->GetCreationTime(), node->GetLastModificationTime(), guid);
    MoveAllChildren(model, node, new_node);
  }
  model->Remove(node, FROM_HERE);

  return new_node;
}

bool IsValidNotesSpecifics(const sync_pb::NotesSpecifics& specifics) {
  bool is_valid = true;
  if (specifics.ByteSize() == 0) {
    DLOG(ERROR) << "Invalid note: empty specifics.";
    is_valid = false;
  }
  const base::Uuid guid = base::Uuid::ParseLowercase(specifics.guid());

  if (!guid.is_valid()) {
    DLOG(ERROR) << "Invalid note: invalid Uuid in the specifics.";
    is_valid = false;
  } else if (guid.AsLowercaseString() ==
             vivaldi::NoteNode::kBannedUuidDueToPastSyncBug) {
    DLOG(ERROR) << "Invalid note: banned Uuid in specifics.";
    is_valid = false;
  }
  const base::Uuid parent_guid =
      base::Uuid::ParseLowercase(specifics.parent_guid());
  if (!parent_guid.is_valid()) {
    DLOG(ERROR) << "Invalid note: invalid parent Uuid in specifics.";
    is_valid = false;
  }

  if (!syncer::UniquePosition::FromProto(specifics.unique_position())
           .IsValid()) {
    // Ignore updates with invalid positions.
    DLOG(ERROR) << "Invalid note: invalid unique position.";
    is_valid = false;
  }

  return is_valid;
}

base::Uuid InferGuidFromLegacyOriginatorId(
    const std::string& originator_cache_guid,
    const std::string& originator_client_item_id) {
  // notes created around 2016, between [M44..M52) use an uppercase UUID
  // as originator client item ID, so it requires case-insensitive parsing.
  base::Uuid guid = base::Uuid::ParseCaseInsensitive(originator_client_item_id);
  if (guid.is_valid()) {
    return guid;
  }

  return base::Uuid::ParseLowercase(
      InferGuidForLegacyNote(originator_cache_guid, originator_client_item_id));
}

bool HasExpectedNoteGuid(const sync_pb::NotesSpecifics& specifics,
                         const syncer::ClientTagHash& client_tag_hash,
                         const std::string& originator_cache_guid,
                         const std::string& originator_client_item_id) {
  DCHECK(base::Uuid::ParseLowercase(specifics.guid()).is_valid());

  if (!client_tag_hash.value().empty()) {
    // Earlier vivaldi versions were mistakenly using the BOOKMARKS type here,
    // so we temporarily produce tags using the BOOKMARKS type and allow it.
    // Remove this in a few version. 07-2021
    return syncer::ClientTagHash::FromUnhashed(
               syncer::NOTES, specifics.guid()) == client_tag_hash ||
           syncer::ClientTagHash::FromUnhashed(
               syncer::BOOKMARKS, specifics.guid()) == client_tag_hash;
  }

  // Guard against returning true for cases where the UUID cannot be inferred.
  if (originator_cache_guid.empty() && originator_client_item_id.empty()) {
    return false;
  }

  return base::Uuid::ParseLowercase(specifics.guid()) ==
         InferGuidFromLegacyOriginatorId(originator_cache_guid,
                                         originator_client_item_id);
}

}  // namespace sync_notes
