// Copyright (c) 2013-2014 Vivaldi Technologies AS. All rights reserved

#include "notes/note_node.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/vivaldi_resources.h"
#include "base/base64.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "ui/base/l10n/l10n_util.h"

namespace vivaldi {

namespace {
static const char kNotes[] = "Notes";
static const char kOtherNotes[] = "Other Notes";

bool IsPermanentType(NoteNode::Type type) {
  return type == NoteNode::MAIN || type == NoteNode::OTHER ||
         type == NoteNode::TRASH;
}
}  // namespace

const char NoteNode::kRootNodeGuid[] = "00000000-0000-4000-a000-000000000001";
const char NoteNode::kMainNodeGuid[] = "00000000-0000-4000-a000-000000000002";
const char NoteNode::kOtherNotesNodeGuid[] =
    "00000000-0000-4000-a000-000000000003";
const char NoteNode::kTrashNodeGuid[] = "00000000-0000-4000-a000-000000000004";

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
      is_permanent_node_(is_permanent_node) {
  if (type_ == SEPARATOR) {
    // Make it easier for sync to match separators
    SetTitle(base::UTF8ToUTF16("--- SEPARATOR ") +
             base::NumberToString16(creation_time_.ToInternalValue()));
  }
}

NoteNode::~NoteNode() {}

PermanentNoteNode::~PermanentNoteNode() = default;

// static
std::unique_ptr<PermanentNoteNode> PermanentNoteNode::CreateMainNotes(
    int64_t id) {
  return base::WrapUnique(new PermanentNoteNode(id, MAIN, kMainNodeGuid,
                                                base::UTF8ToUTF16(kNotes)));
}

// static
std::unique_ptr<PermanentNoteNode> PermanentNoteNode::CreateOtherNotes(
    int64_t id) {
  return base::WrapUnique(new PermanentNoteNode(
      id, OTHER, kOtherNotesNodeGuid, base::UTF8ToUTF16(kOtherNotes)));
}

// static
std::unique_ptr<PermanentNoteNode> PermanentNoteNode::CreateNoteTrash(
    int64_t id) {
  return base::WrapUnique(new PermanentNoteNode(
      id, TRASH, kTrashNodeGuid,
      l10n_util::GetStringUTF16(IDS_NOTES_TRASH_FOLDER_NAME)));
}

PermanentNoteNode::PermanentNoteNode(int64_t id,
                                     Type type,
                                     const std::string& guid,
                                     const base::string16& title)
    : NoteNode(id, guid, type, true) {
  DCHECK(IsPermanentType(type));
  SetTitle(title);
}

}  // namespace vivaldi
