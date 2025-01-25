// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "menus/menu_model.h"

#include "app/vivaldi_version_info.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/uuid.h"
#include "menus/menu_node.h"
#include "menus/menu_storage.h"

namespace menus {

// Helper to get a mutable node.
Menu_Node* AsMutable(const Menu_Node* node) {
  return const_cast<Menu_Node*>(node);
}

Menu_Model::Menu_Model(content::BrowserContext* context, Mode mode)
    : context_(context),
      mode_(mode),
      root_(Menu_Node::root_node_guid(), Menu_Node::root_node_id()) {}

Menu_Model::~Menu_Model() {
  for (auto& observer : observers_)
    observer.MenuModelBeingDeleted(this);
  if (store_) {
    store_->OnModelWillBeDeleted();
  }
}

std::unique_ptr<MenuLoadDetails> Menu_Model::CreateLoadDetails(
    const std::string& menu, bool is_reset) {
  Menu_Node* mainmenu = new Menu_Node(Menu_Node::mainmenu_node_guid(),
                                      Menu_Node::mainmenu_node_id());
  Menu_Control* control = new Menu_Control();
  control->version = ::vivaldi::GetVivaldiVersionString();
  MenuLoadDetails::Mode mode = is_reset
    ? (menu.empty() ? MenuLoadDetails::kResetAll : MenuLoadDetails::kResetByName)
    : MenuLoadDetails::kLoad;
  return base::WrapUnique(
      new MenuLoadDetails(mainmenu, control, menu, loaded_ || is_reset, mode));
}

std::unique_ptr<MenuLoadDetails> Menu_Model::CreateLoadDetails(int64_t id) {
  Menu_Node* mainmenu = new Menu_Node(Menu_Node::mainmenu_node_guid(),
                                      Menu_Node::mainmenu_node_id());
  Menu_Control* control = new Menu_Control();
  control->version = ::vivaldi::GetVivaldiVersionString();
  MenuLoadDetails::Mode mode = MenuLoadDetails::kResetById;
  return base::WrapUnique(
      new MenuLoadDetails(mainmenu, control, id, true, mode));
}

void Menu_Model::Load(bool is_reset) {
  // Make a backend task runner to avoid file access in the IO-thread
  const scoped_refptr<base::SequencedTaskRunner>& task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({
          base::MayBlock(),
          base::TaskPriority::USER_VISIBLE,
          base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
      });
  store_.reset(new MenuStorage(context_, this, task_runner.get()));
  store_->Load(CreateLoadDetails("", is_reset));
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
  if (details->mode() == MenuLoadDetails::kLoad) {
    // Install content for the first time.
    std::unique_ptr<Menu_Node> mainmenu_node(details->release_mainmenu_node());
    mainmenu_node_ = mainmenu_node.get();
    root_.Add(std::move(mainmenu_node));

    control_ = details->release_control();
    loaded_ = true;
    // If model is loaded on demand the first operation can be a reset action.
    if (details->has_upgraded()) {
      Save();
    }
    for (auto& observer : observers_)
      observer.MenuModelLoaded(this);
  } else {
    if (details->mode() == MenuLoadDetails::kResetAll) {
      // Reset entire file (all menus). If model is loaded on demand the first
      // operation can be a reset action and there will be nothing to remove.
      int index = -1;
      for (const auto& node : root_.children()) {
        if (node->id() == details->mainmenu_node()->id()) {
          index = root_.GetIndexOf(node.get()).value();
          break;
        }
      }
      if (index != -1) {
        root_.Remove(index);
      }
      std::unique_ptr<Menu_Node> mainmenu_node(details->release_mainmenu_node());
      mainmenu_node_ = mainmenu_node.get();
      root_.Add(std::move(mainmenu_node));
      control_ = details->release_control();
      Save();
    } else if (details->mode() == MenuLoadDetails::kResetByName) {
      // Reset named menu entry. Works even if old is missing. A menu is in this
      // case a full menu bar, the vivaldi menu, the tab context menu etc.
      for (const auto& node : root_.children()) {
        // All nodes we deal with is within the mainmenu_node
        // TODO: The name main menu node is kind of misleading since we also
        // store context menus under it.
        if (node->id() == details->mainmenu_node()->id()) {
          Menu_Node* old_menu = mainmenu_node()->GetMenuByResourceName(
            details->menu());
          Menu_Node* new_menu = details->mainmenu_node()->GetMenuByResourceName(
            details->menu());
          if (old_menu) {
            Remove(old_menu);
          }
          if (new_menu) {
            std::optional<size_t> index =
                details->mainmenu_node()->GetIndexOf(
              new_menu);
            if (index.has_value()) {
              std::unique_ptr<Menu_Node> new_menu_node =
                  details->mainmenu_node()->Remove(index.value());
              Add(std::move(new_menu_node), mainmenu_node(), 0);
            }
          }
          break;
        }
      }

      Save();

      int id = -1;
      Menu_Node* menu = GetMenuByResourceName(details->menu());
      if (menu && menu->children().size() > 0) {
        id = menu->children()[0]->id();
      }
      for (auto& observer : observers_)
        observer.MenuModelChanged(this, id, details->menu());
    } else if (details->mode() == MenuLoadDetails::kResetById) {
      // Replace node specified by id. The id refers to an exising id in the
      // current installed model. Unlike guids every time we create a new item
      // the id steps. So we can not compare ids in current model and the newly
      // loaded model in details.
      // TODO: Use guid instead. We can fetch the guid in Reset().
      Menu_Node* target = root_.GetById(details->id());
      Menu_Node* target_parent = target ? target->parent() : nullptr;
      const Menu_Node* target_menu = target ? target->GetMenu() : nullptr;
      if (target && target_parent && target_menu) {
        // Loaded is the new tree we have just read from disk. Look up the same
        // menu and folder in it. This works as long the target is a folder. We
        // can have multiple nodes with the same action, but folders will always
        // have a unique action in a menu.
        Menu_Node* loaded_menu =
            details->mainmenu_node()->GetByAction(target_menu->action());
        Menu_Node* loaded =
            loaded_menu ? loaded_menu->GetByAction(target->action()) : nullptr;
        if (loaded && loaded->parent()) {
          // Remove old content
          target_parent = target->parent();
          int target_index = target_parent->GetIndexOf(target).value();
          // Note that this invalidates the target_menu.
          target_parent->Remove(target_index);
          // Add new content
          Menu_Node* loaded_parent = loaded->parent();
          int loaded_index = loaded_parent->GetIndexOf(loaded).value();
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
                                      loaded_menu->action());
        }
      }
    }
    bool all = details->mode() == MenuLoadDetails::kResetAll;
    for (auto& observer : observers_)
      observer.MenuModelReset(this, all);
  }
}


