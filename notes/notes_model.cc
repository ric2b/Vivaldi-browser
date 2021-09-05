// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "notes/notes_model.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/guid.h"
#include "base/i18n/string_compare.h"
#include "base/i18n/string_search.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "importer/imported_notes_entry.h"
#include "notes/note_load_details.h"
#include "notes/note_node.h"
#include "notes/notes_storage.h"
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
  icu::Collator* collator_;
};
}  // namespace

// Helper to get a mutable notes node.
NoteNode* AsMutable(const NoteNode* node) {
  return const_cast<NoteNode*>(node);
}

NotesModel::NotesModel(sync_notes::NoteSyncService* sync_service)
    : root_(std::make_unique<NoteNode>(
          0,
          base::GUID::ParseLowercase(NoteNode::kRootNodeGuid),
          NoteNode::FOLDER)),
      sync_service_(sync_service) {}

NotesModel::~NotesModel() {
  for (auto& observer : observers_)
    observer.NotesModelBeingDeleted(this);

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
      details->ids_reassigned() || details->guids_reassigned()) {
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

  loaded_ = true;

  if (sync_service_) {
    sync_service_->DecodeNoteSyncMetadata(
        details->sync_metadata_str(),
        store_ ? base::BindRepeating(&NotesStorage::ScheduleSave,
                                     base::Unretained(store_.get()))
               : base::DoNothing(),
        this);
  }

  // Notify our direct observers.
  for (auto& observer : observers_)
    observer.NotesModelLoaded(this, details->ids_reassigned());
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

bool NotesModel::LoadNotes() {
  return false;
}

bool NotesModel::SaveNotes() {
  if (store_.get()) {
    store_->ScheduleSave();
    return true;
  }
  return false;
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
      observer.ExtensiveNotesChangesBeginning(this);
  }
}

void NotesModel::EndExtensiveChanges() {
  --extensive_changes_;
  DCHECK_GE(extensive_changes_, 0);
  if (extensive_changes_ == 0) {
    for (auto& observer : observers_)
      observer.ExtensiveNotesChangesEnded(this);
  }
}

NoteNode* NotesModel::AddNode(NoteNode* parent,
                              int index,
                              std::unique_ptr<NoteNode> node) {
  NoteNode* node_ptr = node.get();
  if (!parent)
    parent = root_.get();

  parent->Add(std::move(node), index);

  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesNodeAdded(this, parent, index);

  return node_ptr;
}

NoteNode* NotesModel::AddNote(const NoteNode* parent,
                              size_t index,
                              const base::string16& title,
                              const GURL& url,
                              const base::string16& content,
                              base::Optional<base::Time> creation_time,
                              base::Optional<base::GUID> guid) {
  DCHECK(loaded_);
  DCHECK(!guid || guid->is_valid());

  if (!creation_time)
    creation_time = Time::Now();

  std::unique_ptr<NoteNode> new_node = std::make_unique<NoteNode>(
      generate_next_node_id(), guid ? *guid : base::GUID::GenerateRandomV4(),
      NoteNode::NOTE);
  new_node->SetTitle(title);
  new_node->SetCreationTime(*creation_time);
  new_node->SetContent(content);
  new_node->SetURL(url);

  {
    // Only hold the lock for the duration of the insert.
    base::AutoLock url_lock(url_lock_);
    nodes_ordered_by_url_set_.insert(new_node.get());
  }

  return AddNode(AsMutable(parent), index, std::move(new_node));
}

NoteNode* NotesModel::ImportNote(const NoteNode* parent,
                                 size_t index,
                                 const ImportedNotesEntry& note) {
  if (!loaded_)
    return NULL;

  int64_t id = generate_next_node_id();

  std::unique_ptr<NoteNode> new_node = std::make_unique<NoteNode>(
      id, base::GUID::GenerateRandomV4(),
      note.is_folder ? NoteNode::FOLDER : NoteNode::NOTE);
  new_node->SetTitle(note.title);
  new_node->SetCreationTime(note.creation_time);

  if (!note.is_folder) {
    new_node->SetURL(note.url);
    new_node->SetContent(note.content);
  }
  return AddNode(AsMutable(parent), index, std::move(new_node));
}

