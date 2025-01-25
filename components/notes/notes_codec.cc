// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notes/notes_codec.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/uuid.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/notes/note_node.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using base::Time;
using std::cout;

namespace vivaldi {

const char NotesCodec::kVersionKey[] = "version";
const char NotesCodec::kChecksumKey[] = "checksum";
const char NotesCodec::kIdKey[] = "id";
const char NotesCodec::kTypeKey[] = "type";
const char NotesCodec::kSubjectKey[] = "subject";
const char NotesCodec::kGuidKey[] = "guid";
const char NotesCodec::kDateAddedKey[] = "date_added";
const char NotesCodec::kDateModifiedKey[] = "last_modified";
const char NotesCodec::kURLKey[] = "url";
const char NotesCodec::kChildrenKey[] = "children";
const char NotesCodec::kContentKey[] = "content";
const char NotesCodec::kAttachmentsKey[] = "attachments";
const char NotesCodec::kSyncMetadata[] = "sync_metadata";
const char NotesCodec::kTypeNote[] = "note";
const char NotesCodec::kTypeFolder[] = "folder";
const char NotesCodec::kTypeSeparator[] = "separator";
const char NotesCodec::kTypeAttachment[] = "attachment";
const char NotesCodec::kTypeOther[] = "other";
const char NotesCodec::kTypeTrash[] = "trash";

// Current version of the file.
static const int kCurrentVersion = 1;

NotesCodec::NotesCodec()
    : ids_reassigned_(false), ids_valid_(true), maximum_id_(0) {}

NotesCodec::~NotesCodec() {}

base::Value NotesCodec::Encode(NotesModel* model,
                               const std::string& sync_metadata_str) {
  return Encode(model->main_node(), model->other_node(), model->trash_node(),
                sync_metadata_str);
}

base::Value NotesCodec::Encode(const NoteNode* notes_node,
                               const NoteNode* other_notes_node,
                               const NoteNode* trash_notes_node,
                               const std::string& sync_metadata_str) {
  ids_reassigned_ = false;
  InitializeChecksum();

  std::vector<const NoteNode*> extra_nodes;
  extra_nodes.push_back(other_notes_node);
  extra_nodes.push_back(trash_notes_node);

  base::Value main(EncodeNode(notes_node, &extra_nodes));
  DCHECK(main.is_dict());
  main.GetDict().Set(kVersionKey, kCurrentVersion);
  FinalizeChecksum();
  // We are going to store the computed checksum. So set stored checksum to be
  // the same as computed checksum.
  stored_checksum_ = computed_checksum_;
  main.GetDict().Set(kChecksumKey, computed_checksum_);
  if (!sync_metadata_str.empty()) {
    std::string sync_metadata_str_base64;
    sync_metadata_str_base64 = base::Base64Encode(sync_metadata_str);
    main.GetDict().Set(kSyncMetadata,
                base::Value(std::move(sync_metadata_str_base64)));
  }
  return main;
}

bool NotesCodec::Decode(NoteNode* notes_node,
                        NoteNode* other_notes_node,
                        NoteNode* trash_notes_node,
                        int64_t* max_id,
                        const base::Value& value,
                        std::string* sync_metadata_str) {
  ids_.clear();
  uuids_ = {base::Uuid::ParseLowercase(NoteNode::kRootNodeUuid),
            base::Uuid::ParseLowercase(NoteNode::kMainNodeUuid),
            base::Uuid::ParseLowercase(NoteNode::kOtherNotesNodeUuid),
            base::Uuid::ParseLowercase(NoteNode::kTrashNodeUuid)};
  ids_reassigned_ = false;
  ids_valid_ = true;
  maximum_id_ = 0;
  stored_checksum_.clear();
  InitializeChecksum();
  bool success = DecodeHelper(notes_node, other_notes_node, trash_notes_node,
                              value, sync_metadata_str);
  FinalizeChecksum();
  // If either the checksums differ or some IDs were missing/not unique,
  // reassign IDs.
  if (!ids_valid_ || computed_checksum() != stored_checksum()) {
    ReassignIDs(notes_node, other_notes_node, trash_notes_node);
  }
  *max_id = maximum_id_ + 1;
  return success;
}

base::Value NotesCodec::EncodeNode(
    const NoteNode* node,
    const std::vector<const NoteNode*>* extra_nodes) {
  base::Value value(base::Value::Type::DICT);
  std::string node_id = base::NumberToString(node->id());
  value.GetDict().Set(kIdKey, node_id);
  UpdateChecksum(node_id);

  std::u16string subject = node->GetTitle();
  value.GetDict().Set(kSubjectKey, subject);
  UpdateChecksum(subject);

  const std::string& uuid = node->uuid().AsLowercaseString();
  value.GetDict().Set(kGuidKey, uuid);

  std::string type;
  bool can_have_children = false;
  switch (node->type()) {
    case NoteNode::FOLDER:
    case NoteNode::MAIN:
      type = kTypeFolder;
      can_have_children = true;
      break;
    case NoteNode::NOTE:
      can_have_children = true;
      type = kTypeNote;
      break;
    case NoteNode::TRASH:
      type = kTypeTrash;
      can_have_children = true;
      break;
    case NoteNode::OTHER:
      type = kTypeOther;
      can_have_children = true;
      break;
    case NoteNode::SEPARATOR:
      type = kTypeSeparator;
      break;
    case NoteNode::ATTACHMENT:
      type = kTypeAttachment;
      break;
    default:
      NOTREACHED();
  }
  value.GetDict().Set(kTypeKey, type);
  UpdateChecksum(type);

  value.GetDict().Set(
      kDateAddedKey,
      base::NumberToString(node->GetCreationTime().ToInternalValue()));
  value.GetDict().Set(
      kDateModifiedKey,
      base::NumberToString(node->GetLastModificationTime().ToInternalValue()));

  if (node->type() == NoteNode::NOTE || node->type() == NoteNode::ATTACHMENT) {
    value.GetDict().Set(kContentKey, node->GetContent());
    UpdateChecksum(node->GetContent());

    std::string url = node->GetURL().possibly_invalid_spec();
    value.GetDict().Set(kURLKey, url);
    UpdateChecksum(url);
  }

  if (can_have_children) {
    base::Value child_list(base::Value::Type::LIST);

    for (const auto& child : node->children()) {
      child_list.GetList().Append(EncodeNode(child.get(), nullptr));
    }
    if (extra_nodes) {
      for (const auto* child : *extra_nodes) {
        child_list.GetList().Append(EncodeNode(child, nullptr));
      }
    }
    value.GetDict().Set(kChildrenKey, std::move(child_list));
  }

  return value;
}

bool NotesCodec::DecodeHelper(NoteNode* notes_node,
                              NoteNode* other_notes_node,
                              NoteNode* trash_node,
                              const base::Value& value,
                              std::string* sync_metadata_str) {
  if (!value.is_dict())
    return false;  // Unexpected type.

  std::optional<int> version = value.GetDict().FindInt(kVersionKey);
  if (!version || *version > kCurrentVersion)
    return false;  // Unknown version.

  const base::Value* checksum_value = value.GetDict().Find(kChecksumKey);
  if (checksum_value) {
    const std::string* checksum = checksum_value->GetIfString();
    if (checksum)
      stored_checksum_ = *checksum;
    else
      return false;
  }

  DecodeNode(value, nullptr, notes_node, other_notes_node, trash_node);

  if (sync_metadata_str) {
    const std::string* sync_metadata_str_base64 =
        value.GetDict().FindString(kSyncMetadata);
    if (sync_metadata_str_base64)
      base::Base64Decode(*sync_metadata_str_base64, sync_metadata_str);
  }

  return true;
}

bool NotesCodec::DecodeNode(const base::Value& value,
                            NoteNode* parent,
                            NoteNode* node,
                            NoteNode* child_other_node,
                            NoteNode* child_trash_node) {
  DCHECK(value.is_dict());
  // If no |node| is specified, we'll create one and add it to the |parent|.
  // Therefore, in that case, |parent| must be non-NULL.
  if (!node && !parent) {
    NOTREACHED();
  }

  // It's not valid to have both a node and a specified parent.
  if (node && parent) {
    NOTREACHED();
  }

  std::string id_string;
  int64_t id = 0;
  if (ids_valid_) {
    const std::string* string = value.GetDict().FindString(kIdKey);
    if (!string || !base::StringToInt64(*string, &id) || ids_.count(id) != 0) {
      ids_valid_ = false;
    } else {
      ids_.insert(id);
      id_string = *string;
    }
  }
  UpdateChecksum(id_string);

  maximum_id_ = std::max(maximum_id_, id);

  std::u16string title;
  const std::string* string_value = value.GetDict().FindString(kSubjectKey);
  if (string_value) {
    title = base::UTF8ToUTF16(*string_value);
    UpdateChecksum(title);
  }

  base::Uuid uuid;
  // |node| is only passed in for notes of type NotePermanentNode, in
  // which case we do not need to check for UUID validity as their UUIDs are
  // hard-coded and not read from the persisted file.
  if (!node) {
    // UUIDs can be empty for notes that were created before UUIDs were
    // required. When encountering one such note we thus assign to it a new
    // UUID. The same applies if the stored UUID is invalid or a duplicate.
    const std::string* uuid_str = value.GetDict().FindString(kGuidKey);
    if (uuid_str && !uuid_str->empty()) {
      uuid = base::Uuid::ParseCaseInsensitive(*uuid_str);
    }

    if (!uuid.is_valid()) {
      uuid = base::Uuid::GenerateRandomV4();
      uuids_reassigned_ = true;
    }

    if (uuid.AsLowercaseString() == NoteNode::kBannedUuidDueToPastSyncBug) {
      uuid = base::Uuid::GenerateRandomV4();
      uuids_reassigned_ = true;
    }

    // Guard against UUID collisions, which would violate BookmarkModel's
    // invariant that each UUID is unique.
    if (base::Contains(uuids_, uuid)) {
      uuid = base::Uuid::GenerateRandomV4();
      uuids_reassigned_ = true;
    }

    uuids_.insert(uuid);
  }

  const auto getTimeFromKey = [&value](const auto& key) {
    const std::string* string_value = value.GetDict().FindString(key);
    if (string_value) {
      int64_t internal_time;
      if (base::StringToInt64(*string_value, &internal_time)) {
        return base::Time::FromInternalValue(internal_time);
      }
    }
    return base::Time::Now();
  };
  base::Time creation_time = getTimeFromKey(kDateAddedKey);
  base::Time last_modification_time = getTimeFromKey(kDateModifiedKey);

  const std::string* type_string = value.GetDict().FindString(kTypeKey);
  if (!type_string)
    return false;

  NoteNode::Type type = NoteNode::NOTE;
  if (*type_string == kTypeNote)
    type = NoteNode::NOTE;
  else if (*type_string == kTypeSeparator)
    type = NoteNode::SEPARATOR;
  else if (*type_string == kTypeAttachment)
    type = NoteNode::ATTACHMENT;
  else if (*type_string == kTypeFolder)
    type = NoteNode::FOLDER;
  else if (!node || (*type_string != kTypeOther && *type_string != kTypeTrash))
    // We can't create a permanent node when loading.
    return false;
  UpdateChecksum(*type_string);

  const base::Value::List* child_list = value.GetDict().FindList(kChildrenKey);

  if (*type_string == kTypeNote || *type_string == kTypeAttachment) {
    const std::string* content_string = value.GetDict().FindString(kContentKey);
    if (!content_string)
      return false;

    if (!node) {
      DCHECK(uuid.is_valid());
      node = new NoteNode(id, uuid, type);
    } else {
      return false;
    }

    node->SetContent(base::UTF8ToUTF16(*content_string));
    UpdateChecksum(node->GetContent());

    const std::string* url_string = value.GetDict().FindString(kURLKey);
    if (url_string)
      node->SetURL(GURL(*url_string));
    UpdateChecksum(node->GetURL().possibly_invalid_spec());

    if (*type_string == kTypeNote) {
      const base::Value::List* attachments = value.GetDict().FindList(kAttachmentsKey);
      if (attachments) {
        for (const auto& attachment : *attachments) {
          if (!attachment.is_dict())
            continue;
          std::unique_ptr<DeprecatedNoteAttachment> item(
              DeprecatedNoteAttachment::Decode(attachment, this));
          if (item) {
            node->AddAttachmentDeprecated(std::move(*item));
            has_deprecated_attachments_ = true;
          }
        }
      }
    }
  } else if (*type_string != kTypeSeparator) {
    if (!child_list)
      return false;

    if (!node) {
      DCHECK(uuid.is_valid());
      node = new NoteNode(id, uuid, type);
    } else {
      node->set_id(id);
    }
  } else {
    DCHECK(*type_string == kTypeSeparator);

    if (!node) {
      DCHECK(uuid.is_valid());

      node = new NoteNode(id, uuid, type);
    } else {
      return false;
    }
  }

  if (*type_string != kTypeSeparator && *type_string != kTypeAttachment &&
      child_list) {
    for (const auto& child_value : *child_list) {
      if (!child_value.is_dict())
        return false;

      const std::string* type_string2 = child_value.GetDict().FindString(kTypeKey);
      if (!type_string2)
        return false;
      if (*type_string2 == kTypeOther) {
        if (!child_other_node)
          return false;
        DecodeNode(child_value, nullptr, child_other_node, nullptr, nullptr);
        child_other_node = nullptr;
        continue;
      }
      if (*type_string2 == kTypeTrash) {
        if (!child_trash_node)
          return false;
        DecodeNode(child_value, nullptr, child_trash_node, nullptr, nullptr);
        child_trash_node = nullptr;
        continue;
      }

      DecodeNode(child_value, node, nullptr, nullptr, nullptr);
    }
  }

  if (parent)
    parent->Add(base::WrapUnique(node));
  node->SetTitle(title);
  node->SetCreationTime(creation_time);
  node->SetLastModificationTime(last_modification_time);

  return true;
}

void NotesCodec::ReassignIDs(NoteNode* notes_node,
                             NoteNode* other_node,
                             NoteNode* trash_node) {
  maximum_id_ = 0;
  ReassignIDsHelper(notes_node);
  ReassignIDsHelper(other_node);
  ReassignIDsHelper(trash_node);
  ids_reassigned_ = true;
}

void NotesCodec::ReassignIDsHelper(NoteNode* node) {
  DCHECK(node);
  node->set_id(++maximum_id_);
  for (auto& it : node->children())
    ReassignIDsHelper(it.get());
}

void NotesCodec::UpdateChecksum(const std::string& str) {
  base::MD5Update(&md5_context_, str);
}

void NotesCodec::UpdateChecksum(const std::u16string& str) {
  std::string_view temp(reinterpret_cast<const char*>(str.data()),
                         str.length() * sizeof(str[0]));
  base::MD5Update(&md5_context_,
                  std::string_view(reinterpret_cast<const char*>(str.data()),
                                    str.length() * sizeof(str[0])));
}

void NotesCodec::InitializeChecksum() {
  base::MD5Init(&md5_context_);
}

void NotesCodec::FinalizeChecksum() {
  base::MD5Digest digest;
  base::MD5Final(&digest, &md5_context_);
  computed_checksum_ = base::MD5DigestToBase16(digest);
}

}  // namespace vivaldi