bool Menu_Model::Move(const Menu_Node* node,
                      const Menu_Node* new_parent,
                      size_t index) {
  if (!loaded_ || !node || !IsValidIndex(new_parent, index)) {
    NOTREACHED();
    //return false;
  }

  DCHECK(!new_parent->HasAncestor(node));
  if (new_parent->HasAncestor(node)) {
    // Can't make an ancestor of the node be a child of the node.
    NOTREACHED();
    //return false;
  }

  const Menu_Node* menu = new_parent->GetMenu();
  if (!menu) {
    NOTREACHED();
    //return false;
  }

  const Menu_Node* old_parent = node->parent();
  size_t old_index = old_parent->GetIndexOf(node).value();

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

Menu_Node* Menu_Model::Add(std::unique_ptr<Menu_Node> node,
                           Menu_Node* parent,
                           size_t index) {
  std::string action;
  // We can add a full menu to the node that holds all menus.
  if (parent->id() == Menu_Node::mainmenu_node_id()) {
    if (!node->is_menu()) {
      NOTREACHED();
      //return nullptr;
    }
    // Sanity check to prevent duplicate menus.
    for (const auto& menu_node : parent->children()) {
      if (menu_node->action() == node->action()) {
        NOTREACHED();
        //return nullptr;
      }
    }
    action = node->action();
  } else {
    // Or we can add a new element to an existing menu.
    const Menu_Node* menu = parent->GetMenu();
    if (!menu) {
      NOTREACHED();
      //return nullptr;
    }
    action = menu->action();
  }

  Menu_Node* node_ptr = node.get();
  parent->Add(std::move(node), index);

  Save();

  for (auto& observer : observers_)
    observer.MenuModelChanged(this, -1, action);

  return node_ptr;
}

bool Menu_Model::SetTitle(Menu_Node* node, const std::u16string& title) {
  if (node->GetTitle() == title) {
    return true;
  }

  const Menu_Node* menu = node->GetMenu();
  if (!menu) {
    NOTREACHED();
    //return false;
  }

  RemoveBundleTag(node, false);

  node->SetTitle(title);
  node->SetHasCustomTitle(true);

  Save();

  for (auto& observer : observers_)
    observer.MenuModelChanged(this, -1, menu->action());

  return true;
}

bool Menu_Model::SetParameter(Menu_Node* node, const std::string& parameter) {
  if (node->parameter() == parameter) {
    return true;
  }

  const Menu_Node* menu = node->GetMenu();
  if (!menu) {
    NOTREACHED();
    //return false;
  }

  RemoveBundleTag(node, false);

  node->SetParameter(parameter);

  Save();

  for (auto& observer : observers_)
    observer.MenuModelChanged(this, -1, menu->action());

  return true;
}

bool Menu_Model::SetShowShortcut(Menu_Node* node, bool show_shortcut) {
  const Menu_Node* menu = node->GetMenu();
  if (!menu) {
    NOTREACHED();
    //return false;
  }

  if (node->showShortcut().has_value() &&
      (*node->showShortcut() == show_shortcut)) {
    return true;
  }

  RemoveBundleTag(node, false);

  node->SetShowShortcut(show_shortcut);

  Save();

  for (auto& observer : observers_)
    observer.MenuModelChanged(this, -1, menu->action());

  return true;
}

bool Menu_Model::SetContainerMode(Menu_Node* node, const std::string& mode) {
  if (!node->is_container()) {
    NOTREACHED();
    //return false;
  }

  if (node->containerMode() == mode) {
    return true;
  }

  const Menu_Node* menu = node->GetMenu();
  if (!menu) {
    NOTREACHED();
    //return false;
  }

  RemoveBundleTag(node, false);

  node->SetContainerMode(mode);

  Save();

  for (auto& observer : observers_)
    observer.MenuModelChanged(this, -1, menu->action());

  return true;
}

bool Menu_Model::SetContainerEdge(Menu_Node* node, const std::string& edge) {
  if (!node->is_container()) {
    NOTREACHED();
    //return false;
  }

  if (node->containerEdge() == edge) {
    return true;
  }

  const Menu_Node* menu = node->GetMenu();
  if (!menu) {
    NOTREACHED();
    //return false;
  }

  RemoveBundleTag(node, false);

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
    //return false;
  }

  RemoveBundleTag(node, true);

  Menu_Node* parent = node->parent();
  int index = parent->GetIndexOf(node).value();
  parent->Remove(index);

  Save();

  for (auto& observer : observers_)
    observer.MenuModelChanged(this, -1, menu->action());

  return true;
}

