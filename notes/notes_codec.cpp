// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "notes/notes_codec.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>

#include "base/json/json_string_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using base::Time;
using std::cout;

namespace vivaldi {

const char* NotesCodec::kRootsKey = "roots";
const char* NotesCodec::kVersionKey = "version";
const char* NotesCodec::kChecksumKey = "checksum";
const char* NotesCodec::kIdKey = "id";
const char* NotesCodec::kNameKey = "subject";
const char* NotesCodec::kChildrenKey = "children";
const char* NotesCodec::kSyncTransactionVersion = "sync_transaction_version";

// Current version of the file.
static const int kCurrentVersion = 1;

NotesCodec::NotesCodec()
    : ids_reassigned_(false),
      ids_valid_(true),
      maximum_id_(0),
      model_sync_transaction_version_(
          Notes_Node::kInvalidSyncTransactionVersion) {}

NotesCodec::~NotesCodec() {}

void NotesCodec::register_id(int64_t id) {
  if (ids_.count(id) != 0) {
    ids_valid_ = false;
    return;
  }
  ids_.insert(id);
}

std::unique_ptr<base::Value> NotesCodec::Encode(Notes_Model* model) {
  return Encode(model->main_node(), model->other_node(), model->trash_node(),
                model->root_node()->sync_transaction_version());
}

std::unique_ptr<base::Value> NotesCodec::Encode(const Notes_Node* notes_node,
                                const Notes_Node* other_notes_node,
                                const Notes_Node* trash_notes_node,
                                int64_t sync_transaction_version) {
  ids_reassigned_ = false;
  InitializeChecksum();

  std::vector<const Notes_Node*> extra_nodes;
  extra_nodes.push_back(other_notes_node);
  extra_nodes.push_back(trash_notes_node);

  std::unique_ptr<base::Value> roots(notes_node->Encode(this, &extra_nodes));

  std::unique_ptr<base::DictionaryValue>
      main(base::DictionaryValue::From(std::move(roots)));
  if (main) {
    if (sync_transaction_version != Notes_Node::kInvalidSyncTransactionVersion) {
      main->SetString(kSyncTransactionVersion,
                      base::Int64ToString(sync_transaction_version));
    }
    main->SetInteger(kVersionKey, kCurrentVersion);
    FinalizeChecksum();
    // We are going to store the computed checksum. So set stored checksum to be
    // the same as computed checksum.
    stored_checksum_ = computed_checksum_;
    main->Set(kChecksumKey,
              base::WrapUnique(new base::Value(computed_checksum_)));
  }
  return main;
}

bool NotesCodec::Decode(Notes_Node* notes_node,
                        Notes_Node* other_notes_node,
                        Notes_Node* trash_notes_node,
                        int64_t* max_id,
                        const base::Value& value) {
  ids_.clear();
  ids_reassigned_ = false;
  ids_valid_ = true;
  maximum_id_ = 0;
  stored_checksum_.clear();
  InitializeChecksum();
  bool success =
      DecodeHelper(notes_node, other_notes_node, trash_notes_node, value);
  FinalizeChecksum();
  // If either the checksums differ or some IDs were missing/not unique,
  // reassign IDs.
  if (!ids_valid_ || computed_checksum() != stored_checksum()) {
    ReassignIDs(notes_node, other_notes_node, trash_notes_node);
  }
  *max_id = maximum_id_ + 1;
  return success;
}

void NotesCodec::ExtractSpecialNode(Notes_Node::Type type,
                                    Notes_Node* source,
                                    Notes_Node* target) {
  DCHECK(source);
  DCHECK(target);

  std::unique_ptr<Notes_Node> item;
  for (int i = 0; i < source->child_count(); ++i) {
    Notes_Node* child = source->GetChild(i);
    if (child->type() == type) {
      // Remove the special childnode from the node, moving into a separate node
      item = source->Remove(child);
      break;
    }
  }

  if (item) {
    // Using variable instead of relying on child_count() decrement
    int count = item->child_count();
    while (count-- > 0) {
      target->Add(item->Remove(item->GetChild(0)), 0);
    }
    target->set_id(item->id());
    target->SetTitle(item->GetTitle());
    target->SetCreationTime(item->GetCreationTime());
    item.reset();
  }
}

bool NotesCodec::DecodeHelper(Notes_Node* notes_node,
                              Notes_Node* other_notes_node,
                              Notes_Node* trash_node,
                              const base::Value& value) {
  if (value.type() != base::Value::Type::DICTIONARY)
    return false;  // Unexpected type.

  const base::DictionaryValue& d_value =
      static_cast<const base::DictionaryValue&>(value);

  int version = 0;
  if (d_value.HasKey(kVersionKey) &&
      (!d_value.GetInteger(kVersionKey, &version) || version > kCurrentVersion))
    return false;  // Unknown version.

  const base::Value* checksum_value;
  if (version && d_value.Get(kChecksumKey, &checksum_value)) {
    if (checksum_value->type() != base::Value::Type::STRING)
      return false;
    if (!checksum_value->GetAsString(&stored_checksum_))
      return false;
  }

  notes_node->Decode(d_value, &maximum_id_, this);
  // Other and trash nodes are included inside the normal notes node during
  // encoding; extract them so that they can be placed in the root node
  ExtractSpecialNode(Notes_Node::OTHER, notes_node, other_notes_node);
  ExtractSpecialNode(Notes_Node::TRASH, notes_node, trash_node);

  std::string sync_transaction_version_str;
  if (d_value.GetString(kSyncTransactionVersion,
                        &sync_transaction_version_str) &&
      !base::StringToInt64(sync_transaction_version_str,
                           &model_sync_transaction_version_))
    return false;

  return true;
}

void NotesCodec::ReassignIDs(Notes_Node* notes_node,
                             Notes_Node* other_node,
                             Notes_Node* trash_node) {
  maximum_id_ = 0;
  ReassignIDsHelper(notes_node);
  ReassignIDsHelper(other_node);
  ReassignIDsHelper(trash_node);
  ids_reassigned_ = true;
}

void NotesCodec::ReassignIDsHelper(Notes_Node* node) {
  DCHECK(node);
  node->set_id(++maximum_id_);
  for (int i = 0; i < node->child_count(); ++i)
    ReassignIDsHelper(node->GetChild(i));
}

void NotesCodec::UpdateChecksum(const std::string& str) {
  base::MD5Update(&md5_context_, str);
}

void NotesCodec::UpdateChecksum(const base::string16& str) {
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
