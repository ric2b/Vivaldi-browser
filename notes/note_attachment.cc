// Copyright (c) 2013-2015 Vivaldi Technologies AS. All rights reserved

#include <string>
#include "base/base64.h"
#include "base/guid.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "crypto/sha2.h"

#include "notes/note_attachment.h"
#include "notes/notes_codec.h"

namespace vivaldi {

NoteAttachment::~NoteAttachment() = default;

NoteAttachment::NoteAttachment(const std::string& content) : content_(content) {
  if (content.empty())
    return;

  base::Base64Encode(crypto::SHA256HashString(content), &checksum_);
  checksum_  += "|" + base::NumberToString(content.size());
}

NoteAttachment::NoteAttachment(const std::string& checksum,
                               const std::string& content)
    : checksum_(checksum), content_(content) {}

NoteAttachment::NoteAttachment(NoteAttachment&&) = default;
NoteAttachment& NoteAttachment::operator=(NoteAttachment&&) = default;

std::unique_ptr<base::Value> NoteAttachment::Encode(
    NotesCodec* checksummer) const {
  DCHECK(checksummer);

  std::unique_ptr<base::DictionaryValue> attachment_value =
      base::WrapUnique(new base::DictionaryValue());
  attachment_value->SetKey("checksum", base::Value(checksum_));
  checksummer->UpdateChecksum(checksum_);
  attachment_value->SetKey("content", base::Value(content_));
  checksummer->UpdateChecksum(content_);

  return attachment_value;
}

std::unique_ptr<NoteAttachment> NoteAttachment::Decode(
    const base::DictionaryValue* input,
    NotesCodec* checksummer) {
  DCHECK(input);
  DCHECK(checksummer);

  const base::Value* checksum =
      input->FindKeyOfType("checksum", base::Value::Type::STRING);
  const base::Value* content =
      input->FindKeyOfType("content", base::Value::Type::STRING);

  if (!content)
    return nullptr;

  checksummer->UpdateChecksum(content->GetString());

  std::unique_ptr<NoteAttachment> attachment;
  if (checksum) {
    checksummer->UpdateChecksum(checksum->GetString());
    attachment = std::make_unique<NoteAttachment>(checksum->GetString(),
                                                  content->GetString());
  } else {
    attachment = std::make_unique<NoteAttachment>(content->GetString());
  }

  return attachment;
}

}  // namespace vivaldi