bool Menu_Model::RemoveAction(Menu_Node* root, const std::string& action) {
  for (auto& child : root->children()) {
    Menu_Node* menu = const_cast<Menu_Node*>(child->GetMenu());
    if (menu) {
      bool did_remove = false;

      Menu_Node* item = menu->GetByAction(action);
      while (item) {
        RemoveBundleTag(item, false);
        Menu_Node* parent = item->parent();
        int index = parent->GetIndexOf(item).value();
        parent->Remove(index);
        did_remove = true;
        item = menu->GetByAction(action);
      }

      if (did_remove) {
        for (auto& observer : observers_) {
          observer.MenuModelChanged(this, -1, menu->action());
        }
      }
    }
  }

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
    store_->Load(CreateLoadDetails(menu, true));
    return true;
  }
  return false;
}

bool Menu_Model::ResetAll() {
  if (store_.get()) {
    store_->Load(CreateLoadDetails("", true));
  } else {
    // The context menu model is loaded on demand (the first time a context
    // menu is requested). This has not happened yet.
    Load(true);
  }
  return true;
}

void Menu_Model::RemoveBundleTag(Menu_Node* node, bool include_children) {
  if (node->origin() == Menu_Node::BUNDLE) {
    // Add guid to list of items that can not be touched by an update.
    control_->deleted.push_back(node->guid());
    // Tag the node as modified.
    node->SetOrigin(Menu_Node::MODIFIED_BUNDLE);
  }
  if (node->is_folder() && include_children) {
    for (auto& child : node->children()) {
      RemoveBundleTag(child.get(), include_children);
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
    n->SetGuid(base::Uuid::GenerateRandomV4().AsLowercaseString());
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

Menu_Node* Menu_Model::GetMenuByResourceName(const std::string& menu) {
  // We have <root> -> <top nodes> -> <named menus>
  for (const auto& top_node : root_.children()) {
    for (const auto& menu_node : top_node->children()) {
      // Resource name is stored in the action field for menu nodes.
      if (menu_node->action() == menu) {
        return menu_node.get();
      }
    }
  }
  return nullptr;
}

}  // namespace menus
