// Copyright (c) 2013-2015 Vivaldi Technologies AS. All rights reserved

#include <string>
#include "base/base64.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "crypto/sha2.h"

#include "components/notes/deprecated_note_attachment.h"
#include "components/notes/notes_codec.h"

namespace vivaldi {

DeprecatedNoteAttachment::~DeprecatedNoteAttachment() = default;

DeprecatedNoteAttachment::DeprecatedNoteAttachment(const std::string& content)
    : content_(content) {
  if (content.empty())
    return;

  checksum_ = base::Base64Encode(crypto::SHA256HashString(content));
  checksum_ += "|" + base::NumberToString(content.size());
}

DeprecatedNoteAttachment::DeprecatedNoteAttachment(const std::string& checksum,
                                                   const std::string& content)
    : checksum_(checksum), content_(content) {}

DeprecatedNoteAttachment::DeprecatedNoteAttachment(DeprecatedNoteAttachment&&) =
    default;
DeprecatedNoteAttachment& DeprecatedNoteAttachment::operator=(
    DeprecatedNoteAttachment&&) = default;

std::unique_ptr<DeprecatedNoteAttachment> DeprecatedNoteAttachment::Decode(
    const base::Value& input,
    NotesCodec* checksummer) {
  DCHECK(input.is_dict());
  DCHECK(checksummer);

  const std::string* checksum = input.GetDict().FindString("checksum");
  const std::string* content = input.GetDict().FindString("content");

  if (!content)
    return nullptr;

  checksummer->UpdateChecksum(*content);

  std::unique_ptr<DeprecatedNoteAttachment> attachment;
  if (checksum) {
    checksummer->UpdateChecksum(*checksum);
    attachment =
        std::make_unique<DeprecatedNoteAttachment>(*checksum, *content);
  } else {
    attachment = std::make_unique<DeprecatedNoteAttachment>(*content);
  }

  return attachment;
}

}  // namespace vivaldi
