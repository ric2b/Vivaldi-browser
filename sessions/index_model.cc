// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#include "sessions/index_model.h"

#include "app/vivaldi_version_info.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/uuid.h"
#include "browser/sessions/vivaldi_session_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "sessions/index_node.h"
#include "sessions/index_storage.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace sessions {

// Helper to get a mutable node.
Index_Node* AsMutable(const Index_Node* node) {
  return const_cast<Index_Node*>(node);
}

Index_Model::Index_Model(content::BrowserContext* context)
    : context_(context),
      root_(Index_Node::root_node_guid(), Index_Node::root_node_id()) {}

Index_Model::~Index_Model() {
  for (auto& observer : observers_)
    observer.IndexModelBeingDeleted(this);
  if (store_) {
    store_->OnModelWillBeDeleted();
  }
}

std::unique_ptr<IndexLoadDetails> Index_Model::CreateLoadDetails() {
  Index_Node* items = new Index_Node(Index_Node::items_node_guid(),
                                     Index_Node::items_node_id(),
                                     Index_Node::kFolder);
  Index_Node* backup = new Index_Node(Index_Node::backup_node_guid(),
                                      Index_Node::backup_node_id(),
                                      Index_Node::kNode);
  Index_Node* persistent = new Index_Node(Index_Node::persistent_node_guid(),
                                      Index_Node::persistent_node_id(),
                                      Index_Node::kNode);
  return base::WrapUnique(new IndexLoadDetails(items, backup, persistent));
}

void Index_Model::Load() {
  // Make a backend task runner to avoid file access in the IO-thread
  const scoped_refptr<base::SequencedTaskRunner>& task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({
          base::MayBlock(),
          base::TaskPriority::USER_VISIBLE,
          base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
      });
  store_.reset(new IndexStorage(context_, this, task_runner.get()));
  store_->Load(CreateLoadDetails());
}

void Index_Model::LoadFinished(std::unique_ptr<IndexLoadDetails> details) {
  DCHECK(details);

  // Add new content
  std::unique_ptr<Index_Node> items_node(details->release_items_node());
  items_node_ = items_node.get();
  root_.Add(std::move(items_node));

  if (!details->persistent_node()->filename().empty()) {
    std::unique_ptr<Index_Node> persistent_node(
        details->release_persistent_node());
    persistent_node_ = persistent_node.get();
    root_.Add(std::move(persistent_node));
  }

  if (!details->backup_node()->filename().empty()) {
    // We have timed backup information in the model. This is a sign the prev
    // session crashed as a backup file and the corresponding entry in the model
    // is deleted on a normal shutdown. Note that AutoSaveFromBackup will just
    // delete the file and entry if auto saving is turned off (that prefs value
    // controls the backup code as well).
    std::unique_ptr<Index_Node> backup_node(details->release_backup_node());
    backup_node_ = backup_node.get();
    root_.Add(std::move(backup_node));
    // Note. This will modify the model content.
    int error_code = AutoSaveFromBackup(context_);
    if (error_code) {
      LOG(ERROR) << "Session Model: Failed to restore from backup"
                 << error_code;
    }
  }

  Profile* profile = Profile::FromBrowserContext(context_);
  int save_version = profile->GetPrefs()->GetInteger(
    vivaldiprefs::kSessionsSaveVersion);
  if (save_version == 0) {
    // Move all existing auto saved nodes to trash if auto save policy has changed.
    int error_code = MoveAutoSaveNodesToTrash(context_);
    if (error_code) {
      LOG(ERROR) << "Session Model: Failed to move auto saved elements"
                   << error_code;
    }
    profile->GetPrefs()->SetInteger(vivaldiprefs::kSessionsSaveVersion, 1);
  }

  loaded_ = true;
  loading_failed_ = details->get_loading_failed();

  // Uncomment if debugging of parsed data is needed.
  //root_.DumpTree();

  if (details->get_loaded_from_filescan()) {
    Save();
  }
  for (auto& observer : observers_)
    observer.IndexModelLoaded(this);
}

bool Index_Model::Save() {
  if (store_.get()) {
    store_->ScheduleSave();
    return true;
  }
  return false;
}

