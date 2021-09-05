// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "menus/menu_model.h"

#include "app/vivaldi_version_info.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "menus/menu_node.h"
#include "menus/menu_storage.h"

namespace menus {

// Helper to get a mutable node.
Menu_Node* AsMutable(const Menu_Node* node) {
  return const_cast<Menu_Node*>(node);
}

Menu_Model::Menu_Model(content::BrowserContext* context)
    : context_(context) {}

Menu_Model::~Menu_Model() {
  for (auto& observer : observers_)
    observer.MenuModelBeingDeleted(this);
  if (store_) {
    store_->OnModelWillBeDeleted();
  }
}

std::unique_ptr<MenuLoadDetails> Menu_Model::CreateLoadDetails(
    const std::string& menu) {
  Menu_Node* root = new Menu_Node();
  Menu_Control* control = new Menu_Control();
  control->version = ::vivaldi::GetVivaldiVersionString();
  return base::WrapUnique(new MenuLoadDetails(root, control, menu, loaded_));
}

std::unique_ptr<MenuLoadDetails> Menu_Model::CreateLoadDetails(int64_t id) {
  Menu_Node* root = new Menu_Node();
  Menu_Control* control = new Menu_Control();
  control->version = ::vivaldi::GetVivaldiVersionString();
  return base::WrapUnique(new MenuLoadDetails(root, control, id, loaded_));
}

void Menu_Model::Load(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
  store_.reset(new MenuStorage(context_, this, task_runner.get()));
  store_->Load(CreateLoadDetails(""));
}

bool Menu_Model::Save() {
  if (store_.get()) {
    store_->ScheduleSave();
    return true;
  }
  return false;
}

void Menu_Model::LoadFinished(std::unique_ptr<MenuLoadDetails> details) {
  DCHECK(details);

  if (loaded_) {
    // Reset exisiting content
    if (!details->menu().empty()) {
      // Replace all
      // Remove old content
      while (root_.children().size() > 0) {
        root_.Remove(0);
      }
      // Add new content
      Menu_Node* root_node = details->root_node();
      while (root_node->children().size() > 0) {
        std::unique_ptr<Menu_Node> node = root_node->Remove(0);
        root_.Add(std::move(node));
      }
      control_ = details->release_control();

      Save();

      int id = -1;
      Menu_Node* menu = GetMenu(details->menu());
      if (menu && menu->children().size() > 0) {
        id = menu->children()[0]->id();
      }

      for (auto& observer : observers_)
        observer.MenuModelChanged(this, id, details->menu());
    } else if (details->id() >= 0) {
      // Replace node specified by the id
      Menu_Node* target = root_.GetById(details->id());
      Menu_Node* target_parent = target ? target->parent() : nullptr;
      const Menu_Node* target_menu = target ? target->GetMenu() : nullptr;
      if (target && target_parent && target_menu) {
        // Loaded is the new tree we have just read from disk. Look up the same
        // menu and folder in it. This works as long the target is a folder. We
        // can have multiple nodes with the same action, but folders will always
        // have a unique action in a menu.
        Menu_Node* loaded_menu = details->root_node()->GetByAction(
            target_menu->action());
        Menu_Node* loaded = loaded_menu ?
            loaded_menu->GetByAction(target->action()) : nullptr;
        if (loaded && loaded->parent()) {
          // Remove old content
           Menu_Node* target_parent = target->parent();
          int target_index = target_parent->GetIndexOf(target);
          target_parent->Remove(target_index);
          // Add new content
          Menu_Node* loaded_parent = loaded->parent();
          int loaded_index = loaded_parent->GetIndexOf(loaded);
          std::unique_ptr<Menu_Node> node = loaded_parent->Remove(loaded_index);
          // In case a node or more were tagged as deleted in the old tree, that
          // must now be removed now as a part of reseting the content.
          ClearDeleted(node.get(), true);
          // Ensure there are no guid duplications. This can happen if we move
          // a bundled node out of its folder (we do not change its guid then)
          // and next reset that folder. Any duplications will be made into
          // custom nodes with a new guid.
          RemoveGuidDuplication(node.get());
          target_parent->Add(std::move(node), target_index);

          Save();

          // The ids of the reloaded elements have changed. Send the id of the
          // top element so that JS can use this to select the element.
          for (auto& observer : observers_)
            observer.MenuModelChanged(this, loaded->id(),
                target_menu->action());
        }
      }
    }
  } else {
    // Install content for the first time.
    // Move all menus to make them children of root_
    Menu_Node* root_node = details->root_node();
    while (root_node->children().size() > 0) {
      std::unique_ptr<Menu_Node> node = root_node->Remove(0);
      root_.Add(std::move(node));
    }
    control_ = details->release_control();
    loaded_ = true;
    if (details->has_upgraded()) {
      Save();
    }
    for (auto& observer : observers_)
      observer.MenuModelLoaded(this);
  }
}

bool Menu_Model::SetMenu(std::unique_ptr<Menu_Node> node) {
  if (!node->is_menu()) {
    NOTREACHED();
    return false;
  }
  Menu_Node* parent = node->parent();
  if (!parent) {
    NOTREACHED();
    return false;
  }
  std::string named_menu = node->action();
  int index = parent->GetIndexOf(node.get());
  parent->Remove(index);
  parent->Add(std::move(node), index);

  Save();

  for (auto& observer : observers_)
    observer.MenuModelChanged(this, -1, named_menu);

  return true;
}

bool Menu_Model::Move(const Menu_Node* node, const Menu_Node* new_parent,
                      size_t index) {
  if (!loaded_ || !node || !IsValidIndex(new_parent, index)) {
    NOTREACHED();
    return false;
  }

  DCHECK(!new_parent->HasAncestor(node));
  if (new_parent->HasAncestor(node)) {
    // Can't make an ancestor of the node be a child of the node.
    NOTREACHED();
    return false;
  }

  const Menu_Node* menu = new_parent->GetMenu();
  if (!menu) {
    NOTREACHED();
    return false;
  }

  const Menu_Node* old_parent = node->parent();
  size_t old_index = old_parent->GetIndexOf(node);

  if (old_parent == new_parent &&
      (index == old_index || index == old_index + 1)) {
    // Node is already in this position, nothing to do.
    return false;
  }

  if (old_parent == new_parent && index > old_index)
    index--;

  Menu_Node* mutable_old_parent = AsMutable(old_parent);
  std::unique_ptr<Menu_Node> owned_node = mutable_old_parent->Remove(old_index);
  Menu_Node* mutable_new_parent = AsMutable(new_parent);
  mutable_new_parent->Add(std::move(owned_node), index);

  Save();

  for (auto& observer : observers_)
    observer.MenuModelChanged(this, -1, menu->action());

  return true;
}

Menu_Node* Menu_Model::Add(std::unique_ptr<Menu_Node> node, Menu_Node* parent,
                           size_t index) {
  const Menu_Node* menu = parent->GetMenu();
  if (!menu) {
    NOTREACHED();
    return nullptr;
  }

  Menu_Node* node_ptr = node.get();
  parent->Add(std::move(node), index);

  Save();

  for (auto& observer : observers_)
    observer.MenuModelChanged(this, -1, menu->action());

  return node_ptr;
}

bool Menu_Model::SetTitle(Menu_Node* node, const base::string16& title) {
  if (node->GetTitle() == title) {
    return true;
  }

  const Menu_Node* menu = node->GetMenu();
  if (!menu) {
    NOTREACHED();
    return false;
  }

  SetDeleted(node, false);

  node->SetTitle(title);
  node->SetHasCustomTitle(true);

  Save();

  for (auto& observer : observers_)
    observer.MenuModelChanged(this, -1, menu->action());

  return true;
}

bool Menu_Model::SetContainerMode(Menu_Node* node, const std::string& mode) {
  if (!node->is_container()) {
    NOTREACHED();
    return false;
  }

  if (node->containerMode() == mode) {
    return true;
  }

  const Menu_Node* menu = node->GetMenu();
  if (!menu) {
    NOTREACHED();
    return false;
  }

  SetDeleted(node, false);

  node->SetContainerMode(mode);

  Save();

  for (auto& observer : observers_)
    observer.MenuModelChanged(this, -1, menu->action());

  return true;
}

bool Menu_Model::SetContainerEdge(Menu_Node* node, const std::string& edge) {
  if (!node->is_container()) {
    NOTREACHED();
    return false;
  }

  if (node->containerEdge() == edge) {
    return true;
  }

  const Menu_Node* menu = node->GetMenu();
  if (!menu) {
    NOTREACHED();
    return false;
  }

  SetDeleted(node, false);

  node->SetContainerEdge(edge);

  Save();

  for (auto& observer : observers_)
    observer.MenuModelChanged(this, -1, menu->action());

  return true;
}

bool Menu_Model::Remove(Menu_Node* node) {
  const Menu_Node* menu = node->GetMenu();
  if (!menu) {
    NOTREACHED();
    return false;
  }

  SetDeleted(node, true);

  Menu_Node* parent = node->parent();
  int index = parent->GetIndexOf(node);
  parent->Remove(index);

  Save();

  for (auto& observer : observers_)
    observer.MenuModelChanged(this, -1, menu->action());

  return true;
}

bool Menu_Model::Reset(const Menu_Node* node) {
  if (store_.get()) {
    store_->Load(CreateLoadDetails(node->id()));
    return true;
  }
  return false;
}

bool Menu_Model::Reset(const std::string& menu) {
  if (store_.get()) {
    store_->Load(CreateLoadDetails(menu));
    return true;
  }
  return false;
}

void Menu_Model::SetDeleted(Menu_Node* node, bool include_children) {
  if (node->origin() == Menu_Node::BUNDLE) {
    control_->deleted.push_back(node->guid());
    // SetDeleted() is called for all nodes that are modified or deleted.
    // We can restore a modified bundled folder (and content) to its default
    // state but that action is not possible when the origin is Menu_Node::USER.
    // We use Menu_Node::DELETED_BUNDLE when we change the guid to allow this.
    node->SetOrigin(Menu_Node::DELETED_BUNDLE);
    node->SetGuid(base::GenerateGUID());
  }
  if (node->is_folder() && include_children) {
    for (auto& child : node->children()) {
      SetDeleted(child.get(), include_children);
    }
  }
}

void Menu_Model::ClearDeleted(const Menu_Node* node, bool include_children) {
  for (unsigned long i = 0; i < control_->deleted.size(); i++) {
    if (control_->deleted[i] == node->guid()) {
      control_->deleted.erase(control_->deleted.begin() + i);
      break;
    }
  }
  if (include_children) {
    for (auto& child : node->children()) {
      ClearDeleted(child.get(), include_children);
    }
  }
}

void Menu_Model::RemoveGuidDuplication(const Menu_Node* node) {
  Menu_Node* n = root_.GetByGuid(node->guid());
  if (n) {
    n->SetOrigin(Menu_Node::USER);
    n->SetGuid(base::GenerateGUID());
  }
  if (node->is_folder()) {
    for (auto& child : node->children()) {
      RemoveGuidDuplication(child.get());
    }
  }
}

bool Menu_Model::IsValidIndex(const Menu_Node* parent, size_t index) {
  return (parent && (parent->is_folder() || parent->is_menu())) &&
          (index >= 0 && (index <= parent->children().size()));
}

void Menu_Model::AddObserver(MenuModelObserver* observer) {
  observers_.AddObserver(observer);
}

void Menu_Model::RemoveObserver(MenuModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

Menu_Node* Menu_Model::GetMenu(const std::string& named_menu) {
  // All menus are located as children of root (only that level).
  for (const auto& child : root_.children()) {
    if (child->action() == named_menu) {
      return child.get();
    }
  }
  return nullptr;
}

}  // namespace menus
