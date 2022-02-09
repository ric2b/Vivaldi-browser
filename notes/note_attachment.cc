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
  checksum_ += "|" + base::NumberToString(content.size());
}

NoteAttachment::NoteAttachment(const std::string& checksum,
                               const std::string& content)
    : checksum_(checksum), content_(content) {}

NoteAttachment::NoteAttachment(NoteAttachment&&) = default;
NoteAttachment& NoteAttachment::operator=(NoteAttachment&&) = default;

base::Value NoteAttachment::Encode(NotesCodec* checksummer) const {
  DCHECK(checksummer);

  base::Value attachment_value(base::Value::Type::DICTIONARY);
  attachment_value.SetStringKey("checksum", checksum_);
  checksummer->UpdateChecksum(checksum_);
  attachment_value.SetStringKey("content", content_);
  checksummer->UpdateChecksum(content_);

  return attachment_value;
}

std::unique_ptr<NoteAttachment> NoteAttachment::Decode(
    const base::Value& input,
    NotesCodec* checksummer) {
  DCHECK(input.is_dict());
  DCHECK(checksummer);

  const std::string* checksum = input.FindStringKey("checksum");
  const std::string* content = input.FindStringKey("content");

  if (!content)
    return nullptr;

  checksummer->UpdateChecksum(*content);

  std::unique_ptr<NoteAttachment> attachment;
  if (checksum) {
    checksummer->UpdateChecksum(*checksum);
    attachment = std::make_unique<NoteAttachment>(*checksum, *content);
  } else {
    attachment = std::make_unique<NoteAttachment>(*content);
  }

  return attachment;
}

}  // namespace vivaldi
