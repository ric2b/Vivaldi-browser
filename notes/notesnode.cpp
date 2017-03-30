// Copyright (c) 2013-2014 Vivaldi Technologies AS. All rights reserved

#include <string>
#include <vector>
#include "base/base64.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/utf_string_conversions.h"

#include "notes/notesnode.h"

namespace vivaldi {
Notes_Node::Notes_Node(int64_t id)
    : type_(NOTE),
      creation_time_(
          base::Time::Now()),  // This will be overwritten if read from file.
      id_(id)
{}

Notes_Node::~Notes_Node() {}

base::Value *Notes_Node::WriteJSON() const {
  base::DictionaryValue *value = new base::DictionaryValue();

  value->SetString("id", base::Int64ToString(id_));

  value->SetString("subject", GetTitle());

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
    default:
      NOTREACHED();
      break;
  }
  value->SetString("type", type);
  if (type_ == FOLDER || type_ == TRASH) {
    base::ListValue *children = new base::ListValue();

    for (int i = 0; i < child_count(); i++) {
      children->Append(GetChild(i)->WriteJSON());
    }
    value->Set("children", children);
  } else {
    if (!filename_.empty()) {
      value->SetString("filename", filename_);
    }

    if (!subject_.empty()) {
      value->SetString("subject", subject_);
    }
    value->SetString("content", content_);
    value->SetString("url", url_.possibly_invalid_spec());
    value->SetString("date_added",
                     base::Int64ToString(creation_time_.ToInternalValue()));

    if (!note_icon_.content.empty())
      value->Set("icon", note_icon_.WriteJSON());

    if (attachments_.size()) {
      base::ListValue *attachments = new base::ListValue();

      std::vector<Notes_attachment>::const_iterator item;
      for (item = attachments_.begin(); item < attachments_.end(); item++) {
        base::Value *val = item->WriteJSON();
        attachments->Append(val);
      }

      value->Set("attachments", attachments);
    }
  }
  return value;
}

bool Notes_Node::ReadJSON(base::DictionaryValue &input) {
  base::string16 str;

  if (input.GetString("id", &str))
    base::StringToInt64(str, &id_);

  if (input.GetString("subject", &str))
    SetTitle(str);

  std::string temp;
  if (!input.GetString("type", &temp)) {
    NOTREACHED();  // We must have type!
    return false;
  }

  if (temp != "folder" && temp != "note" && temp != "trash")
    return false;

  if (temp == "note") {
    type_ = NOTE;

    if (input.GetString("filename", &filename_)) {
      // OK, load later
    }

    if (!input.GetString("content", &content_))
      return false;

    base::string16 str;
    if (input.GetString("date_added", &str)) {
      int64_t temp_time;
      base::StringToInt64(str, &temp_time);
      // If this is a new note do not refresh the time from disk.
      if (temp_time) {
        creation_time_ = base::Time::FromInternalValue(temp_time);
      }
    } else {
      creation_time_ = base::Time::Now();  // Or should this be 1970?
    }

    if (input.GetString("url", &str))
      url_ = GURL(str);

    const base::DictionaryValue *attachment = NULL;
    if (input.GetDictionary("icon", &attachment) && attachment) {
      if (!note_icon_.ReadJSON(*attachment))
        return false;
    }

    const base::ListValue *attachments = NULL;
    if (input.GetList("attachments", &attachments) && attachments) {
      for (size_t i = 0; i < attachments->GetSize(); i++) {
        const base::DictionaryValue *d_att = NULL;
        if (attachments->GetDictionary(i, &d_att)) {
          Notes_attachment item;
          if (!item.ReadJSON(*d_att))
            return false;
          attachments_.push_back(item);
        }
      }
    }
  } else {
    type_ = (temp == "trash") ? TRASH : FOLDER;

    base::ListValue *children = NULL;

    if (!input.GetList("children", &children))
      return false;

    for (size_t i = 0; i < children->GetSize(); i++) {
      base::DictionaryValue *item = NULL;

      if (!children->GetDictionary(i, &item))
        return false;

      Notes_Node *child = new Notes_Node(id_);

      child->ReadJSON(*item);

      Add(child, child_count());
    }
  }
  return true;
}

}  // namespace vivaldi
