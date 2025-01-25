// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notes/notes_model.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/i18n/string_compare.h"
#include "base/i18n/string_search.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/uuid.h"
#include "components/notes/note_load_details.h"
#include "components/notes/note_node.h"
#include "components/notes/notes_storage.h"
#include "importer/imported_notes_entry.h"
#include "sync/file_sync/file_store.h"
#include "sync/notes/note_model_view.h"
#include "sync/notes/note_sync_service.h"
#include "ui/base/models/tree_node_iterator.h"

using base::Time;

namespace vivaldi {
namespace {
// Comparator used when sorting notes. Folders are sorted first, then
// notes.
class SortComparator {
 public:
  explicit SortComparator(icu::Collator* collator) : collator_(collator) {}

  // Returns true if |n1| preceeds |n2|.
  bool operator()(const std::unique_ptr<NoteNode>& n1,
                  const std::unique_ptr<NoteNode>& n2) {
    if (n1->type() == n2->type()) {
      // Types are the same, compare the names.
      if (!collator_)
        return n1->GetTitle() < n2->GetTitle();
      return base::i18n::CompareString16WithCollator(
                 *collator_, n1->GetTitle(), n2->GetTitle()) == UCOL_LESS;
    }
    // Types differ, sort such that folders come first.
    return n1->is_folder();
  }

