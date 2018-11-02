// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved

#ifndef NOTES_NOTE_ATTACHMENT_H_
#define NOTES_NOTE_ATTACHMENT_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace base {
class Value;
class DictionaryValue;
}  // namespace base

namespace vivaldi {

class NotesCodec;

class NoteAttachment {
 public:
  explicit NoteAttachment(const std::string& content);
  NoteAttachment(const std::string& checksum, const std::string& content);
  ~NoteAttachment();

  NoteAttachment(NoteAttachment&&);
  NoteAttachment& operator=(NoteAttachment&&);

  std::unique_ptr<base::Value> Encode(NotesCodec* checksummer) const;
  static std::unique_ptr<NoteAttachment> Decode(const base::DictionaryValue*,
                                                NotesCodec* checksummer);

  const std::string& content() const { return content_; }
  std::string checksum() const { return checksum_; }

  bool empty() const { return content_.empty() && checksum_.empty(); }
  bool unsynced() const { return content_.empty() && !checksum_.empty(); }

 private:
  std::string checksum_;
  std::string content_;

  DISALLOW_COPY_AND_ASSIGN(NoteAttachment);
};

typedef std::unordered_map<std::string, NoteAttachment> NoteAttachments;

}  // namespace vivaldi

#endif  // NOTES_NOTE_ATTACHMENT_H_
