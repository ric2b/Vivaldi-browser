// Copyright (c) 2013-2014 Vivaldi Technologies AS. All rights reserved

#include "components/notes/note_node.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/vivaldi_resources.h"
#include "base/base64.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "ui/base/l10n/l10n_util.h"

namespace vivaldi {

namespace {
static const char16_t kNotes[] = u"Notes";
static const char16_t kOtherNotes[] = u"Other Notes";

bool IsPermanentType(NoteNode::Type type) {
  return type == NoteNode::MAIN || type == NoteNode::OTHER ||
         type == NoteNode::TRASH;
}
}  // namespace

// Below predefined UUIDs for permanent note folders, determined via named
// UUIDs. Do NOT modify them as they may be exposed via Sync. For
// reference, here's the python script to produce them:
// > import uuid
// > vivaldi_namespace = uuid.uuid5(uuid.NAMESPACE_DNS, "vivaldi.com")
// > notes_namespace = uuid.uuid5(vivaldi_namespace, "notes")
// > root_guid = uuid.uuid5(notes_namespace, "root")
// > main_guid = uuid.uuid5(notes_namespace, "main")
// > other_guid = uuid.uuid5(notes_namespace, "other")
// > trash_guid = uuid.uuid5(notes_namespace, "trash")

const char NoteNode::kRootNodeUuid[] = "ef3daefb-7b28-5cbc-8397-e3394dbeac45";
const char NoteNode::kMainNodeUuid[] = "0709f24e-6a69-55df-ba1c-eff0c6762616";
const char NoteNode::kOtherNotesNodeUuid[] =
    "7f81b917-0763-5232-a83d-c24704bc9d57";
const char NoteNode::kTrashNodeUuid[] = "572928d8-654d-55c0-8d54-d469f838e392";

// This value is the result of exercising sync's function
// syncer::InferGuidForLegacyNote() with an empty input.
const char NoteNode::kBannedUuidDueToPastSyncBug[] =
    "da39a3ee-5e6b-fb0d-b255-bfef95601890";

NoteNode::NoteNode(int64_t id, const base::Uuid& uuid, Type type)
    : NoteNode(id, uuid, type, false) {
  DCHECK(!IsPermanentType(type));
}

NoteNode::NoteNode(int64_t id,
                   const base::Uuid& uuid,
                   Type type,
                   bool is_permanent_node)
    : type_(type),
      creation_time_(
          base::Time::Now()),  // This will be overwritten if read from file.
      last_modification_time_(
          base::Time::Now()),  // This will be overwritten if read from file.
      uuid_(uuid),
      id_(id),
      is_permanent_node_(is_permanent_node) {
  if (type_ == SEPARATOR) {
    // Make it easier for sync to match separators
    SetTitle(u"--- SEPARATOR " +
             base::NumberToString16(creation_time_.ToInternalValue()));
  }
  DCHECK(uuid.is_valid());
}

NoteNode::~NoteNode() {}

PermanentNoteNode::~PermanentNoteNode() = default;

// static
std::unique_ptr<PermanentNoteNode> PermanentNoteNode::CreateMainNotes(
    int64_t id) {
  return base::WrapUnique(new PermanentNoteNode(
      id, MAIN, base::Uuid::ParseLowercase(kMainNodeUuid), kNotes));
}

// static
std::unique_ptr<PermanentNoteNode> PermanentNoteNode::CreateOtherNotes(
    int64_t id) {
  return base::WrapUnique(new PermanentNoteNode(
      id, OTHER, base::Uuid::ParseLowercase(kOtherNotesNodeUuid), kOtherNotes));
}

// static
std::unique_ptr<PermanentNoteNode> PermanentNoteNode::CreateNoteTrash(
    int64_t id) {
  return base::WrapUnique(new PermanentNoteNode(
      id, TRASH, base::Uuid::ParseLowercase(kTrashNodeUuid),
      l10n_util::GetStringUTF16(IDS_NOTES_TRASH_FOLDER_NAME)));
}

PermanentNoteNode::PermanentNoteNode(int64_t id,
                                     Type type,
                                     const base::Uuid& uuid,
                                     const std::u16string& title)
    : NoteNode(id, uuid, type, true) {
  DCHECK(IsPermanentType(type));
  SetTitle(title);
}

}  // namespace vivaldi
