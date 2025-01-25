// Copyright (c) 2013-2015 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_NOTES_NOTE_NODE_H_
#define COMPONENTS_NOTES_NOTE_NODE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/notes/deprecated_note_attachment.h"
#include "ui/base/models/tree_node_model.h"
#include "url/gurl.h"

class Profile;

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace vivaldi {
class NotesStorage;
class NoteLoadDetails;

class NoteNode : public ui::TreeNode<NoteNode> {
 public:
  enum Type { NOTE, FOLDER, MAIN, OTHER, TRASH, SEPARATOR, ATTACHMENT };

  static const char kRootNodeUuid[];
  static const char kMainNodeUuid[];
  static const char kOtherNotesNodeUuid[];
  static const char kTrashNodeUuid[];

  // A bug in sync caused some problematic UUIDs to be produced.
  static const char kBannedUuidDueToPastSyncBug[];

  NoteNode(int64_t id, const base::Uuid& uuid, Type type);
  ~NoteNode() override;

  // Returns true if the node is a NotePermanentNode (which does not include
  // the root).
  bool is_permanent_node() const { return is_permanent_node_; }

  Type type() const { return type_; }
  bool is_folder() const { return type_ == FOLDER || is_permanent_node_; }
  bool is_note() const { return type_ == NOTE; }
  bool is_main() const { return type_ == MAIN; }
  bool is_other() const { return type_ == OTHER; }
  bool is_trash() const { return type_ == TRASH; }
  bool is_separator() const { return type_ == SEPARATOR; }
  bool is_attachment() const { return type_ == ATTACHMENT; }

  // Returns an unique id for this node.
  // For notes nodes that are managed by the notes model, the IDs are
  // persisted across sessions.
  int64_t id() const { return id_; }
  void set_id(int64_t id) { id_ = id; }

  // Returns this node's UUID, which is guaranteed to be valid.
  // For note nodes that are managed by the notes model, the UUIDs are
  // persisted across sessions and stable throughout the lifetime of the
  // note.
  const base::Uuid& uuid() const { return uuid_; }

  // Get the creation time for the node.
  base::Time GetCreationTime() const { return creation_time_; }

  // Get the last modification time for the node.
  base::Time GetLastModificationTime() const { return last_modification_time_; }

  const std::u16string& GetContent() const { return content_; }

  const GURL& GetURL() const { return url_; }

  void SetContent(const std::u16string& content) { content_ = content; }
  void SetURL(const GURL& url) { url_ = url; }
  void SetCreationTime(const base::Time creation_time) {
    creation_time_ = creation_time;
  }
  void SetLastModificationTime(const base::Time last_modified_time) {
    last_modification_time_ = last_modified_time;
  }

  void AddAttachmentDeprecated(DeprecatedNoteAttachment&& attachment) {
    deprecated_attachments_.insert(
        std::make_pair(attachment.checksum(),
                       std::forward<DeprecatedNoteAttachment>(attachment)));
  }

 protected:
  NoteNode(int64_t id,
           const base::Uuid& uuid,
           Type type,
           bool is_permanent_node);
  NoteNode(const NoteNode&) = delete;
  NoteNode& operator=(const NoteNode&) = delete;

 private:
  friend class NotesModel;

  // Type of node. See enum above.
  const Type type_;
  // Time of creation.
  base::Time creation_time_;
  // Time of last modification.
  base::Time last_modification_time_;
  // Actual note text.
  std::u16string content_;
  // Attached URL.
  GURL url_;
  // List of attached data. Deprecated. Only used for migration.
  DeprecatedNoteAttachments deprecated_attachments_;

  // The UUID for this node. A NoteNode UUID is immutable and differs from
  // the |id_| in that it is consistent across different clients and
  // stable throughout the lifetime of the bookmark.
  const base::Uuid uuid_;

  // The unique identifier for this node.
  int64_t id_;

  const bool is_permanent_node_;
};

// Node used for the permanent folders (excluding the root).
class PermanentNoteNode : public NoteNode {
 public:
  ~PermanentNoteNode() override;

 private:
  friend class NoteLoadDetails;

  // Permanent nodes are well-known, it's not allowed to create arbitrary ones.
  static std::unique_ptr<PermanentNoteNode> CreateMainNotes(int64_t id);
  static std::unique_ptr<PermanentNoteNode> CreateOtherNotes(int64_t id);
  static std::unique_ptr<PermanentNoteNode> CreateNoteTrash(int64_t id);

  // Constructor is private to disallow the construction of permanent nodes
  // other than the well-known ones, see factory methods.
  PermanentNoteNode(int64_t id,
                    Type type,
                    const base::Uuid& uuid,
                    const std::u16string& title);
  PermanentNoteNode(const PermanentNoteNode&) = delete;
  PermanentNoteNode& operator=(const PermanentNoteNode&) = delete;
};

}  // namespace vivaldi

#endif  // COMPONENTS_NOTES_NOTE_NODE_H_
