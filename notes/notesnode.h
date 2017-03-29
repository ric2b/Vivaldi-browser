// Copyright (c) 2013-2014 Vivaldi Technologies AS. All rights reserved

#ifndef VIVALDI_NOTESNODE_ENTRY_H_
#define VIVALDI_NOTESNODE_ENTRY_H_

#include <string>
#include <vector>
#include "notes_attachment.h"
#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "content/public/browser/notification_observer.h"
#include "ui/base/models/tree_node_model.h"

class Profile;

namespace base {
  class Value;
  class DictionaryValue;
  class SequencedTaskRunner;
}

namespace Vivaldi {

class NotesStorage;
class NotesLoadDetails;

class Notes_Node : public ui::TreeNode < Notes_Node >
{
public:
  enum Type {
    NOTE,
    FOLDER
  };

  Notes_Node(int64 id);
  ~Notes_Node() override;

  base::Value *WriteJSON() const;
  bool ReadJSON(base::DictionaryValue &input);

  bool SaveNote();
  bool LoadNote();

  void SetType(Type type) { type_ = type; }
  bool is_folder() const { return type_ == FOLDER; }

  // Get the unique id for this Node.
  int64 id() const { return id_; }
  void set_id(int64 id) { id_ = id; }

  // Get the creation time for the node.
  base::Time GetCreationTime() { return creation_time_; }
  base::string16 GetFilename() { return filename_; }

  const base::string16 &GetSubject() const { return subject_; }
  const base::string16 &GetContent() const { return content_; }

  const GURL &GetURL() const { return url_; }

  const Notes_attachment &GetIcon() const { return note_icon_; }

  const Notes_attachment &GetAttachment(int index) const { return attachments_[index]; }
  const std::vector<Notes_attachment> GetAttachments() { return attachments_; }
  void SetContent(const base::string16 &content) { content_ = content; }
  void SetURL(const GURL &url) { url_ = url; }

  void SetIcon(const Notes_attachment &icon) { note_icon_ = icon; }
  void AddAttachment(const Notes_attachment &attachment) { attachments_.push_back(attachment); }
  void DeleteAttachment(int index) { attachments_.erase(attachments_.begin() + index); }

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
  int64 id_;

  DISALLOW_COPY_AND_ASSIGN(Notes_Node);
};


} // namespace Vivaldi
#endif //VIVALDI_NOTESNODE_ENTRY_H_