NoteNode* NotesModel::AddFolder(const NoteNode* parent,
                                size_t index,
                                const base::string16& name,
                                base::Optional<base::GUID> guid) {
  DCHECK(loaded_);
  DCHECK(!guid || guid->is_valid());

  std::unique_ptr<NoteNode> new_node = std::make_unique<NoteNode>(
      generate_next_node_id(), guid ? *guid : base::GUID::GenerateRandomV4(),
      NoteNode::FOLDER);
  new_node->SetTitle(name);
  DCHECK(new_node->GetTitle() == name);
  DCHECK(new_node->is_folder());

  return AddNode(AsMutable(parent), index, std::move(new_node));
}

NoteNode* NotesModel::AddSeparator(const NoteNode* parent,
                                   size_t index,
                                   base::Optional<base::string16> name,
                                   base::Optional<base::Time> creation_time,
                                   base::Optional<base::GUID> guid) {
  DCHECK(loaded_);
  DCHECK(!guid || guid->is_valid());

  if (!creation_time)
    creation_time = Time::Now();

  std::unique_ptr<NoteNode> new_node = std::make_unique<NoteNode>(
      generate_next_node_id(), guid ? *guid : base::GUID::GenerateRandomV4(),
      NoteNode::SEPARATOR);
  if (name)
    new_node->SetTitle(*name);
  new_node->SetCreationTime(*creation_time);

  return AddNode(AsMutable(parent), index, std::move(new_node));
}

void NotesModel::SetTitle(const NoteNode* node, const base::string16& title) {
  if (!node) {
    NOTREACHED();
    return;
  }
  if (node->GetTitle() == title)
    return;

  if (is_permanent_node(node)) {
    NOTREACHED();
    return;
  }

  for (auto& observer : observers_)
    observer.OnWillChangeNotesNode(this, node);

  AsMutable(node)->SetTitle(title);

  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesNodeChanged(this, node);
}

void NotesModel::SetContent(const NoteNode* node,
                            const base::string16& content) {
  if (!node) {
    NOTREACHED();
    return;
  }
  if (node->GetContent() == content)
    return;

  if (is_permanent_node(node)) {
    NOTREACHED();
    return;
  }

  for (auto& observer : observers_)
    observer.OnWillChangeNotesNode(this, node);

  AsMutable(node)->SetContent(content);

  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesNodeChanged(this, node);
}

void NotesModel::SetURL(const NoteNode* node, const GURL& url) {
  if (!node) {
    NOTREACHED();
    return;
  }

  // We cannot change the URL of a folder.
  if (node->is_folder()) {
    NOTREACHED();
    return;
  }

  if (node->GetURL() == url)
    return;

  NoteNode* mutable_node = AsMutable(node);

  for (auto& observer : observers_)
    observer.OnWillChangeNotesNode(this, node);

  {
    base::AutoLock url_lock(url_lock_);
    RemoveNodeFromURLSet(mutable_node);
    mutable_node->SetURL(url);
    nodes_ordered_by_url_set_.insert(mutable_node);
  }

  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesNodeChanged(this, node);
}

void NotesModel::ClearAttachments(const NoteNode* node) {
  if (!node) {
    NOTREACHED();
    return;
  }

  for (auto& observer : observers_)
    observer.OnWillChangeNotesNode(this, node);

  AsMutable(node)->ClearAttachments();

  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesNodeChanged(this, node);
}

void NotesModel::AddAttachment(const NoteNode* node,
                               NoteAttachment&& attachment) {
  if (!node) {
    NOTREACHED();
    return;
  }

  for (auto& observer : observers_)
    observer.OnWillChangeNotesNode(this, node);

  AsMutable(node)->AddAttachment(std::forward<NoteAttachment>(attachment));

  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesNodeChanged(this, node);
}

void NotesModel::SwapAttachments(const NoteNode* node1, const NoteNode* node2) {
  if (!node1 || !node2) {
    NOTREACHED();
    return;
  }

  for (auto& observer : observers_)
    observer.OnWillChangeNotesNode(this, node1);

  for (auto& observer : observers_)
    observer.OnWillChangeNotesNode(this, node2);

  AsMutable(node1)->SwapAttachments(AsMutable(node2));

  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesNodeChanged(this, node1);

  for (auto& observer : observers_)
    observer.NotesNodeChanged(this, node2);
}

