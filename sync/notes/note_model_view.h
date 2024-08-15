// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTES_NOTE_MODEL_VIEW_H_
#define SYNC_NOTES_NOTE_MODEL_VIEW_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/uuid.h"
#include "components/notes/note_node.h"

namespace vivaldi {
class NotesModel;
class NotesModelObserver;
}  // namespace vivaldi

namespace sync_notes {

// A sync-specific abstraction mimic-ing the API in NoteModel that allows
// exposing the minimal API surface required for sync and customizing how local
// permanent folders map to server-side permanent folders.
class NoteModelView {
 public:
  // `note_model` must not be null and must outlive any usage of this
  // object.
  explicit NoteModelView(vivaldi::NotesModel* note_model);
  NoteModelView(const NoteModelView&) = delete;
  virtual ~NoteModelView();

  NoteModelView& operator=(const NoteModelView&) = delete;

  // Returns whether `node` is actually relevant in the context of this view,
  // which allows filtering which subset of notes should be sync-ed. Note
  // that some other APIs, such as traversing root(), can expose nodes that are
  // NOT meant to be sync-ed, hence the need for this predicate.
  bool IsNodeSyncable(const vivaldi::NoteNode* node) const;

  // Functions that allow influencing which note tree is exposed to sync.
  virtual const vivaldi::NoteNode* main_node() const = 0;
  virtual const vivaldi::NoteNode* other_node() const = 0;
  virtual const vivaldi::NoteNode* trash_node() const = 0;

  // Ensures that main_node(), other_node() and trash_node() return
  // non-null. This is always the case for local-or-syncable permanent folders,
  // and the function a no-op, but for account permanent folders it is necessary
  // to create them explicitly.
  virtual void EnsurePermanentNodesExist() = 0;

  // Deletes all nodes that would return true for IsNodeSyncable(). Permanent
  // folders may or may not be deleted depending on precise mapping (only
  // account permanent folders can be deleted).
  virtual void RemoveAllSyncableNodes() = 0;

  // See vivaldi::NotesModel for documentation, as all functions below
  // mimic the same API.
  bool loaded() const;
  const vivaldi::NoteNode* root_node() const;
  bool is_permanent_node(const vivaldi::NoteNode* node) const;
  void AddObserver(vivaldi::NotesModelObserver* observer);
  void RemoveObserver(vivaldi::NotesModelObserver* observer);
  void BeginExtensiveChanges();
  void EndExtensiveChanges();
  void Remove(const vivaldi::NoteNode* node, const base::Location& location);
  void Move(const vivaldi::NoteNode* node,
            const vivaldi::NoteNode* new_parent,
            size_t index);
  void SetTitle(const vivaldi::NoteNode* node, const std::u16string& title);
  void SetContent(const vivaldi::NoteNode* node, const std::u16string& content);
  void SetLastModificationTime(const vivaldi::NoteNode* node,
                               const base::Time time);
  void SetURL(const vivaldi::NoteNode* node, const GURL& url);
  const vivaldi::NoteNode* AddFolder(
      const vivaldi::NoteNode* parent,
      size_t index,
      const std::u16string& name,
      std::optional<base::Time> creation_time,
      std::optional<base::Time> last_modified_time,
      std::optional<base::Uuid> uuid);
  const vivaldi::NoteNode* AddNote(const vivaldi::NoteNode* parent,
                                   size_t index,
                                   const std::u16string& title,
                                   const GURL& url,
                                   const std::u16string& content,
                                   std::optional<base::Time> creation_time,
                                   std::optional<base::Time> last_modified_time,
                                   std::optional<base::Uuid> uuid);
  const vivaldi::NoteNode* AddSeparator(const vivaldi::NoteNode* parent,
                                        size_t index,
                                        const std::u16string& name,
                                        std::optional<base::Time> creation_time,
                                        std::optional<base::Uuid> uuid);
  const vivaldi::NoteNode* AddAttachmentFromChecksum(
      const vivaldi::NoteNode* parent,
      size_t index,
      const std::u16string& title,
      const GURL& url,
      const std::string& checksum,
      std::optional<base::Time> creation_time,
      std::optional<base::Uuid> uuid);
  void ReorderChildren(
      const vivaldi::NoteNode* parent,
      const std::vector<const vivaldi::NoteNode*>& ordered_nodes);

 protected:
  vivaldi::NotesModel* underlying_model() { return note_model_.get(); }
  const vivaldi::NotesModel* underlying_model() const {
    return note_model_.get();
  }

 private:
  // Using WeakPtr here allows detecting violations of the constructor
  // precondition and CHECK fail if NotesModel is destroyed earlier.
  // This also simplifies unit-testing, because using raw_ptr otherwise
  // complicates the way to achieve a reasonable destruction order for
  // TestNoteModelView.
  const base::WeakPtr<vivaldi::NotesModel> note_model_;
};

class NoteModelViewUsingLocalOrSyncableNodes : public NoteModelView {
 public:
  // `note_model` must not be null and must outlive any usage of this
  // object.
  explicit NoteModelViewUsingLocalOrSyncableNodes(
      vivaldi::NotesModel* note_model);
  ~NoteModelViewUsingLocalOrSyncableNodes() override;

  // NoteModelView overrides.
  const vivaldi::NoteNode* main_node() const override;
  const vivaldi::NoteNode* other_node() const override;
  const vivaldi::NoteNode* trash_node() const override;
  void EnsurePermanentNodesExist() override;
  void RemoveAllSyncableNodes() override;
};

}  // namespace sync_notes

#endif  // SYNC_NOTES_NOTE_MODEL_VIEW_H_
