// Copyright (c) 2013-2014 Vivaldi Technologies AS. All rights reserved

#include "notes/notes_model.h"

#include "base/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"

#include "notes/notesnode.h"
#include "notes/notes_storage.h"

namespace Vivaldi {
  // Helper to get a mutable bookmark node.
Notes_Node* AsMutable(const Notes_Node *node) {
  return const_cast<Notes_Node*>(node);
}

Notes_Model::Notes_Model(Profile* profile) :
  profile_(profile),
  root_(0),
  loaded_(false),
  loaded_signal_(true, false),
  current_index_(0) // root_ has 0
{
  root_.SetType(Notes_Node::FOLDER);
  if (!profile_) {
    // Profile is null during testing.
    DoneLoading(CreateLoadDetails());
  }
}

Notes_Model::~Notes_Model()
{
  FOR_EACH_OBSERVER(NotesModelObserver, observers_,
    NotesModelBeingDeleted(this));

  if (store_.get()) {
    // The store maintains a reference back to us. We need to tell it we're gone
    // so that it doesn't try and invoke a method back on us again.
    store_->NotesModelDeleted();
  }
}

void Notes_Model::Shutdown()
{
  if (loaded_)
    return;

  // See comment in HistoryService::ShutdownOnUIThread where this is invoked for
  // details. It is also called when the BookmarkModel is deleted.
  loaded_signal_.Signal();
}

NotesLoadDetails* Notes_Model::CreateLoadDetails()
{
  return new NotesLoadDetails(&root_);
}

void Notes_Model::Load(const scoped_refptr<base::SequencedTaskRunner>& task_runner)
{
  if (store_.get()) {
    // If the store is non-null, it means Load was already invoked. Load should
    // only be invoked once.
    NOTREACHED();
    return;
  }

  // Load the notes. NotesStorage notifies us when done.
  store_ = new NotesStorage(profile_, this, task_runner.get());
  store_->LoadNotes(CreateLoadDetails());
}

void Notes_Model::DoneLoading(NotesLoadDetails* details_delete_me)
{
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

  loaded_signal_.Signal();

  // Notify our direct observers.
  FOR_EACH_OBSERVER(NotesModelObserver, observers_,
    Loaded(this, false));
}

void Notes_Model::BlockTillLoaded()
{
  loaded_signal_.Wait();
}

void Notes_Model::Observe(int type,
  const content::NotificationSource& source,
  const content::NotificationDetails& details)
{
}

bool Notes_Model::LoadNotes()
{
  return false;
}

bool Notes_Model::SaveNotes()
{
  if (store_.get())
  {
    store_->ScheduleSave();
    return true;
  }
  return false;
}


void Notes_Model::AddObserver(NotesModelObserver* observer)
{
  observers_.AddObserver(observer);
}

void Notes_Model::RemoveObserver(NotesModelObserver* observer)
{
  observers_.RemoveObserver(observer);
}

void Notes_Model::BeginExtensiveChanges()
{
  if (++extensive_changes_ == 1) {
    FOR_EACH_OBSERVER(NotesModelObserver, observers_,
      ExtensiveNotesChangesBeginning(this));
  }
}

void Notes_Model::EndExtensiveChanges()
{
  --extensive_changes_;
  DCHECK_GE(extensive_changes_, 0);
  if (extensive_changes_ == 0)
  {
    FOR_EACH_OBSERVER(NotesModelObserver, observers_,
      ExtensiveNotesChangesEnded(this));
  }
}

Notes_Node *Notes_Model::AddNode(Notes_Node *parent, int index, Notes_Node *node)
{
  if (!parent)
    parent = &root_;

  parent->Add(node, index);

  if (store_.get())
    store_->ScheduleSave();

  FOR_EACH_OBSERVER(NotesModelObserver, observers_,
    NotesNodeAdded(this, parent, index));

  return node;
}

Notes_Node *Notes_Model::AddNote(const Notes_Node *parent, int index, const base::string16 &subject, const GURL &url, const base::string16 &content)
{
  if (!loaded_)
    return NULL;

  int64 id = getNewIndex();

  Notes_Node *new_node = new Notes_Node(id);
  new_node->SetTitle(subject);
  new_node->SetContent(content);
  new_node->SetURL(url);

  return AddNode(AsMutable(parent), index, new_node);
}

Notes_Node *Notes_Model::AddFolder(const Notes_Node *parent, int index, const base::string16 &name)
{
  if (!loaded_)
    return NULL;

  int64 id = getNewIndex();
  Notes_Node *new_node = new Notes_Node(id);
  new_node->SetTitle(name);
  new_node->SetType(Notes_Node::FOLDER);

  return AddNode(AsMutable(parent), index, new_node);
}

const Notes_Node* GetNodeByID(const Notes_Node* node, int64 id) {
  if (node->id() == id)
    return node;

  for (int i = 0, child_count = node->child_count(); i < child_count; ++i) {
    const Notes_Node* result = GetNodeByID(node->GetChild(i), id);
    if (result)
      return result;
  }
  return NULL;
}

bool Notes_Model::Remove(Notes_Node* parent, int index) {
  if (!parent)
    return false;
  Notes_Node* node = parent->GetChild(index);
  Notes_Node* deadnode = parent->Remove(node);
  delete deadnode;
  if (store_.get())
    store_->ScheduleSave();
  return true;
}


bool Notes_Model::Move(const Notes_Node* node,
  const Notes_Node* new_parent,
  int index) {
  DCHECK(new_parent->HasAncestor(node));
  if (new_parent->HasAncestor(node)) {
    // Can't make an ancestor of the node be a child of the node.
    return false;
  }

  const Notes_Node* old_parent = node->parent();
  int old_index = old_parent->GetIndexOf(node);

  if (old_parent == new_parent &&
    (index == old_index || index == old_index + 1)) {
    // Node is already in this position, nothing to do.
    return false;
  }

  if (old_parent == new_parent && index > old_index)
    index--;

  Notes_Node* mutable_new_parent = AsMutable(new_parent);
  mutable_new_parent->Add(AsMutable(node), index);

  if (store_.get())
    store_->ScheduleSave();

  return true;

}

} // namespace Vivaldi