void NotesModel::SetDateFolderModified(const NoteNode* parent,
                                       const Time time) {
  DCHECK(parent);
  AsMutable(parent)->SetCreationTime(time);

  if (store_.get())
    store_->ScheduleSave();
}

void NotesModel::SetDateAdded(const NoteNode* node, base::Time date_added) {
  if (!node) {
    NOTREACHED();
    return;
  }

  if (node->GetCreationTime() == date_added)
    return;

  if (is_permanent_node(node)) {
    NOTREACHED();
    return;
  }

  AsMutable(node)->SetCreationTime(date_added);

  // Syncing might result in dates newer than the folder's last modified date.
  if (date_added > node->parent()->GetCreationTime()) {
    // Will trigger store_->ScheduleSave().
    SetDateFolderModified(node->parent(), date_added);
  } else if (store_.get()) {
    store_->ScheduleSave();
  }
}

bool NotesModel::IsValidIndex(const NoteNode* parent,
                              size_t index,
                              bool allow_end) {
  return (parent && parent->is_folder() &&
          (index >= 0 && (index < parent->children().size() ||
                          (allow_end && index == parent->children().size()))));
}

void NotesModel::GetNodesByURL(const GURL& url,
                               std::vector<const NoteNode*>* nodes) {
  base::AutoLock url_lock(url_lock_);
  NoteNode tmp_node(0, base::GUID::GenerateRandomV4(), NoteNode::NOTE);
  tmp_node.SetURL(url);
  NodesOrderedByURLSet::iterator i = nodes_ordered_by_url_set_.find(&tmp_node);
  while (i != nodes_ordered_by_url_set_.end() && (*i)->GetURL() == url) {
    nodes->push_back(*i);
    ++i;
  }
}

void NotesModel::Remove(const NoteNode* node) {
  DCHECK(loaded_);
  DCHECK(node);
  DCHECK(!is_root_node(node));

  const NoteNode* parent = node->parent();
  DCHECK(parent);
  int index = parent->GetIndexOf(node);

  RemoveAndDeleteNode(AsMutable(parent), index, AsMutable(node));
}

void NotesModel::RemoveAllUserNotes() {
  for (auto& observer : observers_)
    observer.OnWillRemoveAllNotes(this);

  BeginExtensiveChanges();
  // Skip deleting permanent nodes. Permanent notes nodes are the root and
  // its immediate children. For removing all non permanent nodes just remove
  // all children of non-root permanent nodes.
  {
    base::AutoLock url_lock(url_lock_);
    nodes_ordered_by_url_set_.clear();
    main_node_->DeleteAll();
  }

  EndExtensiveChanges();
  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesAllNodesRemoved(this);
}

bool NotesModel::IsNotesNoLock(const GURL& url) {
  NoteNode tmp_node(0, base::GUID::GenerateRandomV4(), NoteNode::NOTE);
  tmp_node.SetURL(url);
  return (nodes_ordered_by_url_set_.find(&tmp_node) !=
          nodes_ordered_by_url_set_.end());
}