bool Index_Model::Move(const Index_Node* node, const Index_Node* parent,
                       size_t index) {
  if (!node->parent() || !IsValidIndex(parent, index)) {
    LOG(ERROR) << "Session model. Can not move node. Parent missing or invalid index.";
    return false;
  }
  DCHECK(!parent->HasAncestor(node));
  if (parent->HasAncestor(node)) {
    LOG(ERROR) << "Session model. Can not move node. Will become a child of itself.";
    return false;
  }

  // We can only move an item into a container if it originally came from it.
  // This limits it to moving from trash to the container.
  if (!node->container_guid().empty()) {
    if (node->container_guid() != parent->guid() &&
        !parent->is_trash_folder()) {
      LOG(ERROR) << "Session model. Can not move node.";
      return false;
    }
  }

  int64_t id = node->id();
  size_t old_index = node->parent()->GetIndexOf(node).value();

  if (node->parent() == parent &&
      (index == old_index || index == old_index + 1)) {
    // Node is already in this position, nothing to do.
    return false;
  }

  if (node->parent() == parent && index > old_index)
    index--;

  Index_Node* mutable_old_parent = AsMutable(node->parent());
  std::unique_ptr<Index_Node> owned_node = mutable_old_parent->Remove(old_index);
  Index_Node* mutable_new_parent = AsMutable(parent);
  mutable_new_parent->Add(std::move(owned_node), index);

  Save();

  if (loaded_) {
    for (auto& observer : observers_)
      observer.IndexModelNodeMoved(this, id, parent->id(), index);
  }

  return true;
}

Index_Node* Index_Model::Add(std::unique_ptr<Index_Node> node,
                             Index_Node* parent, size_t index,
                             std::string owner) {
  Index_Node* node_ptr = node.get();
  parent->Add(std::move(node), index);

  // Save quick access to backup. This node can be invalidated in Remove().
  if (node_ptr->id() == Index_Node::backup_node_id()) {
    backup_node_ = node_ptr;
  } else if (node_ptr->id() == Index_Node::persistent_node_id()) {
    persistent_node_ = node_ptr;
  }

  Save();

  if (loaded_) {
    for (auto& observer : observers_)
      observer.IndexModelNodeAdded(this, node_ptr, parent->id(), index, owner);
  }

  return node_ptr;
}

bool Index_Model::SetTitle(Index_Node* node,
                           const std::u16string& title) {
  if (node->GetTitle() == title) {
    return true;
  }

  node->SetTitle(title);

  Save();

  if (loaded_) {
    std::string name = base::UTF16ToUTF8(node->GetTitle());
    for (auto& observer : observers_)
      observer.IndexModelNodeChanged(this, node);
  }

  return true;
}

bool Index_Model::Change(Index_Node* node, Index_Node* from) {
  node->Copy(from);

  Save();

  if (loaded_) {
    for (auto& observer : observers_)
      observer.IndexModelNodeChanged(this, node);
  }

  return true;
}

bool Index_Model::Swap(Index_Node* node_a, Index_Node* node_b) {
  std::unique_ptr<Index_Node> tmp = std::make_unique<Index_Node>("", -1);
  tmp->Copy(node_a);
  node_a->Copy(node_b);
  node_b->Copy(tmp.get());

  Save();

  if (loaded_) {
    for (auto& observer : observers_) {
      observer.IndexModelNodeChanged(this, node_a);
      observer.IndexModelNodeChanged(this, node_b);
    }
  }

  return true;
}

bool Index_Model::Remove(Index_Node* node) {
  int64_t id = node->id();
  Index_Node* parent = node->parent();
  bool was_container = parent->is_container();
  int index = parent->GetIndexOf(node).value();
  parent->Remove(index);

  if (id == Index_Node::kBackupNodeId) {
    backup_node_ = nullptr;
  } else if (id == Index_Node::kPersistentNodeId) {
    persistent_node_ = nullptr;
  }

  Save();

  if (loaded_) {
    for (auto& observer : observers_)
      observer.IndexModelNodeRemoved(this, id);
    // The container state depends on the number of children. If none are
    // left we want to notify the parent has changes as well. Failing to do so
    // can be seen as a wrongly shown child indicator in the UI when updating
    // autosave data.
    if (was_container && !parent->is_container()) {
      for (auto& observer : observers_)
        observer.IndexModelNodeChanged(this, parent);
    }
  }

  return true;
}

bool Index_Model::IsValidIndex(const Index_Node* parent, size_t index) {
  return (parent &&
         (index >= 0 && (index <= parent->children().size())));
}

bool Index_Model::IsTrashed(Index_Node* node) {
  return node->parent() && node->parent()->id() == Index_Node::trash_node_id();
}

void Index_Model::AddObserver(IndexModelObserver* observer) {
  observers_.AddObserver(observer);
}

void Index_Model::RemoveObserver(IndexModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace sessions
