// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notes/note_specifics_conversions.h"

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/guid.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/sync/engine/entity_data.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync_bookmarks/switches.h"
#include "notes/note_node.h"
#include "notes/notes_model.h"
#include "url/gurl.h"

namespace sync_notes {

namespace {

// Maximum number of bytes to allow in a legacy canonicalized title (must
// match sync's internal limits; see write_node.cc).
const int kLegacyCanonicalizedTitleLimitBytes = 255;

// The list of node titles which are reserved for use by the server.
const char* const kForbiddenTitles[] = {"", ".", ".."};

// This is an exact copy of the same code in note_update_preprocessing.cc.
// TODO(crbug.com/1032052): Remove when client tags are adopted in
// ModelTypeWorker.
std::string ComputeGuidFromBytes(base::span<const uint8_t> bytes) {
  DCHECK_GE(bytes.size(), 16U);

  // This implementation is based on the equivalent logic in base/guid.cc.

  // Set the GUID to version 4 as described in RFC 4122, section 4.4.
  // The format of GUID version 4 must be xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx,
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

// This is an exact copy of the same code in note_update_preprocessing.cc.
// TODO(crbug.com/1032052): Remove when client tags are adopted in
// ModelTypeWorker.
std::string InferGuidForLegacyNote(
    const std::string& originator_cache_guid,
    const std::string& originator_client_item_id) {
  DCHECK(
      !base::GUID::ParseCaseInsensitive(originator_client_item_id).is_valid());

  const std::string unique_tag =
      base::StrCat({originator_cache_guid, originator_client_item_id});
  const base::SHA1Digest hash =
      base::SHA1HashSpan(base::as_bytes(base::make_span(unique_tag)));

  static_assert(base::kSHA1Length >= 16, "16 bytes needed to infer GUID");

  const std::string guid = ComputeGuidFromBytes(base::make_span(hash));
  DCHECK(base::GUID::ParseLowercase(guid).is_valid());
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
  if (remote_entity_data.specifics.notes().has_full_title() &&
      !remote_entity_data.is_note_guid_in_specifics_preprocessed) {
    return false;
  }
  return base::FeatureList::IsEnabled(
      switches::kSyncReuploadBookmarkFullTitles);
}

sync_pb::EntitySpecifics CreateSpecificsFromNoteNode(
    const vivaldi::NoteNode* node,
    vivaldi::NotesModel* model) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::NotesSpecifics* notes_specifics = specifics.mutable_notes();
  if (!node->is_folder() && !node->is_separator()) {
    notes_specifics->set_url(node->GetURL().spec());
    notes_specifics->set_content(base::UTF16ToUTF8(node->GetContent()));
    if (!node->GetAttachments().empty())
      for (const auto& attachment : node->GetAttachments()) {
        auto* attachment_specifics = notes_specifics->add_attachments();
        attachment_specifics->set_checksum(attachment.second.checksum());
      }
  }

  DCHECK(node->guid().is_valid()) << "Actual: " << node->guid();
  notes_specifics->set_guid(node->guid().AsLowercaseString());

  const std::string node_title = base::UTF16ToUTF8(node->GetTitle());
  notes_specifics->set_legacy_canonicalized_title(
      FullTitleToLegacyCanonicalizedTitle(node_title));
  notes_specifics->set_full_title(node_title);
  notes_specifics->set_creation_time_us(
      node->GetCreationTime().ToDeltaSinceWindowsEpoch().InMicroseconds());

  if (node->is_separator())
    notes_specifics->set_special_node_type(sync_pb::NotesSpecifics::SEPARATOR);

  return specifics;
}

const vivaldi::NoteNode* CreateNoteNodeFromSpecifics(
    const sync_pb::NotesSpecifics& specifics,
    const vivaldi::NoteNode* parent,
    size_t index,
    bool is_folder,
    vivaldi::NotesModel* model) {
  DCHECK(parent);
  DCHECK(model);
  DCHECK(!is_folder ||
         specifics.special_node_type() != sync_pb::NotesSpecifics::SEPARATOR);

  base::GUID guid = base::GUID::ParseLowercase(specifics.guid());
  DCHECK(guid.is_valid());

  const vivaldi::NoteNode* node;
  if (is_folder) {
    node = model->AddFolder(parent, index, NodeTitleFromSpecifics(specifics),
                            guid);
  } else if (specifics.special_node_type() ==
             sync_pb::NotesSpecifics::SEPARATOR) {
    const int64_t create_time_us = specifics.creation_time_us();
    base::Time create_time = base::Time::FromDeltaSinceWindowsEpoch(
        // Use FromDeltaSinceWindowsEpoch because create_time_us has
        // always used the Windows epoch.
        base::TimeDelta::FromMicroseconds(create_time_us));
    node = model->AddSeparator(parent, index, NodeTitleFromSpecifics(specifics),
                               create_time, guid);
  } else {
    const int64_t create_time_us = specifics.creation_time_us();
    base::Time create_time = base::Time::FromDeltaSinceWindowsEpoch(
        // Use FromDeltaSinceWindowsEpoch because create_time_us has
        // always used the Windows epoch.
        base::TimeDelta::FromMicroseconds(create_time_us));
    node = model->AddNote(
        parent, index, NodeTitleFromSpecifics(specifics), GURL(specifics.url()),
        base::UTF8ToUTF16(specifics.content()), create_time, guid);

    for (auto it : specifics.attachments()) {
      if (!it.has_checksum())
        continue;
      model->AddAttachment(node, vivaldi::NoteAttachment(it.checksum(), ""));
    }
  }
  return node;
}

void UpdateNoteNodeFromSpecifics(const sync_pb::NotesSpecifics& specifics,
                                 const vivaldi::NoteNode* node,
                                 vivaldi::NotesModel* model) {
  DCHECK(node);
  DCHECK(model);
  // We shouldn't try to update the properties of the NoteNode before
  // resolving any conflict in GUID. Either GUIDs are the same, or the GUID in
  // specifics is invalid, and hence we can ignore it.
  base::GUID guid = base::GUID::ParseLowercase(specifics.guid());
  DCHECK(!guid.is_valid() || guid == node->guid());

  if (!node->is_folder() && !node->is_separator()) {
    model->SetURL(node, GURL(specifics.url()));
    model->SetContent(node, base::UTF8ToUTF16(specifics.content()));
  }

  model->SetTitle(node, NodeTitleFromSpecifics(specifics));

  for (auto it : specifics.attachments()) {
    if (!it.has_checksum())
      continue;
    if (!node->GetAttachment(it.checksum()))
      model->AddAttachment(node, vivaldi::NoteAttachment(it.checksum(), ""));
  }
}

const vivaldi::NoteNode* ReplaceNoteNodeGUID(const vivaldi::NoteNode* node,
                                             const base::GUID& guid,
                                             vivaldi::NotesModel* model) {
  DCHECK(guid.is_valid());

  if (node->guid() == guid) {
    // Nothing to do.
    return node;
  }

  const vivaldi::NoteNode* new_node = nullptr;
  if (node->is_folder()) {
    new_node =
        model->AddFolder(node->parent(), node->parent()->GetIndexOf(node),
                         node->GetTitle(), guid);
  } else if (node->is_separator()) {
    new_node =
        model->AddSeparator(node->parent(), node->parent()->GetIndexOf(node),
                            node->GetTitle(), node->GetCreationTime(), guid);
  } else {
    new_node = model->AddNote(
        node->parent(), node->parent()->GetIndexOf(node), node->GetTitle(),
        node->GetURL(), node->GetContent(), node->GetCreationTime(), guid);
    model->SwapAttachments(new_node, node);
  }
  for (size_t i = node->children().size(); i > 0; --i) {
    model->Move(node->children()[i - 1].get(), new_node, 0);
  }
  model->Remove(node);

  return new_node;
}

bool IsValidNotesSpecifics(const sync_pb::NotesSpecifics& specifics,
                           bool is_folder) {
  bool is_valid = true;
  if (specifics.ByteSize() == 0) {
    DLOG(ERROR) << "Invalid note: empty specifics.";
    is_valid = false;
  }
  if (is_folder &&
      specifics.special_node_type() == sync_pb::NotesSpecifics::SEPARATOR) {
    DLOG(ERROR) << "Invalid note: can't be both a folder and a separator.";
    is_valid = false;
  }
  base::GUID guid = base::GUID::ParseLowercase(specifics.guid());
  if (!guid.is_valid()) {
    DLOG(ERROR) << "Invalid note: invalid GUID in the specifics.";
    is_valid = false;
  }

  return is_valid;
}

base::GUID InferGuidFromLegacyOriginatorId(
    const std::string& originator_cache_guid,
    const std::string& originator_client_item_id) {
  // notes created around 2016, between [M44..M52) use an uppercase GUID
  // as originator client item ID, so it requires case-insensitive parsing.
  base::GUID guid = base::GUID::ParseCaseInsensitive(originator_client_item_id);
  if (guid.is_valid()) {
    return guid;
  }

  return base::GUID::ParseLowercase(InferGuidForLegacyNote(
      originator_cache_guid, originator_client_item_id));
}

bool HasExpectedNoteGuid(const sync_pb::NotesSpecifics& specifics,
                         const syncer::ClientTagHash& client_tag_hash,
                         const std::string& originator_cache_guid,
                         const std::string& originator_client_item_id) {
  DCHECK(base::GUID::ParseLowercase(specifics.guid()).is_valid());

  // If the client tag hash matches, that should already be good enough.
  if (syncer::ClientTagHash::FromUnhashed(
          syncer::NOTES, specifics.guid()) == client_tag_hash) {
    return true;
  }

  // Earlier vivaldi versions were mistakenly using the BOOKMARKS type here,
  // so we temporarily produce tags using the BOOKMARKS type and allow it.
  // Remove this in a few version. 07-2021
  if (syncer::ClientTagHash::FromUnhashed(
          syncer::BOOKMARKS, specifics.guid()) == client_tag_hash) {
    return true;
  }

  if (base::GUID::ParseCaseInsensitive(originator_client_item_id).is_valid()) {
    // Notes created around 2016, between [M44..M52) use an uppercase GUID
    // as originator client item ID, so it needs to be lowercased to adhere to
    // the invariant that GUIDs in specifics are canonicalized.
    return specifics.guid() == base::ToLowerASCII(originator_client_item_id);
  }

  return specifics.guid() == InferGuidForLegacyNote(originator_cache_guid,
                                                    originator_client_item_id);
}

}  // namespace sync_notes
