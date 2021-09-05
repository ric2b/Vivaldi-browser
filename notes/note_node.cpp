// Copyright (c) 2013-2014 Vivaldi Technologies AS. All rights reserved

#include "notes/note_node.h"

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

namespace vivaldi {

namespace {
std::string PermanentNodeTypeToGuid(NoteNode::Type type) {
  switch (type) {
    case NoteNode::MAIN:
      return NoteNode::kMainNodeGuid;
    case NoteNode::OTHER:
      return NoteNode::kOtherNotesNodeGuid;
    case NoteNode::FOLDER:
    case NoteNode::NOTE:
    case NoteNode::SEPARATOR:
      NOTREACHED();
      return std::string();
    case NoteNode::TRASH:
      return NoteNode::kTrashNodeGuid;
  }
  NOTREACHED();
  return std::string();
}

bool IsPermanentType(NoteNode::Type type) {
  return type == NoteNode::MAIN || type == NoteNode::OTHER ||
         type == NoteNode::TRASH;
}
}  // namespace

const int64_t NoteNode::kInvalidSyncTransactionVersion = -1;
const char NoteNode::kRootNodeGuid[] = "00000000-0000-4000-a000-000000000001";
const char NoteNode::kMainNodeGuid[] = "00000000-0000-4000-a000-000000000002";
const char NoteNode::kOtherNotesNodeGuid[] =
    "00000000-0000-4000-a000-000000000003";
const char NoteNode::kTrashNodeGuid[] = "00000000-0000-4000-a000-000000000004";

std::string NoteNode::RootNodeGuid() {
  return NoteNode::kRootNodeGuid;
}

NoteNode::NoteNode(int64_t id, const std::string& guid, Type type)
    : NoteNode(id, guid, type, false) {
  DCHECK(!IsPermanentType(type));
}

NoteNode::NoteNode(int64_t id,
                   const std::string& guid,
                   Type type,
                   bool is_permanent_node)
    : type_(type),
      creation_time_(
          base::Time::Now()),  // This will be overwritten if read from file.
      guid_(guid),
      id_(id),
      sync_transaction_version_(kInvalidSyncTransactionVersion),
      is_permanent_node_(is_permanent_node) {
  if (type_ == SEPARATOR) {
    // Make it easier for sync to match separators
    SetTitle(base::UTF8ToUTF16("--- SEPARATOR ") +
             base::NumberToString16(creation_time_.ToInternalValue()));
  }
}

NoteNode::~NoteNode() {}

PermanentNoteNode::PermanentNoteNode(int64_t id, Type type)
    : NoteNode(id, PermanentNodeTypeToGuid(type), type, true) {
  DCHECK(IsPermanentType(type));
}

PermanentNoteNode::~PermanentNoteNode() = default;

}  // namespace vivaldi
