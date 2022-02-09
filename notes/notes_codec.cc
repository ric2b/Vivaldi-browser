// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "notes/notes_codec.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/guid.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "notes/note_node.h"
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
const char NotesCodec::kURLKey[] = "url";
const char NotesCodec::kChildrenKey[] = "children";
const char NotesCodec::kContentKey[] = "content";
const char NotesCodec::kAttachmentsKey[] = "attachments";
const char NotesCodec::kSyncMetadata[] = "sync_metadata";
const char NotesCodec::kTypeNote[] = "note";
const char NotesCodec::kTypeFolder[] = "folder";
const char NotesCodec::kTypeSeparator[] = "separator";
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
  main.SetIntKey(kVersionKey, kCurrentVersion);
  FinalizeChecksum();
  // We are going to store the computed checksum. So set stored checksum to be
  // the same as computed checksum.
  stored_checksum_ = computed_checksum_;
  main.SetStringKey(kChecksumKey, computed_checksum_);
  if (!sync_metadata_str.empty()) {
    std::string sync_metadata_str_base64;
    base::Base64Encode(sync_metadata_str, &sync_metadata_str_base64);
    main.SetKey(kSyncMetadata,
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
  guids_ = {base::GUID::ParseLowercase(NoteNode::kRootNodeGuid),
            base::GUID::ParseLowercase(NoteNode::kMainNodeGuid),
            base::GUID::ParseLowercase(NoteNode::kOtherNotesNodeGuid),
            base::GUID::ParseLowercase(NoteNode::kTrashNodeGuid)};
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
  base::Value value(base::Value::Type::DICTIONARY);
  std::string node_id = base::NumberToString(node->id());
  value.SetStringKey(kIdKey, node_id);
  UpdateChecksum(node_id);

  std::u16string subject = node->GetTitle();
  value.SetStringKey(kSubjectKey, subject);
  UpdateChecksum(subject);

  const std::string& guid = node->guid().AsLowercaseString();
  value.SetStringKey(kGuidKey, guid);

  std::string type;
  bool is_folder = false;
  switch (node->type()) {
    case NoteNode::FOLDER:
    case NoteNode::MAIN:
      type = kTypeFolder;
      is_folder = true;
      break;
    case NoteNode::NOTE:
      type = kTypeNote;
      break;
    case NoteNode::TRASH:
      type = kTypeTrash;
      is_folder = true;
      break;
    case NoteNode::OTHER:
      type = kTypeOther;
      is_folder = true;
      break;
    case NoteNode::SEPARATOR:
      type = kTypeSeparator;
      break;
    default:
      NOTREACHED();
      break;
  }
  value.SetStringKey(kTypeKey, type);
  UpdateChecksum(type);

  value.SetStringKey(
      kDateAddedKey,
      base::NumberToString(node->GetCreationTime().ToInternalValue()));

  if (is_folder) {
    base::Value child_list(base::Value::Type::LIST);

    for (const auto& child : node->children()) {
      child_list.Append(EncodeNode(child.get(), nullptr));
    }
    if (extra_nodes) {
      for (const auto* child : *extra_nodes) {
        child_list.Append(EncodeNode(child, nullptr));
      }
    }
    value.SetKey(kChildrenKey, std::move(child_list));
  } else if (node->type() == NoteNode::NOTE) {
    value.SetStringKey(kContentKey, node->GetContent());
    UpdateChecksum(node->GetContent());

    std::string url = node->GetURL().possibly_invalid_spec();
    value.SetStringKey(kURLKey, url);
    UpdateChecksum(url);

    if (node->GetAttachments().size()) {
      base::Value attachments(base::Value::Type::LIST);

      for (const auto& attachment : node->GetAttachments()) {
        attachments.Append(attachment.second.Encode(this));
      }

      value.SetKey(kAttachmentsKey, std::move(attachments));
    }
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

  absl::optional<int> version = value.FindIntKey(kVersionKey);
  if (!version || *version > kCurrentVersion)
    return false;  // Unknown version.

  const base::Value* checksum_value = value.FindKey(kChecksumKey);
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
        value.FindStringKey(kSyncMetadata);
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
    return false;
  }

  // It's not valid to have both a node and a specified parent.
  if (node && parent) {
    NOTREACHED();
    return false;
  }

  std::string id_string;
  int64_t id = 0;
  if (ids_valid_) {
    const std::string* string = value.FindStringKey(kIdKey);
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
  const std::string* string_value = value.FindStringKey(kSubjectKey);
  if (string_value) {
    title = base::UTF8ToUTF16(*string_value);
    UpdateChecksum(title);
  }

  base::GUID guid;
  // |node| is only passed in for notes of type NotePermanentNode, in
  // which case we do not need to check for GUID validity as their GUIDs are
  // hard-coded and not read from the persisted file.
  if (!node) {
    // GUIDs can be empty for notes that were created before GUIDs were
    // required. When encountering one such note we thus assign to it a new
    // GUID. The same applies if the stored GUID is invalid or a duplicate.
    const std::string* guid_str = value.FindStringKey(kGuidKey);
    if (guid_str && !guid_str->empty()) {
      guid = base::GUID::ParseCaseInsensitive(*guid_str);
    }

    if (!guid.is_valid()) {
      guid = base::GUID::GenerateRandomV4();
      guids_reassigned_ = true;
    }

    if (guid.AsLowercaseString() == NoteNode::kBannedGuidDueToPastSyncBug) {
      guid = base::GUID::GenerateRandomV4();
      guids_reassigned_ = true;
    }

    // Guard against GUID collisions, which would violate BookmarkModel's
    // invariant that each GUID is unique.
    if (base::Contains(guids_, guid)) {
      guid = base::GUID::GenerateRandomV4();
      guids_reassigned_ = true;
    }

    guids_.insert(guid);
  }

  base::Time creation_time = base::Time::Now();
  string_value = value.FindStringKey(kDateAddedKey);
  if (string_value) {
    int64_t internal_time;
    if (base::StringToInt64(*string_value, &internal_time)) {
      creation_time = base::Time::FromInternalValue(internal_time);
    }
  }

  const std::string* type_string = value.FindStringKey(kTypeKey);
  if (!type_string)
    return false;

  NoteNode::Type type = NoteNode::NOTE;
  if (*type_string == kTypeNote)
    type = NoteNode::NOTE;
  else if (*type_string == kTypeSeparator)
    type = NoteNode::SEPARATOR;
  else if (*type_string == kTypeFolder)
    type = NoteNode::FOLDER;
  else if (!node || (*type_string != kTypeOther && *type_string != kTypeTrash))
    // We can't create a permanent node when loading.
    return false;
  UpdateChecksum(*type_string);

  if (*type_string == kTypeNote) {
    const std::string* content_string = value.FindStringKey(kContentKey);
    if (!content_string)
      return false;

    if (!node) {
      DCHECK(guid.is_valid());
      node = new NoteNode(id, guid, type);
    } else {
      return false;
    }

    node->SetContent(base::UTF8ToUTF16(*content_string));
    UpdateChecksum(node->GetContent());

    const std::string* url_string = value.FindStringKey(kURLKey);
    if (url_string)
      node->SetURL(GURL(*url_string));
    UpdateChecksum(node->GetURL().possibly_invalid_spec());

    const base::Value* attachments = value.FindListKey(kAttachmentsKey);
    if (attachments) {
      for (const auto& attachment : attachments->GetList()) {
        if (!attachment.is_dict())
          continue;
        std::unique_ptr<NoteAttachment> item(
            NoteAttachment::Decode(attachment, this));
        if (item)
          node->AddAttachment(std::move(*item));
      }
    }

    if (parent)
      parent->Add(base::WrapUnique(node));
  } else if (*type_string != kTypeSeparator) {
    const base::Value* child_list = value.FindListKey(kChildrenKey);

    if (!child_list)
      return false;

    if (!node) {
      DCHECK(guid.is_valid());
      node = new NoteNode(id, guid, type);
    } else {
      node->set_id(id);
    }

    if (parent)
      parent->Add(base::WrapUnique(node));

    for (const auto& child_value : child_list->GetList()) {
      if (!child_value.is_dict())
        return false;

      const std::string* type_string2 = child_value.FindStringKey(kTypeKey);
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
  } else {
    DCHECK(*type_string == kTypeSeparator);

    if (!node) {
      DCHECK(guid.is_valid());

      node = new NoteNode(id, guid, type);
    } else {
      node->set_id(id);
    }

    if (parent)
      parent->Add(base::WrapUnique(node));
  }

  node->SetTitle(title);
  node->SetCreationTime(creation_time);

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
  base::StringPiece temp(reinterpret_cast<const char*>(str.data()),
                         str.length() * sizeof(str[0]));
  base::MD5Update(&md5_context_,
                  base::StringPiece(reinterpret_cast<const char*>(str.data()),
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
