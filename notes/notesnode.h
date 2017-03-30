// Copyright (c) 2013-2015 Vivaldi Technologies AS. All rights reserved

#ifndef NOTES_NOTESNODE_H_
#define NOTES_NOTESNODE_H_

#include <string>
#include <vector>
#include "base/strings/string16.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "content/public/browser/notification_observer.h"
#include "ui/base/models/tree_node_model.h"

#include "notes/notes_attachment.h"

class Profile;

namespace base {
class Value;
class DictionaryValue;
class SequencedTaskRunner;
}

namespace vivaldi {
class NotesStorage;
class NotesLoadDetails;

class Notes_Node : public ui::TreeNode<Notes_Node> {
 public:
  enum Type { NOTE, FOLDER, TRASH };

  explicit Notes_Node(int64_t id);
  ~Notes_Node() override;

  base::Value *WriteJSON() const;
  bool ReadJSON(base::DictionaryValue &input);

  void SetType(Type type) { type_ = type; }
  bool is_folder() const { return type_ == FOLDER || type_ == TRASH; }
  bool is_trash() const { return type_ == TRASH; }

  // Get the unique id for this Node.
  int64_t id() const { return id_; }
  void set_id(int64_t id) { id_ = id; }

  // Get the creation time for the node.
  base::Time GetCreationTime() { return creation_time_; }
  base::string16 GetFilename() { return filename_; }

  const base::string16 &GetSubject() const { return subject_; }
  const base::string16 &GetContent() const { return content_; }

  const GURL &GetURL() const { return url_; }

  const Notes_attachment &GetIcon() const { return note_icon_; }

  const Notes_attachment &GetAttachment(int index) const {
    return attachments_[index];
  }
  const std::vector<Notes_attachment> GetAttachments() { return attachments_; }
  void SetContent(const base::string16 &content) { content_ = content; }
  void SetURL(const GURL &url) { url_ = url; }
  void SetCreationTime(const base::Time creation_time) {
    creation_time_ = creation_time;
  }

  void SetIcon(const Notes_attachment &icon) { note_icon_ = icon; }
  void AddAttachment(const Notes_attachment &attachment) {
    attachments_.push_back(attachment);
  }
  void DeleteAttachment(int index) {
    attachments_.erase(attachments_.begin() + index);
  }

 private:
  // Type of node, folder or note.
  Type type_;
  // Filename of attached content.
  base::string16 filename_;
  // Time of creation.
  base::Time creation_time_;
  // Time of modification.
  base::Time modified_time_;
  // Note subject or title.
  base::string16 subject_;
  // Actual note text.
  base::string16 content_;
  // Attached URL.
  GURL url_;
  // Icon data.
  Notes_attachment note_icon_;
  // List of attached data.
  std::vector<Notes_attachment> attachments_;

  // The unique identifier for this node.
  int64_t id_;

  DISALLOW_COPY_AND_ASSIGN(Notes_Node);
};


}  // namespace vivaldi

#endif  // NOTES_NOTESNODE_H_
