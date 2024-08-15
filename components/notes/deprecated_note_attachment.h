// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_NOTES_DEPRECATED_NOTE_ATTACHMENT_H_
#define COMPONENTS_NOTES_DEPRECATED_NOTE_ATTACHMENT_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/values.h"

namespace vivaldi {

class NotesCodec;

class DeprecatedNoteAttachment {
 public:
  explicit DeprecatedNoteAttachment(const std::string& content);
  DeprecatedNoteAttachment(const std::string& checksum,
                           const std::string& content);
  ~DeprecatedNoteAttachment();

  DeprecatedNoteAttachment(DeprecatedNoteAttachment&&);
  DeprecatedNoteAttachment& operator=(DeprecatedNoteAttachment&&);

  static std::unique_ptr<DeprecatedNoteAttachment> Decode(
      const base::Value&,
      NotesCodec* checksummer);

  const std::string& content() const { return content_; }
  std::string checksum() const { return checksum_; }

 private:
  std::string checksum_;
  std::string content_;
};

typedef std::map<std::string, DeprecatedNoteAttachment>
    DeprecatedNoteAttachments;

}  // namespace vivaldi

#endif  // COMPONENTS_NOTES_DEPRECATED_NOTE_ATTACHMENT_H_
