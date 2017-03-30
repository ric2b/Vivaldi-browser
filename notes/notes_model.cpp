// Copyright (c) 2013-2014 Vivaldi Technologies AS. All rights reserved

#include "notes/notes_model.h"

#include "app/vivaldi_resources.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "importer/imported_notes_entry.h"
#include "notes/notesnode.h"
#include "notes/notes_storage.h"
#include "ui/base/l10n/l10n_util.h"


namespace vivaldi {
  // Helper to get a mutable bookmark node.
Notes_Node* AsMutable(const Notes_Node *node) {
  return const_cast<Notes_Node*>(node);
}

Notes_Model::Notes_Model(Profile *profile)
    : profile_(profile), root_(0), trash_node_(nullptr), loaded_(false),
      loaded_signal_(true, false), extensive_changes_(0),
      current_index_(0) {  // root_ has 0
  root_.SetType(Notes_Node::FOLDER);
  if (!profile_) {
    // Profile is null during testing.
    DoneLoading(CreateLoadDetails());
  }
}

Notes_Model::~Notes_Model() {
  FOR_EACH_OBSERVER(NotesModelObserver, observers_,
                    NotesModelBeingDeleted(this));

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
  // details. It is also called when the BookmarkModel is deleted.
  loaded_signal_.Signal();
}

NotesLoadDetails *Notes_Model::CreateLoadDetails() {
  return new NotesLoadDetails(&root_);
}

void Notes_Model::Load(
    const scoped_refptr<base::SequencedTaskRunner> &task_runner) {
  if (store_.get()) {
    // If the store is non-null, it means Load was already invoked. Load should
    // only be invoked once.
    NOTREACHED();
    return;
  }

  // Load the notes. NotesStorage notifies us when done.
  store_.reset(new NotesStorage(profile_, this, task_runner.get()));
  store_->LoadNotes(make_scoped_ptr(CreateLoadDetails()));
}

void Notes_Model::DoneLoading(NotesLoadDetails *details_delete_me) {
  DCHECK(details_delete_me);
  scoped_ptr<NotesLoadDetails> details(details_delete_me);
  details_delete_me->release_notes_node();
  if (loaded_) {
    // We should only ever be loaded once.
    NOTREACHED();
    return;
  }

  current_index_ = details->highest_id();

  loaded_ = true;

  // Check if we have trash and add it if we don't.
  trash_node_ = GetTrashNode();

  loaded_signal_.Signal();

  // Notify our direct observers.
  FOR_EACH_OBSERVER(NotesModelObserver, observers_, Loaded(this, false));
}

void Notes_Model::BlockTillLoaded() { loaded_signal_.Wait(); }

void Notes_Model::Observe(int type, const content::NotificationSource &source,
                          const content::NotificationDetails &details) {}

bool Notes_Model::LoadNotes() { return false; }

bool Notes_Model::SaveNotes() {
  if (store_.get()) {
    store_->ScheduleSave();
    return true;
  }
  return false;
}

void Notes_Model::AddObserver(NotesModelObserver *observer) {
  observers_.AddObserver(observer);
}

void Notes_Model::RemoveObserver(NotesModelObserver *observer) {
  observers_.RemoveObserver(observer);
}

void Notes_Model::BeginExtensiveChanges() {
  if (++extensive_changes_ == 1) {
    FOR_EACH_OBSERVER(NotesModelObserver, observers_,
                      ExtensiveNotesChangesBeginning(this));
  }
}

void Notes_Model::EndExtensiveChanges() {
  --extensive_changes_;
  DCHECK_GE(extensive_changes_, 0);
  if (extensive_changes_ == 0) {
    FOR_EACH_OBSERVER(NotesModelObserver, observers_,
                      ExtensiveNotesChangesEnded(this));
  }
}

Notes_Node *Notes_Model::AddNode(Notes_Node *parent, int index,
                                 Notes_Node *node) {
  if (!parent)
    parent = &root_;

  parent->Add(node, index);

  if (store_.get())
    store_->ScheduleSave();

  FOR_EACH_OBSERVER(NotesModelObserver, observers_,
                    NotesNodeAdded(this, parent, index));

  return node;
}

Notes_Node *Notes_Model::AddNote(const Notes_Node *parent, int index,
                                 const base::string16 &subject, const GURL &url,
                                 const base::string16 &content) {
  if (!loaded_)
    return NULL;

  int64_t id = GetNewIndex();

  Notes_Node *new_node = new Notes_Node(id);
  new_node->SetTitle(subject);
  new_node->SetContent(content);
  new_node->SetURL(url);

  return AddNode(AsMutable(parent), index, new_node);
}

Notes_Node *Notes_Model::AddNote(const Notes_Node *parent, int index,
                                 bool is_folder,
                                 const ImportedNotesEntry &note) {
  if (!loaded_)
    return NULL;

  int64_t id = GetNewIndex();

  Notes_Node *new_node = new Notes_Node(id);
  new_node->SetTitle(note.title);
  new_node->SetCreationTime(note.creation_time);

  if (is_folder) {
    new_node->SetType(Notes_Node::FOLDER);
  } else {
    new_node->SetURL(note.url);
    new_node->SetContent(note.content);
  }
  return AddNode(AsMutable(parent), index, new_node);
}

Notes_Node *Notes_Model::AddFolder(const Notes_Node *parent, int index,
                                   const base::string16 &name) {
  if (!loaded_)
    return NULL;

  int64_t id = GetNewIndex();
  Notes_Node *new_node = new Notes_Node(id);
  new_node->SetTitle(name);
  new_node->SetType(Notes_Node::FOLDER);

  return AddNode(AsMutable(parent), index, new_node);
}

const Notes_Node *GetNodeByID(const Notes_Node *node, int64_t id) {
  if (node->id() == id)
    return node;

  for (int i = 0, child_count = node->child_count(); i < child_count; ++i) {
    const Notes_Node *result = GetNodeByID(node->GetChild(i), id);
    if (result)
      return result;
  }
  return NULL;
}

bool Notes_Model::Remove(Notes_Node *parent, int index) {
  if (!parent)
    return false;
  Notes_Node *node = parent->GetChild(index);
  Notes_Node *deadnode = parent->Remove(node);
  delete deadnode;
  if (store_.get())
    store_->ScheduleSave();
  return true;
}

bool Notes_Model::Move(const Notes_Node *node, const Notes_Node *new_parent,
                       int index) {
  if (new_parent->HasAncestor(node)) {
    // Can't make an ancestor of the node be a child of the node.
    NOTREACHED();
    return false;
  }

  const Notes_Node *old_parent = node->parent();
  int old_index = old_parent->GetIndexOf(node);

  if (old_parent == new_parent &&
      (index == old_index || index == old_index + 1)) {
    // Node is already in this position, nothing to do.
    return false;
  }

  if (old_parent == new_parent && index > old_index)
    index--;

  Notes_Node *mutable_new_parent = AsMutable(new_parent);
  mutable_new_parent->Add(AsMutable(node), index);

  if (store_.get())
    store_->ScheduleSave();

  return true;
}

Notes_Node* Notes_Model::GetTrashNode() {
  Notes_Node* root_node = root();
  for (int i = 0; i < root_node->child_count(); ++i) {
    Notes_Node* child = root_node->GetChild(i);
    if (child->is_trash()) {
      // Move it to the end of the list.
      child = root_node->Remove(child);
      AddNode(root_node, root_node->child_count(), child);
      return child;
    }
  }
  Notes_Node* trash = new Notes_Node(GetNewIndex());
  if (trash) {
    trash->SetType(Notes_Node::TRASH);
    trash->SetTitle(
        l10n_util::GetStringUTF16(IDS_NOTES_TRASH_FOLDER_NAME));
    AddNode(root_node, root_node->child_count(), trash);
  }
  return trash;
}

}  // namespace vivaldi
