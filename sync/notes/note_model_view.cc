// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notes/note_model_view.h"

#include "base/check.h"
#include "components/notes/note_node.h"
#include "components/notes/notes_model.h"

namespace sync_notes {

namespace {

const vivaldi::NoteNode* GetAncestorPermanentFolder(
    const vivaldi::NoteNode* node) {
  CHECK(node);

  const vivaldi::NoteNode* self_or_ancestor = node;

  while (!self_or_ancestor->is_permanent_node()) {
    self_or_ancestor = self_or_ancestor->parent();
    CHECK(self_or_ancestor);
  }

  return self_or_ancestor;
}

}  // namespace

NoteModelView::NoteModelView(vivaldi::NotesModel* note_model)
    : note_model_(note_model->AsWeakPtr()) {
  CHECK(note_model_);
}

NoteModelView::~NoteModelView() = default;

bool NoteModelView::IsNodeSyncable(const vivaldi::NoteNode* node) const {
  const vivaldi::NoteNode* ancestor_permanent_folder =
      GetAncestorPermanentFolder(node);
  CHECK(ancestor_permanent_folder);
  CHECK(ancestor_permanent_folder->is_permanent_node());
  CHECK_NE(ancestor_permanent_folder, root_node());

  // A node is considered syncable if it is a descendant of one of the syncable
  // permanent folder (e.g. excludes enterprise-managed nodes).
  return ancestor_permanent_folder == main_node() ||
         ancestor_permanent_folder == other_node() ||
         ancestor_permanent_folder == trash_node();
}

bool NoteModelView::loaded() const {
  return note_model_->loaded();
}

const vivaldi::NoteNode* NoteModelView::root_node() const {
  return note_model_->root_node();
}

bool NoteModelView::is_permanent_node(const vivaldi::NoteNode* node) const {
  return note_model_->is_permanent_node(node);
}

void NoteModelView::AddObserver(vivaldi::NotesModelObserver* observer) {
  note_model_->AddObserver(observer);
}

void NoteModelView::RemoveObserver(vivaldi::NotesModelObserver* observer) {
  note_model_->RemoveObserver(observer);
}

void NoteModelView::BeginExtensiveChanges() {
  note_model_->BeginExtensiveChanges();
}

void NoteModelView::EndExtensiveChanges() {
  note_model_->EndExtensiveChanges();
}

void NoteModelView::Remove(const vivaldi::NoteNode* node,
                           const base::Location& location) {
  note_model_->Remove(node, location);
}

void NoteModelView::Move(const vivaldi::NoteNode* node,
                         const vivaldi::NoteNode* new_parent,
                         size_t index) {
  note_model_->Move(node, new_parent, index);
}

void NoteModelView::SetTitle(const vivaldi::NoteNode* node,
                             const std::u16string& title) {
  note_model_->SetTitle(node, title, false);
}

void NoteModelView::SetContent(const vivaldi::NoteNode* node,
                               const std::u16string& content) {
  note_model_->SetContent(node, content, false);
}

void NoteModelView::SetURL(const vivaldi::NoteNode* node, const GURL& url) {
  note_model_->SetURL(node, url, false);
}

void NoteModelView::SetLastModificationTime(const vivaldi::NoteNode* node,
                                            const base::Time time) {
  note_model_->SetLastModificationTime(node, time);
}

const vivaldi::NoteNode* NoteModelView::AddFolder(
    const vivaldi::NoteNode* parent,
    size_t index,
    const std::u16string& name,
    std::optional<base::Time> creation_time,
    std::optional<base::Time> last_modified_time,
    std::optional<base::Uuid> uuid) {
  return note_model_->AddFolder(parent, index, name, creation_time,
                                last_modified_time, uuid);
}

const vivaldi::NoteNode* NoteModelView::AddNote(
    const vivaldi::NoteNode* parent,
    size_t index,
    const std::u16string& title,
    const GURL& url,
    const std::u16string& content,
    std::optional<base::Time> creation_time,
    std::optional<base::Time> last_modified_time,
    std::optional<base::Uuid> uuid) {
  return note_model_->AddNote(parent, index, title, url, content, creation_time,
                              last_modified_time, uuid);
}

const vivaldi::NoteNode* NoteModelView::AddSeparator(
    const vivaldi::NoteNode* parent,
    size_t index,
    const std::u16string& name,
    std::optional<base::Time> creation_time,
    std::optional<base::Uuid> uuid) {
  return note_model_->AddSeparator(parent, index, name, creation_time, uuid);
}

const vivaldi::NoteNode* NoteModelView::AddAttachmentFromChecksum(
    const vivaldi::NoteNode* parent,
    size_t index,
    const std::u16string& title,
    const GURL& url,
    const std::string& checksum,
    std::optional<base::Time> creation_time,
    std::optional<base::Uuid> uuid) {
  return note_model_->AddAttachmentFromChecksum(parent, index, title, url,
                                                checksum, creation_time, uuid);
}

void NoteModelView::ReorderChildren(
    const vivaldi::NoteNode* parent,
    const std::vector<const vivaldi::NoteNode*>& ordered_nodes) {
  note_model_->ReorderChildren(parent, ordered_nodes);
}

NoteModelViewUsingLocalOrSyncableNodes::NoteModelViewUsingLocalOrSyncableNodes(
    vivaldi::NotesModel* note_model)
    : NoteModelView(note_model) {}

NoteModelViewUsingLocalOrSyncableNodes::
    ~NoteModelViewUsingLocalOrSyncableNodes() = default;

const vivaldi::NoteNode* NoteModelViewUsingLocalOrSyncableNodes::main_node()
    const {
  return underlying_model()->main_node();
}

const vivaldi::NoteNode* NoteModelViewUsingLocalOrSyncableNodes::other_node()
    const {
  return underlying_model()->other_node();
}

const vivaldi::NoteNode* NoteModelViewUsingLocalOrSyncableNodes::trash_node()
    const {
  return underlying_model()->trash_node();
}

void NoteModelViewUsingLocalOrSyncableNodes::EnsurePermanentNodesExist() {
  // Local-or-syncable permanent folders always exist, nothing to be done.
  CHECK(main_node());
  CHECK(other_node());
  CHECK(trash_node());
}

void NoteModelViewUsingLocalOrSyncableNodes::RemoveAllSyncableNodes() {
  underlying_model()->BeginExtensiveChanges();

  for (const auto& permanent_node : root_node()->children()) {
    if (!IsNodeSyncable(permanent_node.get())) {
      continue;
    }

    for (int i = static_cast<int>(permanent_node->children().size() - 1);
         i >= 0; --i) {
      underlying_model()->Remove(permanent_node->children()[i].get(),
                                 FROM_HERE);
    }
  }

  underlying_model()->EndExtensiveChanges();
}

}  // namespace sync_notes
