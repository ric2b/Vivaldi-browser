// Copyright (c) 2013-2014 Vivaldi Technologies AS. All rights reserved

#include "notes/notesnode.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

#include "notes/notes_codec.h"

namespace vivaldi {

const int64_t Notes_Node::kInvalidSyncTransactionVersion = -1;

Notes_Node::Notes_Node(int64_t id)
    : type_(NOTE),
      creation_time_(
          base::Time::Now()),  // This will be overwritten if read from file.
      id_(id),
      sync_transaction_version_(kInvalidSyncTransactionVersion) {}

Notes_Node::~Notes_Node() {}

void Notes_Node::SetType(Type type) {
  type_ = type;
  if (type == SEPARATOR && GetTitle().empty())
    // Makes it easier to match for Sync
    SetTitle(base::UTF8ToUTF16("--- SEPARATOR ") +
             base::NumberToString16(creation_time_.ToInternalValue()));
}

std::unique_ptr<base::Value> Notes_Node::Encode(
    NotesCodec* checksummer,
    const std::vector<const Notes_Node*>* extra_nodes) const {
  DCHECK(checksummer);

  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());

  std::string node_id = base::Int64ToString(id_);
  value->SetString("id", node_id);
  checksummer->UpdateChecksum(node_id);

  base::string16 subject = GetTitle();
  value->SetString("subject", subject);
  checksummer->UpdateChecksum(subject);

  std::string type;
  switch (type_) {
    case FOLDER:
      type = "folder";
      break;
    case NOTE:
      type = "note";
      break;
    case TRASH:
      type = "trash";
      break;
    case OTHER:
      type = "other";
      break;
    case SEPARATOR:
      type = "separator";
      break;
    default:
      NOTREACHED();
      break;
  }
  value->SetString("type", type);
  checksummer->UpdateChecksum(type);

  std::string temp = base::Int64ToString(creation_time_.ToInternalValue());
  value->SetString("date_added", temp);

  if (type_ == FOLDER || type_ == TRASH || type_ == OTHER) {
    std::unique_ptr<base::ListValue> children(new base::ListValue());

    for (int i = 0; i < child_count(); i++) {
      children->Append(GetChild(i)->Encode(checksummer));
    }
    if (extra_nodes) {
      for (auto* it : *extra_nodes) {
        children->Append(it->Encode(checksummer));
      }
    }
    value->Set("children", std::move(children));
  } else if (type_ == NOTE) {
    value->SetString("content", content_);
    checksummer->UpdateChecksum(content_);

    temp = url_.possibly_invalid_spec();
    value->SetString("url", temp);
    checksummer->UpdateChecksum(temp);

    if (attachments_.size()) {
      std::unique_ptr<base::ListValue> attachments(new base::ListValue());

      for (const auto& attachment : attachments_) {
        attachments->Append(attachment.second.Encode(checksummer));
      }

      value->Set("attachments", std::move(attachments));
    }
  }

  if (sync_transaction_version() != kInvalidSyncTransactionVersion) {
    value->SetString(NotesCodec::kSyncTransactionVersion,
                     base::Int64ToString(sync_transaction_version()));
  }
  return value;
}

bool Notes_Node::Decode(const base::DictionaryValue& d_input,
                        int64_t* max_node_id,
                        NotesCodec* checksummer) {
  DCHECK(checksummer);

  base::string16 str;
  std::string id_string;
  int64_t id = 0;
  if (checksummer->ids_valid()) {
    if (!d_input.GetString("id", &id_string) ||
        !base::StringToInt64(id_string, &id) ||
        checksummer->count_id(id) != 0) {
      checksummer->set_ids_valid(false);
    } else {
      id_ = id;
      checksummer->register_id(id);
    }
  }
  checksummer->UpdateChecksum(id_string);

  *max_node_id = std::max(*max_node_id, id);

  if (d_input.GetString("subject", &str)) {
    SetTitle(str);
    checksummer->UpdateChecksum(str);
  }

  std::string str1;
  if (d_input.GetString("date_added", &str1)) {
    int64_t temp_time;
    base::StringToInt64(str1, &temp_time);
    // If this is a new note do not refresh the time from disk.
    if (temp_time) {
      creation_time_ = base::Time::FromInternalValue(temp_time);
    }
  } else {
    creation_time_ = base::Time::Now();  // Or should this be 1970?
  }

  std::string temp;
  if (!d_input.GetString("type", &temp)) {
    NOTREACHED();  // We must have type!
    return false;
  }

  if (temp != "folder" && temp != "note" && temp != "trash" &&
      temp != "other" && temp != "separator") {
    return false;
  }
  checksummer->UpdateChecksum(temp);

  if (temp == "note") {
    type_ = NOTE;

    if (!d_input.GetString("content", &content_))
      return false;
    checksummer->UpdateChecksum(content_);

    base::string16 str;

    if (d_input.GetString("url", &str))
      url_ = GURL(str);
    checksummer->UpdateChecksum(url_.possibly_invalid_spec());

    const base::ListValue* attachments = NULL;
    if (d_input.GetList("attachments", &attachments) && attachments) {
      for (size_t i = 0; i < attachments->GetSize(); i++) {
        const base::DictionaryValue* d_att = NULL;
        if (attachments->GetDictionary(i, &d_att)) {
          std::unique_ptr<NoteAttachment> item(
              NoteAttachment::Decode(d_att, checksummer));
          if (!item)
            return false;
          attachments_.insert(
              std::make_pair(item->checksum(), std::move(*item)));
        }
      }
    }
  } else {
    if (temp == "trash")
      type_ = TRASH;
    else if (temp == "other")
      type_ = OTHER;
    else if (temp == "separator")
      type_ = SEPARATOR;
    else
      type_ = FOLDER;

    const base::ListValue* children = NULL;

    if (!d_input.GetList("children", &children))
      return false;

    for (size_t i = 0; i < children->GetSize(); i++) {
      const base::DictionaryValue* item = NULL;

      if (!children->GetDictionary(i, &item))
        return false;

      std::unique_ptr<Notes_Node> child = std::make_unique<Notes_Node>(0);

      child->Decode(*item, max_node_id, checksummer);

      Add(std::move(child), child_count());
    }
  }

  int64_t sync_transaction_version = kInvalidSyncTransactionVersion;
  std::string sync_transaction_version_str;
  if (d_input.GetString(NotesCodec::kSyncTransactionVersion,
                        &sync_transaction_version_str) &&
      !base::StringToInt64(sync_transaction_version_str,
                           &sync_transaction_version))
    return false;

  set_sync_transaction_version(sync_transaction_version);

  return true;
}

}  // namespace vivaldi