 private:
  const raw_ptr<icu::Collator> collator_;
};

const NoteNode* GetNodeByID(const NoteNode* node, int64_t id) {
  if (node->id() == id)
    return node;

  for (auto& it : node->children()) {
    const NoteNode* result = GetNodeByID(it.get(), id);
    if (result)
      return result;
  }
  return nullptr;
}
}  // namespace

// Helper to get a mutable notes node.
NoteNode* AsMutable(const NoteNode* node) {
  return const_cast<NoteNode*>(node);
}

NotesModel::NotesModel(sync_notes::NoteSyncService* sync_service,
                       file_sync::SyncedFileStore* synced_file_store)
    : root_(std::make_unique<NoteNode>(
          0,
          base::Uuid::ParseLowercase(NoteNode::kRootNodeUuid),
          NoteNode::FOLDER)),
      sync_service_(sync_service),
      synced_file_store_(synced_file_store) {}

NotesModel::~NotesModel() {
  for (auto& observer : observers_)
    observer.NotesModelBeingDeleted();

  if (store_.get()) {
    // The store maintains a reference back to us. We need to tell it we're gone
    // so that it doesn't try and invoke a method back on us again.
    store_->NotesModelDeleted();
  }
}

void NotesModel::Load(const base::FilePath& profile_path) {
  // If the store is non-null, it means Load was already invoked. Load should
  // only be invoked once.
  DCHECK(!store_);

  // Load the notes. NotesStorage notifies us when done.
  store_ = std::make_unique<NotesStorage>(this, profile_path);

  // Creating ModelLoader schedules the load on a backend task runner.
  NoteModelLoader::Create(
      profile_path, std::make_unique<NoteLoadDetails>(),
      base::BindOnce(&NotesModel::DoneLoading, AsWeakPtr()));
}

void NotesModel::DoneLoading(std::unique_ptr<NoteLoadDetails> details) {
  DCHECK(details);
  DCHECK(!loaded_);

  next_node_id_ = details->max_id();
  if (details->computed_checksum() != details->stored_checksum() ||
      details->ids_reassigned() || details->uuids_reassigned()) {
    // If notes file changed externally, the IDs may have changed
    // externally. In that case, the decoder may have reassigned IDs to make
    // them unique. So when the file has changed externally, we should save the
    // notes file to persist new IDs.
    if (store_.get())
      store_->ScheduleSave();
  }
  root_ = details->release_root();
  main_node_ = details->main_notes_node();
  other_node_ = details->other_notes_node();
  trash_node_ = details->trash_notes_node();

  if (!synced_file_store_ || synced_file_store_->IsLoaded())
    OnSyncedFilesStoreLoaded(std::move(details));
  else
    synced_file_store_->AddOnLoadedCallback(
        base::BindOnce(&NotesModel::OnSyncedFilesStoreLoaded,
                       weak_factory_.GetWeakPtr(), std::move(details)));
}

void NotesModel::OnSyncedFilesStoreLoaded(
    std::unique_ptr<NoteLoadDetails> details) {
  loaded_ = true;

  if (sync_service_) {
    sync_service_->DecodeNoteSyncMetadata(
        details->sync_metadata_str(),
        store_ ? base::BindRepeating(&NotesStorage::ScheduleSave,
                                     base::Unretained(store_.get()))
               : base::DoNothing(),
        std::make_unique<sync_notes::NoteModelViewUsingLocalOrSyncableNodes>(
            this));
  }

  // Notify our direct observers.
  for (auto& observer : observers_)
    observer.NotesModelLoaded(details->ids_reassigned());

  if (details->has_deprecated_attachments()) {
    BeginExtensiveChanges();
    MigrateAttachmentsRecursive(AsMutable(root_node()));
    EndExtensiveChanges();
  }
}

void NotesModel::MigrateAttachmentsRecursive(NoteNode* node) {
  for (auto& deprecated_attachment : node->deprecated_attachments_) {
    if (deprecated_attachment.second.content().empty())
      continue;

    std::string_view content(deprecated_attachment.second.content());
    size_t separator_pos = content.find_first_of(',');
    if (separator_pos == std::string::npos)
      continue;
    content.remove_prefix(separator_pos + 1);
    std::optional<std::vector<uint8_t>> decoded = base::Base64Decode(content);
    AddAttachment(node, node->children().size(), u"migrated", GURL(), *decoded);
  }

  for (size_t i = node->children().size(); i > 0; --i)
    MigrateAttachmentsRecursive(node->children()[i - 1].get());
}

void NotesModel::GetNotes(std::vector<NotesModel::URLAndTitle>* notes) {
  base::AutoLock url_lock(url_lock_);
  const GURL* last_url = NULL;
  for (auto* i : nodes_ordered_by_url_set_) {
    const GURL* url = &(i->GetURL());
    // Only add unique URLs.
    if (!last_url || *url != *last_url) {
      NotesModel::URLAndTitle note;
      note.url = *url;
      note.title = i->GetTitle();
      note.content = i->GetContent();
      notes->push_back(note);
    }
    last_url = url;
  }
}

void NotesModel::AddObserver(NotesModelObserver* observer) {
  observers_.AddObserver(observer);
}

void NotesModel::RemoveObserver(NotesModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void NotesModel::BeginExtensiveChanges() {
  if (++extensive_changes_ == 1) {
    for (auto& observer : observers_)
      observer.ExtensiveNotesChangesBeginning();
  }
}

void NotesModel::EndExtensiveChanges() {
  --extensive_changes_;
  DCHECK_GE(extensive_changes_, 0);
  if (extensive_changes_ == 0) {
    for (auto& observer : observers_)
      observer.ExtensiveNotesChangesEnded();
  }
}

NoteNode* NotesModel::AddNode(NoteNode* parent,
                              int index,
                              std::unique_ptr<NoteNode> node) {
  NoteNode* node_ptr = node.get();
  DCHECK(parent);

  parent->Add(std::move(node), index);

  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesNodeAdded(parent, index);

  return node_ptr;
}

void NotesModel::UpdateLastModificationTime(const NoteNode* node) {
  DCHECK(node);

  const base::Time time = base::Time::Now();
  if (node->GetLastModificationTime() == time || is_permanent_node(node))
    return;

  AsMutable(node)->SetLastModificationTime(time);
  if (store_.get())
    store_->ScheduleSave();
}

const NoteNode* NotesModel::AddNote(
    const NoteNode* parent,
    size_t index,
    const std::u16string& title,
    const GURL& url,
    const std::u16string& content,
    std::optional<base::Time> creation_time,
    std::optional<base::Time> last_modification_time,
    std::optional<base::Uuid> uuid) {
  DCHECK(loaded_);
  DCHECK(!uuid || uuid->is_valid());
  DCHECK(parent);

  if (!creation_time)
    creation_time = Time::Now();
  if (!last_modification_time)
    last_modification_time = creation_time;

  std::unique_ptr<NoteNode> new_node = std::make_unique<NoteNode>(
      generate_next_node_id(), uuid ? *uuid : base::Uuid::GenerateRandomV4(),
      NoteNode::NOTE);
  new_node->SetTitle(title);
  new_node->SetCreationTime(*creation_time);
  new_node->SetLastModificationTime(*last_modification_time);
  new_node->SetContent(content);
  new_node->SetURL(url);

  {
    // Only hold the lock for the duration of the insert.
    base::AutoLock url_lock(url_lock_);
    nodes_ordered_by_url_set_.insert(new_node.get());
  }

  return AddNode(AsMutable(parent), index, std::move(new_node));
}

const NoteNode* NotesModel::ImportNote(const NoteNode* parent,
                                       size_t index,
                                       const ImportedNotesEntry& note) {
  if (!loaded_)
    return NULL;

  int64_t id = generate_next_node_id();

  std::unique_ptr<NoteNode> new_node = std::make_unique<NoteNode>(
      id, base::Uuid::GenerateRandomV4(),
      note.is_folder ? NoteNode::FOLDER : NoteNode::NOTE);
  new_node->SetTitle(note.title);
  new_node->SetCreationTime(note.creation_time);
  new_node->SetLastModificationTime(note.last_modification_time);

  if (!note.is_folder) {
    new_node->SetURL(note.url);
    new_node->SetContent(note.content);
  }
  return AddNode(AsMutable(parent), index, std::move(new_node));
}

const NoteNode* NotesModel::AddFolder(
    const NoteNode* parent,
    size_t index,
    const std::u16string& name,
    std::optional<base::Time> creation_time,
    std::optional<base::Time> last_modification_time,
    std::optional<base::Uuid> uuid) {
  DCHECK(loaded_);
  DCHECK(!uuid || uuid->is_valid());

  const base::Time provided_creation_time_or_now =
      creation_time.value_or(Time::Now());
  if (!last_modification_time)
    last_modification_time = provided_creation_time_or_now;

  std::unique_ptr<NoteNode> new_node = std::make_unique<NoteNode>(
      generate_next_node_id(), uuid.value_or(base::Uuid::GenerateRandomV4()),
      NoteNode::FOLDER);
  new_node->SetCreationTime(provided_creation_time_or_now);
  new_node->SetLastModificationTime(*last_modification_time);

  new_node->SetTitle(name);
  DCHECK(new_node->GetTitle() == name);
  DCHECK(new_node->is_folder());

  return AddNode(AsMutable(parent), index, std::move(new_node));
}

const NoteNode* NotesModel::AddSeparator(
    const NoteNode* parent,
    size_t index,
    std::optional<std::u16string> name,
    std::optional<base::Time> creation_time,
    std::optional<base::Uuid> uuid) {
  DCHECK(loaded_);
  DCHECK(!uuid || uuid->is_valid());

  if (!creation_time)
    creation_time = Time::Now();

  std::unique_ptr<NoteNode> new_node = std::make_unique<NoteNode>(
      generate_next_node_id(), uuid ? *uuid : base::Uuid::GenerateRandomV4(),
      NoteNode::SEPARATOR);
  new_node->SetCreationTime(*creation_time);
  if (name) {
    new_node->SetTitle(*name);
    DCHECK(new_node->GetTitle() == name);
  }

  DCHECK(new_node->is_separator());

  return AddNode(AsMutable(parent), index, std::move(new_node));
}

const NoteNode* NotesModel::AddAttachmentFromChecksum(
    const NoteNode* parent,
    size_t index,
    const std::u16string& title,
    const GURL& url,
    const std::string& checksum,
    std::optional<base::Time> creation_time,
    std::optional<base::Uuid> uuid) {
  DCHECK(loaded_);
  DCHECK(parent);
  DCHECK(parent->is_note());
  DCHECK(!uuid || uuid->is_valid());
  DCHECK(synced_file_store_);

  if (!creation_time)
    creation_time = Time::Now();

  std::unique_ptr<NoteNode> new_node = std::make_unique<NoteNode>(
      generate_next_node_id(), uuid ? *uuid : base::Uuid::GenerateRandomV4(),
      NoteNode::ATTACHMENT);
  new_node->SetTitle(title);
  new_node->SetCreationTime(*creation_time);
  new_node->SetContent(base::ASCIIToUTF16(checksum));
  new_node->SetURL(url);

  {
    // Only hold the lock for the duration of the insert.
    base::AutoLock url_lock(url_lock_);
    nodes_ordered_by_url_set_.insert(new_node.get());
  }

  synced_file_store_->SetLocalFileRef(new_node->uuid(), syncer::NOTES,
                                      checksum);
  return AddNode(AsMutable(parent), index, std::move(new_node));
}

const NoteNode* NotesModel::AddAttachment(
    const NoteNode* parent,
    size_t index,
    const std::u16string& title,
    const GURL& url,
    std::vector<uint8_t> content,
    std::optional<base::Time> creation_time,
    std::optional<base::Uuid> uuid) {
  DCHECK(loaded_);
  DCHECK(parent);
  DCHECK(parent->is_note());
  DCHECK(!uuid || uuid->is_valid());
  DCHECK(synced_file_store_);

  if (!creation_time)
    creation_time = Time::Now();

  std::unique_ptr<NoteNode> new_node = std::make_unique<NoteNode>(
      generate_next_node_id(), uuid ? *uuid : base::Uuid::GenerateRandomV4(),
      NoteNode::ATTACHMENT);
  new_node->SetTitle(title);
  new_node->SetCreationTime(*creation_time);
  new_node->SetURL(url);

  {
    // Only hold the lock for the duration of the insert.
    base::AutoLock url_lock(url_lock_);
    nodes_ordered_by_url_set_.insert(new_node.get());
  }

  std::string checksum = synced_file_store_->SetLocalFile(
      new_node->uuid(), syncer::NOTES, std::move(content));
  new_node->SetContent(base::ASCIIToUTF16(checksum));
  return AddNode(AsMutable(parent), index, std::move(new_node));
}

void NotesModel::SetTitle(const NoteNode* node,
                          const std::u16string& title,
                          bool updateLastModificationTime) {
  DCHECK(node);

  if (node->GetTitle() == title)
    return;

  if (is_permanent_node(node)) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  for (auto& observer : observers_)
    observer.OnWillChangeNotesNode(node);

  AsMutable(node)->SetTitle(title);

  if (updateLastModificationTime)
    UpdateLastModificationTime(node);

  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesNodeChanged(node);
}

void NotesModel::SetLastModificationTime(const NoteNode* node,
                                         const base::Time time) {
  DCHECK(node);

  if (node->GetLastModificationTime() == time || is_permanent_node(node))
    return;

  for (auto& observer : observers_)
    observer.OnWillChangeNotesNode(node);

  AsMutable(node)->SetLastModificationTime(time);
  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesNodeChanged(node);
}

void NotesModel::SetContent(const NoteNode* node,
                            const std::u16string& content,
                            bool updateLastModificationTime) {
  DCHECK(node);
  DCHECK(!node->is_folder());
  if (node->GetContent() == content)
    return;

  DCHECK(!node->is_attachment());

  for (auto& observer : observers_)
    observer.OnWillChangeNotesNode(node);

  AsMutable(node)->SetContent(content);

  if (updateLastModificationTime)
    UpdateLastModificationTime(node);

  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesNodeChanged(node);
}

void NotesModel::SetURL(const NoteNode* node,
                        const GURL& url,
                        bool updateLastModificationTime) {
  DCHECK(node);
  DCHECK(!node->is_folder());
  DCHECK(!node->is_separator());

  if (node->GetURL() == url)
    return;

  NoteNode* mutable_node = AsMutable(node);

  for (auto& observer : observers_)
    observer.OnWillChangeNotesNode(node);

  {
    base::AutoLock url_lock(url_lock_);
    RemoveNodeFromURLSet(mutable_node);
    mutable_node->SetURL(url);
    nodes_ordered_by_url_set_.insert(mutable_node);
  }

  if (updateLastModificationTime)
    UpdateLastModificationTime(node);

  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesNodeChanged(node);
}

void NotesModel::SetDateAdded(const NoteNode* node, base::Time date_added) {
  DCHECK(node);
  DCHECK(!is_permanent_node(node));

  if (node->GetCreationTime() == date_added)
    return;

  AsMutable(node)->SetCreationTime(date_added);

  // Syncing might result in dates newer than the folder's last modified date.
  if (date_added > node->parent()->GetCreationTime()) {
    // Will trigger store_->ScheduleSave().
    SetLastModificationTime(node->parent(), date_added);
  } else if (store_.get()) {
    store_->ScheduleSave();
  }
}

bool NotesModel::IsValidIndex(const NoteNode* parent,
                              size_t index,
                              bool allow_end) {
  return (parent && (parent->is_folder() || parent->is_note()) &&
          (index >= 0 && (index < parent->children().size() ||
                          (allow_end && index == parent->children().size()))));
}

void NotesModel::GetNodesByURL(const GURL& url,
                               std::vector<const NoteNode*>* nodes) {
  base::AutoLock url_lock(url_lock_);
  NoteNode tmp_node(0, base::Uuid::GenerateRandomV4(), NoteNode::NOTE);
  tmp_node.SetURL(url);
  NodesOrderedByURLSet::iterator i = nodes_ordered_by_url_set_.find(&tmp_node);
  while (i != nodes_ordered_by_url_set_.end() && (*i)->GetURL() == url) {
    nodes->push_back(*i);
    ++i;
  }
}

void NotesModel::Remove(const NoteNode* node, const base::Location& location) {
  DCHECK(loaded_);
  DCHECK(node);
  DCHECK(!is_root_node(node));

  const NoteNode* parent = node->parent();
  DCHECK(parent);
  size_t index = static_cast<size_t>(*parent->GetIndexOf(node));
  DCHECK_NE(static_cast<size_t>(-1), index);

  for (auto& observer : observers_)
    observer.OnWillRemoveNotes(parent, index, node, location);

  std::unique_ptr<NoteNode> owned_node;
  {
    base::AutoLock url_lock(url_lock_);
    RemoveNodeTreeFromURLSet(AsMutable(node));
    RemoveAttachmentsRecursive(AsMutable(node));
    owned_node = AsMutable(parent)->Remove(index);
  }

  SetLastModificationTime(parent);
  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesNodeRemoved(parent, index, node, location);
}

void NotesModel::RemoveAllUserNotes(const base::Location& location) {
  DCHECK(loaded_);
  for (auto& observer : observers_)
    observer.OnWillRemoveAllNotes(location);

  BeginExtensiveChanges();
  // Skip deleting permanent nodes. Permanent notes nodes are the root and
  // its immediate children. For removing all non permanent nodes just remove
  // all children of non-root permanent nodes.
  {
    base::AutoLock url_lock(url_lock_);
    nodes_ordered_by_url_set_.clear();

    for (const auto& permanent_node : root_->children()) {
      RemoveAttachmentsRecursive(permanent_node.get());
      permanent_node->DeleteAll();
    }
  }

  EndExtensiveChanges();
  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesAllNodesRemoved(location);
}

bool NotesModel::IsNotesNoLock(const GURL& url) {
  NoteNode tmp_node(0, base::Uuid::GenerateRandomV4(), NoteNode::NOTE);
  tmp_node.SetURL(url);
  return (nodes_ordered_by_url_set_.find(&tmp_node) !=
          nodes_ordered_by_url_set_.end());
}

void NotesModel::RemoveNodeTreeFromURLSet(NoteNode* node) {
  DCHECK(loaded_);
  DCHECK(node);
  DCHECK(!is_permanent_node(node));

  url_lock_.AssertAcquired();
  if (node->is_note() || node->is_attachment()) {
    RemoveNodeFromURLSet(node);
  }

  // Recurse through children.
  for (auto child_it = node->children().rbegin();
       child_it != node->children().rend(); ++child_it)
    RemoveNodeTreeFromURLSet((*child_it).get());
}

void NotesModel::RemoveNodeFromURLSet(NoteNode* node) {
  // NOTE: this is called in such a way that url_lock_ is already held. As
  // such, this doesn't explicitly grab the lock.
  NodesOrderedByURLSet::iterator i = nodes_ordered_by_url_set_.find(node);
  // i points to the first node with the URL, advance until we find the
  // node we're removing.
  while (i != nodes_ordered_by_url_set_.end() && *i != node)
    ++i;

  if (i != nodes_ordered_by_url_set_.end())
    nodes_ordered_by_url_set_.erase(i);
}

void NotesModel::RemoveAttachmentsRecursive(NoteNode* node) {
  if (!synced_file_store_)
    return;

  if (node->is_attachment())
    synced_file_store_->RemoveLocalRef(node->uuid(), syncer::NOTES);

  for (size_t i = node->children().size(); i > 0; --i)
    RemoveAttachmentsRecursive(node->children()[i - 1].get());
}

void NotesModel::BeginGroupedChanges() {
  for (auto& observer : observers_)
    observer.GroupedNotesChangesBeginning();
}

void NotesModel::EndGroupedChanges() {
  for (auto& observer : observers_)
    observer.GroupedNotesChangesEnded();
}

const NoteNode* NotesModel::GetNoteNodeByID(int64_t id) const {
  DCHECK(loaded_);
  return GetNodeByID(root_node(), id);
}

void NotesModel::Move(const NoteNode* node,
                      const NoteNode* new_parent,
                      size_t index) {
  DCHECK(loaded_);
  DCHECK(node);
  DCHECK(IsValidIndex(new_parent, index, true));
  DCHECK(!is_root_node(new_parent));
  DCHECK(!is_permanent_node(node));
  DCHECK(!new_parent->HasAncestor(node));
  DCHECK(node->is_attachment() && new_parent->is_note() ||
         !node->is_attachment() && new_parent->is_folder());

  const NoteNode* old_parent = node->parent();
  size_t old_index = old_parent->GetIndexOf(node).value();

  if (old_parent == new_parent &&
      (index == old_index || index == old_index + 1)) {
    // Node is already in this position, nothing to do.
    return;
  }

  UpdateLastModificationTime(old_parent);
  UpdateLastModificationTime(new_parent);

  if (old_parent == new_parent && index > old_index)
    index--;

  NoteNode* mutable_old_parent = AsMutable(old_parent);
  std::unique_ptr<NoteNode> owned_node = mutable_old_parent->Remove(old_index);
  NoteNode* mutable_new_parent = AsMutable(new_parent);
  mutable_new_parent->Add(std::move(owned_node), index);

  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesNodeMoved(old_parent, old_index, new_parent, index);
}

void NotesModel::SortChildren(const NoteNode* parent) {
  if (!parent || !parent->is_folder() || is_root_node(parent) ||
      parent->children().size() <= 1) {
    return;
  }

  for (auto& observer : observers_)
    observer.OnWillReorderNotesNode(parent);

  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(icu::Collator::createInstance(error));
  if (U_FAILURE(error))
    collator.reset(NULL);
  AsMutable(parent)->SortChildren(SortComparator(collator.get()));

  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesNodeChildrenReordered(parent);
}

void NotesModel::ReorderChildren(
    const NoteNode* parent,
    const std::vector<const NoteNode*>& ordered_nodes) {
  // Ensure that all children in |parent| are in |ordered_nodes|.
  DCHECK_EQ(parent->children().size(), ordered_nodes.size());
  for (size_t i = 0; i < ordered_nodes.size(); ++i)
    DCHECK_EQ(parent, ordered_nodes[i]->parent());

  for (auto& observer : observers_)
    observer.OnWillReorderNotesNode(parent);

  if (ordered_nodes.size() > 1) {
    std::map<const NoteNode*, size_t> order;
    for (size_t i = 0; i < ordered_nodes.size(); ++i)
      order[ordered_nodes[i]] = i;

    std::vector<size_t> new_order(ordered_nodes.size());
    for (size_t old_index = 0; old_index < parent->children().size();
         ++old_index) {
      const NoteNode* node = parent->children()[old_index].get();
      size_t new_index = order[node];
      new_order[old_index] = new_index;
    }

    AsMutable(parent)->ReorderChildren(new_order);

    if (store_.get())
      store_->ScheduleSave();
  }

  for (auto& observer : observers_)
    observer.NotesNodeChildrenReordered(parent);
}

void NotesModel::GetNotesMatching(const std::u16string& text,
                                  size_t max_count,
                                  std::vector<const NoteNode*>& matches) {
  if (!loaded_)
    return;

  if (text.length() > 0) {
    ui::TreeNodeIterator<NoteNode> iterator(root_.get());

    while (iterator.has_next()) {
      NoteNode* node = iterator.Next();
      bool match = false;
      match = base::i18n::StringSearchIgnoringCaseAndAccents(
          text, node->GetContent(), NULL, NULL);
      if (!match && node->GetURL().is_valid()) {
        std::string value = node->GetURL().host() + node->GetURL().path();
        match = base::i18n::StringSearchIgnoringCaseAndAccents(
            text, base::UTF8ToUTF16(value), NULL, NULL);
      }
      if (match) {
        matches.push_back(node);
      }
    }
  }
}

void NotesModel::GetNotesFoldersMatching(
    const std::u16string& text,
    size_t max_count,
    std::vector<const NoteNode*>& matches) {
  if (!loaded_)
    return;

  if (text.length() > 0) {
    ui::TreeNodeIterator<NoteNode> iterator(root_.get());

    while (iterator.has_next()) {
      NoteNode* node = iterator.Next();
      if (!node->is_folder()) {
        continue;
      }
      bool match = false;
      match = base::i18n::StringSearchIgnoringCaseAndAccents(
          text, node->GetTitle(), NULL, NULL);
      if (match) {
        matches.push_back(node);
      }
    }
  }
}

int64_t NotesModel::generate_next_node_id() {
  DCHECK(loaded_);
  return next_node_id_++;
}

}  // namespace vivaldi
