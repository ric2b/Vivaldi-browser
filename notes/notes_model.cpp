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

#include "app/vivaldi_resources.h"
#include "base/i18n/string_compare.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "importer/imported_notes_entry.h"
#include "notes/notes_storage.h"
#include "notes/notesnode.h"
#include "ui/base/l10n/l10n_util.h"

using base::Time;

namespace vivaldi {
namespace {
// Comparator used when sorting notes. Folders are sorted first, then
// notes.
class SortComparator {
 public:
  explicit SortComparator(icu::Collator* collator) : collator_(collator) {}

  // Returns true if |n1| preceeds |n2|.
  bool operator()(const std::unique_ptr<Notes_Node>& n1,
                  const std::unique_ptr<Notes_Node>& n2) {
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

static const char kNotes[] = "Notes";
static const char kOtherNotes[] = "Other Notes";

// Helper to get a mutable notes node.
Notes_Node* AsMutable(const Notes_Node* node) {
  return const_cast<Notes_Node*>(node);
}

Notes_Model::Notes_Model(content::BrowserContext* context)
    : context_(context),
      root_(0),
      main_node_(nullptr),
      other_node_(nullptr),
      trash_node_(nullptr),
      loaded_(false),
      loaded_signal_(base::WaitableEvent::ResetPolicy::MANUAL,
                     base::WaitableEvent::InitialState::NOT_SIGNALED),
      extensive_changes_(0),
      current_index_(0) {  // root_ has 0
  root_.SetType(Notes_Node::FOLDER);
}

Notes_Model::~Notes_Model() {
  for (auto& observer : observers_)
    observer.NotesModelBeingDeleted(this);

  if (store_.get()) {
    // The store maintains a reference back to us. We need to tell it we're gone
    // so that it doesn't try and invoke a method back on us again.
    store_->NotesModelDeleted();
  }
}

void Notes_Model::Shutdown() {
  if (loaded_)
    return;

  // See comment in HistoryService::ShutdownOnUIThread where this is invoked for
  // details. It is also called when the Notes_Model is deleted.
  loaded_signal_.Signal();
}

// Static
std::unique_ptr<Notes_Model> Notes_Model::CreateModel() {
  std::unique_ptr<Notes_Model> model(new Notes_Model(NULL));
  std::unique_ptr<NotesLoadDetails> details(model->CreateLoadDetails());
  model->DoneLoading(std::move(details));
  return model;
}

std::unique_ptr<NotesLoadDetails> Notes_Model::CreateLoadDetails() {
  Notes_Node* main_node = new Notes_Node(GetNewIndex());
  main_node->SetType(Notes_Node::FOLDER);
  main_node->SetTitle(base::ASCIIToUTF16(kNotes));

  Notes_Node* other_node = new Notes_Node(GetNewIndex());
  other_node->SetType(Notes_Node::OTHER);
  other_node->SetTitle(base::ASCIIToUTF16(kOtherNotes));

  Notes_Node* trash_node = new Notes_Node(GetNewIndex());
  trash_node->SetType(Notes_Node::TRASH);
  trash_node->SetTitle(l10n_util::GetStringUTF16(IDS_NOTES_TRASH_FOLDER_NAME));

  return base::WrapUnique(
      new NotesLoadDetails(main_node, other_node, trash_node, current_index_));
}

void Notes_Model::Load(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
  if (store_.get()) {
    // If the store is non-null, it means Load was already invoked. Load should
    // only be invoked once.
    NOTREACHED();
    return;
  }

  // Load the notes. NotesStorage notifies us when done.
  store_.reset(new NotesStorage(context_, this, task_runner.get()));
  store_->LoadNotes(CreateLoadDetails());
}

void Notes_Model::DoneLoading(std::unique_ptr<NotesLoadDetails> details) {
  DCHECK(details);
  if (loaded_) {
    // We should only ever be loaded once.
    NOTREACHED();
    return;
  }

  if (details->computed_checksum() != details->stored_checksum() ||
      details->ids_reassigned()) {
    // If notes file changed externally, the IDs may have changed
    // externally. In that case, the decoder may have reassigned IDs to make
    // them unique. So when the file has changed externally, we should save the
    // notes file to persist new IDs.
    if (store_.get())
      store_->ScheduleSave();
  }
  std::unique_ptr<Notes_Node> main_node_owned = details->release_notes_node();
  std::unique_ptr<Notes_Node> other_node_owned =
      details->release_other_notes_node();
  std::unique_ptr<Notes_Node> trash_node_owned =
      details->release_trash_notes_node();
  main_node_ = main_node_owned.get();
  other_node_ = other_node_owned.get();
  trash_node_ = trash_node_owned.get();
  root_.Add(std::move(main_node_owned), 0);
  root_.Add(std::move(other_node_owned), 1);
  root_.Add(std::move(trash_node_owned), 2);
  current_index_ = details->highest_id();

  loaded_ = true;

  // Check if we have trash and add it if we don't.
  trash_node_ = GetOrCreateTrashNode();

  loaded_signal_.Signal();

  // Notify our direct observers.
  for (auto& observer : observers_)
    observer.NotesModelLoaded(this, details->ids_reassigned());
}

void Notes_Model::GetNotes(std::vector<Notes_Model::URLAndTitle>* notes) {
  base::AutoLock url_lock(url_lock_);
  const GURL* last_url = NULL;
  for (auto* i : nodes_ordered_by_url_set_) {
    const GURL* url = &(i->GetURL());
    // Only add unique URLs.
    if (!last_url || *url != *last_url) {
      Notes_Model::URLAndTitle note;
      note.url = *url;
      note.title = i->GetTitle();
      note.content = i->GetContent();
      notes->push_back(note);
    }
    last_url = url;
  }
}

void Notes_Model::BlockTillLoaded() {
  loaded_signal_.Wait();
}

bool Notes_Model::LoadNotes() {
  return false;
}

bool Notes_Model::SaveNotes() {
  if (store_.get()) {
    store_->ScheduleSave();
    return true;
  }
  return false;
}

void Notes_Model::AddObserver(NotesModelObserver* observer) {
  observers_.AddObserver(observer);
}

void Notes_Model::RemoveObserver(NotesModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void Notes_Model::BeginExtensiveChanges() {
  if (++extensive_changes_ == 1) {
    for (auto& observer : observers_)
      observer.ExtensiveNotesChangesBeginning(this);
  }
}

void Notes_Model::EndExtensiveChanges() {
  --extensive_changes_;
  DCHECK_GE(extensive_changes_, 0);
  if (extensive_changes_ == 0) {
    for (auto& observer : observers_)
      observer.ExtensiveNotesChangesEnded(this);
  }
}

Notes_Node* Notes_Model::AddNode(Notes_Node* parent,
                                 int index,
                                 std::unique_ptr<Notes_Node> node) {
  Notes_Node* node_ptr = node.get();
  if (!parent)
    parent = &root_;

  parent->Add(std::move(node), index);

  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesNodeAdded(this, parent, index);

  return node_ptr;
}

Notes_Node* Notes_Model::AddNote(const Notes_Node* parent,
                                 int index,
                                 const base::string16& subject,
                                 const GURL& url,
                                 const base::string16& content) {
  if (!loaded_)
    return NULL;

  int64_t id = GetNewIndex();

  std::unique_ptr<Notes_Node> new_node = std::make_unique<Notes_Node>(id);
  new_node->SetTitle(subject);
  new_node->SetContent(content);
  new_node->SetURL(url);

  {
    // Only hold the lock for the duration of the insert.
    base::AutoLock url_lock(url_lock_);
    nodes_ordered_by_url_set_.insert(new_node.get());
  }

  return AddNode(AsMutable(parent), index, std::move(new_node));
}

Notes_Node* Notes_Model::AddNote(const Notes_Node* parent,
                                 int index,
                                 bool is_folder,
                                 const ImportedNotesEntry& note) {
  if (!loaded_)
    return NULL;

  int64_t id = GetNewIndex();

  std::unique_ptr<Notes_Node> new_node = std::make_unique<Notes_Node>(id);
  new_node->SetTitle(note.title);
  new_node->SetCreationTime(note.creation_time);

  if (is_folder) {
    new_node->SetType(Notes_Node::FOLDER);
  } else {
    new_node->SetURL(note.url);
    new_node->SetContent(note.content);
  }
  return AddNode(AsMutable(parent), index, std::move(new_node));
}

Notes_Node* Notes_Model::AddFolder(const Notes_Node* parent,
                                   int index,
                                   const base::string16& name) {
  DCHECK(loaded_);
  if (!loaded_)
    return NULL;

  int64_t id = GetNewIndex();
  std::unique_ptr<Notes_Node> new_node = std::make_unique<Notes_Node>(id);
  new_node->SetTitle(name);
  new_node->SetType(Notes_Node::FOLDER);
  DCHECK(new_node->GetTitle() == name);
  DCHECK(new_node->is_folder());

  return AddNode(AsMutable(parent), index, std::move(new_node));
}

void Notes_Model::SetTitle(const Notes_Node* node,
                           const base::string16& title) {
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

void Notes_Model::SetContent(const Notes_Node* node,
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

void Notes_Model::SetURL(const Notes_Node* node, const GURL& url) {
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

  Notes_Node* mutable_node = AsMutable(node);

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

void Notes_Model::ClearAttachments(const Notes_Node* node) {
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

void Notes_Model::AddAttachment(const Notes_Node* node,
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

void Notes_Model::SetDateFolderModified(const Notes_Node* parent,
                                        const Time time) {
  DCHECK(parent);
  AsMutable(parent)->SetCreationTime(time);

  if (store_.get())
    store_->ScheduleSave();
}

void Notes_Model::SetDateAdded(const Notes_Node* node, base::Time date_added) {
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

bool Notes_Model::IsValidIndex(const Notes_Node* parent,
                               int index,
                               bool allow_end) {
  return (parent && parent->is_folder() &&
          (index >= 0 && (index < parent->child_count() ||
                          (allow_end && index == parent->child_count()))));
}

void Notes_Model::GetNodesByURL(const GURL& url,
                                std::vector<const Notes_Node*>* nodes) {
  base::AutoLock url_lock(url_lock_);
  Notes_Node tmp_node(0);
  tmp_node.SetURL(url);
  NodesOrderedByURLSet::iterator i = nodes_ordered_by_url_set_.find(&tmp_node);
  while (i != nodes_ordered_by_url_set_.end() && (*i)->GetURL() == url) {
    nodes->push_back(*i);
    ++i;
  }
}

void Notes_Model::Remove(const Notes_Node* node) {
  DCHECK(loaded_);
  DCHECK(node);
  DCHECK(!is_root_node(node));

  const Notes_Node* parent = node->parent();
  DCHECK(parent);
  int index = parent->GetIndexOf(node);

  RemoveAndDeleteNode(AsMutable(parent), index, AsMutable(node));
}

void Notes_Model::RemoveAllUserNotes() {
  std::vector<std::unique_ptr<Notes_Node>> removed_nodes;

  for (auto& observer : observers_)
    observer.OnWillRemoveAllNotes(this);

  BeginExtensiveChanges();
  // Skip deleting permanent nodes. Permanent notes nodes are the root and
  // its immediate children. For removing all non permanent nodes just remove
  // all children of non-root permanent nodes.
  {
    base::AutoLock url_lock(url_lock_);
    Notes_Node* permanent_node = main_node_;

    for (int j = permanent_node->child_count() - 1; j >= 0; --j) {
      removed_nodes.push_back(
          permanent_node->Remove(permanent_node->GetChild(j)));
    }
  }
  EndExtensiveChanges();
  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesAllNodesRemoved(this);
}

bool Notes_Model::IsNotesNoLock(const GURL& url) {
  Notes_Node tmp_node(0);
  tmp_node.SetURL(url);
  return (nodes_ordered_by_url_set_.find(&tmp_node) !=
          nodes_ordered_by_url_set_.end());
}

void Notes_Model::RemoveNodeTreeFromURLSet(Notes_Node* node) {
  if (!loaded_ || !node || is_permanent_node(node)) {
    NOTREACHED();
    return;
  }

  url_lock_.AssertAcquired();
  if (node->is_note()) {
    RemoveNodeFromURLSet(node);
  }

  // Recurse through children.
  for (int i = node->child_count() - 1; i >= 0; --i)
    RemoveNodeTreeFromURLSet(node->GetChild(i));
}

void Notes_Model::RemoveNodeFromURLSet(Notes_Node* node) {
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

bool Notes_Model::Remove(Notes_Node* parent, int index) {
  if (!parent)
    return false;
  Notes_Node* node = parent->GetChild(index);
  RemoveAndDeleteNode(parent, index, node);
  return true;
}

void Notes_Model::RemoveAndDeleteNode(Notes_Node* parent,
                                      int index,
                                      Notes_Node* node_ptr) {
  std::unique_ptr<Notes_Node> node;

  for (auto& observer : observers_)
    observer.OnWillRemoveNotes(this, parent, index, node_ptr);

  {
    base::AutoLock url_lock(url_lock_);
    RemoveNodeTreeFromURLSet(node_ptr);
    node = parent->Remove(node_ptr);
  }

  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesNodeRemoved(this, parent, index, node.get());
}

void Notes_Model::SetNodeSyncTransactionVersion(
    const Notes_Node* node,
    int64_t sync_transaction_version) {
  if (sync_transaction_version == node->sync_transaction_version())
    return;

  AsMutable(node)->set_sync_transaction_version(sync_transaction_version);
  if (store_.get())
    store_->ScheduleSave();
}

void Notes_Model::BeginGroupedChanges() {
  for (auto& observer : observers_)
    observer.GroupedNotesChangesBeginning(this);
}

void Notes_Model::EndGroupedChanges() {
  for (auto& observer : observers_)
    observer.GroupedNotesChangesEnded(this);
}

const Notes_Node* GetNotesNodeByID(const Notes_Model* model, int64_t id) {
  // TODO(sky): TreeNode needs a method that visits all nodes using a predicate.
  return GetNodeByID(model->root_node(), id);
}

const Notes_Node* GetNodeByID(const Notes_Node* node, int64_t id) {
  if (node->id() == id)
    return node;

  for (int i = 0, child_count = node->child_count(); i < child_count; ++i) {
    const Notes_Node* result = GetNodeByID(node->GetChild(i), id);
    if (result)
      return result;
  }
  return NULL;
}

bool Notes_Model::Move(const Notes_Node* node,
                       const Notes_Node* new_parent,
                       int index) {
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

  const Notes_Node* old_parent = node->parent();
  int old_index = old_parent->GetIndexOf(node);

  if (old_parent == new_parent &&
      (index == old_index || index == old_index + 1)) {
    // Node is already in this position, nothing to do.
    return false;
  }

  SetDateFolderModified(new_parent, Time::Now());

  if (old_parent == new_parent && index > old_index)
    index--;

  Notes_Node* mutable_old_parent = AsMutable(old_parent);
  std::unique_ptr<Notes_Node> owned_node =
      mutable_old_parent->Remove(AsMutable(node));
  Notes_Node* mutable_new_parent = AsMutable(new_parent);
  mutable_new_parent->Add(std::move(owned_node), index);

  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesNodeMoved(this, old_parent, old_index, new_parent, index);

  return true;
}

Notes_Node* Notes_Model::GetOrCreateTrashNode() {
  Notes_Node* root_node_p = root_node();
  for (int i = 0; i < root_node_p->child_count(); ++i) {
    Notes_Node* child = root_node_p->GetChild(i);
    if (child->is_trash()) {
      // Move it to the end of the list.
      std::unique_ptr<Notes_Node> move_child = root_node_p->Remove(child);
      AddNode(root_node_p, root_node_p->child_count(), std::move(move_child));
      return child;
    }
  }
  std::unique_ptr<Notes_Node> trash =
      std::make_unique<Notes_Node>(GetNewIndex());
  Notes_Node* trash_p = trash.get();
  if (trash) {
    trash->SetType(Notes_Node::TRASH);
    trash->SetTitle(l10n_util::GetStringUTF16(IDS_NOTES_TRASH_FOLDER_NAME));
    AddNode(root_node_p, root_node_p->child_count(), std::move(trash));
  }
  return trash_p;
}

void Notes_Model::SortChildren(const Notes_Node* parent) {
  if (!parent || !parent->is_folder() || is_root_node(parent) ||
      parent->child_count() <= 1) {
    return;
  }

  for (auto& observer : observers_)
    observer.OnWillReorderNotesNode(this, parent);

  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(icu::Collator::createInstance(error));
  if (U_FAILURE(error))
    collator.reset(NULL);
  Notes_Node* mutable_parent = AsMutable(parent);
  std::sort(mutable_parent->children().begin(),
            mutable_parent->children().end(), SortComparator(collator.get()));

  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesNodeChildrenReordered(this, parent);
}

void Notes_Model::ReorderChildren(
    const Notes_Node* parent,
    const std::vector<const Notes_Node*>& ordered_nodes) {
  // Ensure that all children in |parent| are in |ordered_nodes|.
  DCHECK_EQ(static_cast<size_t>(parent->child_count()), ordered_nodes.size());
  for (size_t i = 0; i < ordered_nodes.size(); ++i)
    DCHECK_EQ(parent, ordered_nodes[i]->parent());

  for (auto& observer : observers_)
    observer.OnWillReorderNotesNode(this, parent);

  if (ordered_nodes.size() > 1) {
    std::map<const Notes_Node*, size_t> order;
    for (size_t i = 0; i < ordered_nodes.size(); ++i)
      order[ordered_nodes[i]] = i;

    std::vector<std::unique_ptr<Notes_Node>> new_children(ordered_nodes.size());
    Notes_Node* mutable_parent = AsMutable(parent);
    for (auto& child : mutable_parent->children()) {
      size_t new_location = order[child.get()];
      new_children[new_location] = std::move(child);
    }
    mutable_parent->children().swap(new_children);
  }

  if (store_.get())
    store_->ScheduleSave();

  for (auto& observer : observers_)
    observer.NotesNodeChildrenReordered(this, parent);
}

}  // namespace vivaldi