void NotesModel::RemoveNodeTreeFromURLSet(NoteNode* node) {
  if (!loaded_ || !node || is_permanent_node(node)) {
    NOTREACHED();
    return;
  }

  url_lock_.AssertAcquired();
  if (node->is_note()) {
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

bool NotesModel::Remove(NoteNode* parent, size_t index) {
  if (!parent)
    return false;
  NoteNode* node = parent->children()[index].get();
  RemoveAndDeleteNode(parent, index, node);
  return true;
}

void NotesModel::RemoveAndDeleteNode(NoteNode* parent,
                                     size_t index,
                                     NoteNode* node_ptr) {
  std::unique_ptr<NoteNode> node;

  for (auto& observer : observers_)
    observer.OnWillRemoveNotes(this, parent, index, node_ptr);

  {
    base::AutoLock url_lock(url_lock_);
    RemoveNodeTreeFromURLSet(node_ptr);
    node = parent->Remove(index);
  }

  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesNodeRemoved(this, parent, index, node.get());
}

void NotesModel::BeginGroupedChanges() {
  for (auto& observer : observers_)
    observer.GroupedNotesChangesBeginning(this);
}

void NotesModel::EndGroupedChanges() {
  for (auto& observer : observers_)
    observer.GroupedNotesChangesEnded(this);
}

const NoteNode* GetNotesNodeByID(const NotesModel* model, int64_t id) {
  // TODO(sky): TreeNode needs a method that visits all nodes using a predicate.
  return GetNodeByID(model->root_node(), id);
}

const NoteNode* GetNodeByID(const NoteNode* node, int64_t id) {
  if (node->id() == id)
    return node;

  for (auto& it : node->children()) {
    const NoteNode* result = GetNodeByID(it.get(), id);
    if (result)
      return result;
  }
  return NULL;
}

bool NotesModel::Move(const NoteNode* node,
                      const NoteNode* new_parent,
                      size_t index) {
  if (!loaded_ || !node || !IsValidIndex(new_parent, index, true) ||
      is_root_node(new_parent) || is_permanent_node(node)) {
    NOTREACHED();
    return false;
  }

  DCHECK(!new_parent->HasAncestor(node));
  if (new_parent->HasAncestor(node)) {
    // Can't make an ancestor of the node be a child of the node.
    NOTREACHED();
    return false;
  }

  const NoteNode* old_parent = node->parent();
  size_t old_index = old_parent->GetIndexOf(node);

  if (old_parent == new_parent &&
      (index == old_index || index == old_index + 1)) {
    // Node is already in this position, nothing to do.
    return false;
  }

  SetDateFolderModified(new_parent, Time::Now());

  if (old_parent == new_parent && index > old_index)
    index--;

  NoteNode* mutable_old_parent = AsMutable(old_parent);
  std::unique_ptr<NoteNode> owned_node = mutable_old_parent->Remove(old_index);
  NoteNode* mutable_new_parent = AsMutable(new_parent);
  mutable_new_parent->Add(std::move(owned_node), index);

  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesNodeMoved(this, old_parent, old_index, new_parent, index);

  return true;
}

void NotesModel::SortChildren(const NoteNode* parent) {
  if (!parent || !parent->is_folder() || is_root_node(parent) ||
      parent->children().size() <= 1) {
    return;
  }

  for (auto& observer : observers_)
    observer.OnWillReorderNotesNode(this, parent);

  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(icu::Collator::createInstance(error));
  if (U_FAILURE(error))
    collator.reset(NULL);
  NoteNode* mutable_parent = AsMutable(parent);
  std::sort(mutable_parent->children_.begin(), mutable_parent->children_.end(),
            SortComparator(collator.get()));

  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesNodeChildrenReordered(this, parent);
}

void NotesModel::ReorderChildren(
    const NoteNode* parent,
    const std::vector<const NoteNode*>& ordered_nodes) {
  // Ensure that all children in |parent| are in |ordered_nodes|.
  DCHECK_EQ(parent->children().size(), ordered_nodes.size());
  for (size_t i = 0; i < ordered_nodes.size(); ++i)
    DCHECK_EQ(parent, ordered_nodes[i]->parent());

  for (auto& observer : observers_)
    observer.OnWillReorderNotesNode(this, parent);

  if (ordered_nodes.size() > 1) {
    std::map<const NoteNode*, size_t> order;
    for (size_t i = 0; i < ordered_nodes.size(); ++i)
      order[ordered_nodes[i]] = i;

    std::vector<std::unique_ptr<NoteNode>> new_children(ordered_nodes.size());
    NoteNode* mutable_parent = AsMutable(parent);
    for (auto& child : mutable_parent->children_) {
      size_t new_location = order[child.get()];
      new_children[new_location] = std::move(child);
    }
    mutable_parent->children_.swap(new_children);
  }

  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesNodeChildrenReordered(this, parent);
}

void NotesModel::GetNotesMatching(
    const base::string16& text,
    size_t max_count,
    std::vector<std::pair<int, NoteNode::Type>>* matches) {
  if (!loaded_)
    return;

  std::vector<vivaldi::NoteNode> search_result;
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
        matches->push_back(
            std::pair<int, NoteNode::Type>(node->id(), node->type()));
      }
    }
  }
}

int64_t NotesModel::generate_next_node_id() {
  DCHECK(loaded_);
  return next_node_id_++;
}

}  // namespace vivaldi
