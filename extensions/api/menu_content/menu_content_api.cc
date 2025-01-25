//
// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved.
//
#include "extensions/api/menu_content/menu_content_api.h"

#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/tools/vivaldi_tools.h"
#include "menus/context_menu_service_factory.h"
#include "menus/main_menu_service_factory.h"
#include "menus/menu_model.h"

namespace extensions {

using content::BrowserContext;
using extensions::vivaldi::menu_content::MenuTreeNode;
using extensions::vivaldi::menu_content::NodeType;
using menus::ContextMenuServiceFactory;
using menus::MainMenuServiceFactory;
using menus::Menu_Model;
using menus::Menu_Node;

namespace {
typedef std::pair<Menu_Node*, Menu_Model*> NodeModel;

NodeModel GetMenu(BrowserContext* browser_context,
                  const std::string& menu) {
  Menu_Model* model;
  Menu_Node* node;

  model = MainMenuServiceFactory::GetForBrowserContext(browser_context);
  node = model->GetMenuByResourceName(menu);
  if (node) {
    return std::make_pair(node, model);
  }
  model = ContextMenuServiceFactory::GetForBrowserContext(browser_context);
  node = model->GetMenuByResourceName(menu);
  if (node) {
    return std::make_pair(node, model);
  }
  return std::make_pair(nullptr, nullptr);
}

int64_t GetIdFromString(const std::string& string_id) {
  int64_t id;
  // All valid ids are in range [0..maxint]
  return (!base::StringToInt64(string_id, &id) || id < 0) ? -1 : id;
}

MenuTreeNode MakeAPITreeNode(Menu_Node* menu_node) {
  MenuTreeNode tree_node;
  tree_node.id = base::NumberToString(menu_node->id());
  tree_node.action = menu_node->action();
  if (menu_node->origin() == Menu_Node::USER) {
    tree_node.custom = true;
  }
  if (menu_node->hasCustomTitle()) {
    tree_node.title = base::UTF16ToUTF8(menu_node->GetTitle());
  }
  if (menu_node->showShortcut().has_value()) {
    tree_node.showshortcut = menu_node->showShortcut();
  }
  switch (menu_node->type()) {
    case menus::Menu_Node::MENU:
      tree_node.type = vivaldi::menu_content::NodeType::kMenu;
      break;
    case menus::Menu_Node::COMMAND:
      tree_node.type = vivaldi::menu_content::NodeType::kCommand;
      tree_node.parameter = menu_node->parameter();
      break;
    case menus::Menu_Node::CHECKBOX:
      tree_node.type = vivaldi::menu_content::NodeType::kCheckbox;
      break;
    case menus::Menu_Node::RADIO:
      tree_node.type = vivaldi::menu_content::NodeType::kRadio;
      tree_node.radiogroup = menu_node->radioGroup();
      break;
    case menus::Menu_Node::FOLDER:
      tree_node.type = vivaldi::menu_content::NodeType::kFolder;
      break;
    case menus::Menu_Node::SEPARATOR:
      tree_node.type = vivaldi::menu_content::NodeType::kSeparator;
      break;
    case menus::Menu_Node::CONTAINER:
      tree_node.type = vivaldi::menu_content::NodeType::kContainer;
      tree_node.containermode =
          vivaldi::menu_content::ParseContainerMode(menu_node->containerMode());
      tree_node.containeredge =
          vivaldi::menu_content::ParseContainerEdge(menu_node->containerEdge());
      break;
    default:
      tree_node.type = vivaldi::menu_content::NodeType::kNone;
      break;
  }

  if (menu_node->is_folder() || menu_node->is_menu()) {
    std::vector<MenuTreeNode> children;
    for (auto& child : menu_node->children()) {
      children.push_back(MakeAPITreeNode(child.get()));
    }
    tree_node.children = std::move(children);
  }

  return tree_node;
}

}  // namespace

static base::LazyInstance<BrowserContextKeyedAPIFactory<MenuContentAPI>>::
    DestructorAtExit g_menu_content = LAZY_INSTANCE_INITIALIZER;

MenuContentAPI::MenuContentAPI(BrowserContext* browser_context)
    : browser_context_(browser_context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this,
                                 vivaldi::menu_content::OnChanged::kEventName);
}

void MenuContentAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
  if (main_menu_model_) {
    main_menu_model_->RemoveObserver(this);
    main_menu_model_ = nullptr;
  }
  if (context_menu_model_) {
    context_menu_model_->RemoveObserver(this);
    context_menu_model_ = nullptr;
  }
}

void MenuContentAPI::OnListenerAdded(const EventListenerInfo& details) {
  DCHECK(!main_menu_model_);
  main_menu_model_ =
      MainMenuServiceFactory::GetForBrowserContext(browser_context_);
  main_menu_model_->AddObserver(this);

  DCHECK(!context_menu_model_);
  context_menu_model_ =
      ContextMenuServiceFactory::GetForBrowserContext(browser_context_);
  context_menu_model_->AddObserver(this);

  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

void MenuContentAPI::MenuModelChanged(menus::Menu_Model* model,
                                      int64_t select_id,
                                      const std::string& menu) {
  MenuContentAPI::SendOnChanged(browser_context_, model, select_id, menu);
}

void MenuContentAPI::MenuModelReset(menus::Menu_Model* model, bool all) {
  if (all) {
    SendOnResetAll(browser_context_, model);
  }
}

// static
void MenuContentAPI::SendOnChanged(BrowserContext* browser_context,
                                   Menu_Model* model,
                                   int64_t select_id,
                                   const std::string& menu) {
  Menu_Node* node = model ? model->GetMenuByResourceName(menu) : nullptr;
  if (node) {
    // We want the children of the menu node
    std::vector<MenuTreeNode> nodes;
    if (node->is_menu()) {
      for (auto& child : node->children()) {
        nodes.push_back(MakeAPITreeNode(child.get()));
      }
    }

    vivaldi::menu_content::Role role =
        vivaldi::menu_content::ParseRole(node->role());
    if (role == vivaldi::menu_content::Role::kNone) {
      LOG(ERROR) << "Menu changed. Unknown menu role detected.";
      return;
    }

    ::vivaldi::BroadcastEvent(vivaldi::menu_content::OnChanged::kEventName,
                              vivaldi::menu_content::OnChanged::Create(
                                  menu, base::NumberToString(node->id()),
                                  role,
                                  base::NumberToString(select_id), nodes),
                              browser_context);
  }
}

// static
void MenuContentAPI::SendOnResetAll(content::BrowserContext*  browser_context,
                                    menus::Menu_Model* model) {
  ::vivaldi::BroadcastEvent(vivaldi::menu_content::OnResetAll::kEventName,
                              vivaldi::menu_content::OnResetAll::Create(
                                model->mode() == Menu_Model::kContextMenu),
                              browser_context);
}

// static
BrowserContextKeyedAPIFactory<MenuContentAPI>*
MenuContentAPI::GetFactoryInstance() {
  return g_menu_content.Pointer();
}

ExtensionFunction::ResponseAction MenuContentGetFunction::Run() {
  std::optional<vivaldi::menu_content::Get::Params> params(
      vivaldi::menu_content::Get::Params::Create(args()));
  NodeModel pair = GetMenu(browser_context(), params->menu);
  if (pair.first) {
    SendResponse(pair.second, params->menu);
  } else {
    // No model contains the requested menu. The context menu model is loaded
    // on demand so we may have to do that now.
    Menu_Model* model =
        ContextMenuServiceFactory::GetForBrowserContext(browser_context());
    if (model->loaded()) {
      Respond(Error("Menu not available - " + params->menu));
    } else {
      AddRef();  // Balanced in MenuModelLoaded().
      model->AddObserver(this);
      model->Load(false);
      return RespondLater();
    }
  }

  return AlreadyResponded();
}

void MenuContentGetFunction::MenuModelLoaded(Menu_Model* model) {
  std::optional<vivaldi::menu_content::Get::Params> params(
      vivaldi::menu_content::Get::Params::Create(args()));
  SendResponse(model, params->menu);
  model->RemoveObserver(this);
  Release();  // Balanced in Run().
}

void MenuContentGetFunction::SendResponse(Menu_Model* model,
                                          const std::string& menu) {
  Menu_Node* node = model->GetMenuByResourceName(menu);
  if (!node) {
    Respond(Error("Menu not available - " + menu));
  } else {
    vivaldi::menu_content::Role role =
        vivaldi::menu_content::ParseRole(node->role());
    if (role == vivaldi::menu_content::Role::kNone) {
      Respond(Error("Unknown menu role"));
      return;
    }

    // We want the children of the menu node
    std::vector<MenuTreeNode> nodes;
    if (node->is_menu()) {
      for (auto& child : node->children()) {
        nodes.push_back(MakeAPITreeNode(child.get()));
      }
    }
    Respond(ArgumentList(vivaldi::menu_content::Get::Results::Create(
        base::NumberToString(node->id()), role, nodes)));
  }
}

ExtensionFunction::ResponseAction MenuContentMoveFunction::Run() {
  std::optional<vivaldi::menu_content::Move::Params> params(
      vivaldi::menu_content::Move::Params::Create(args()));
  bool success = false;
  NodeModel pair = GetMenu(browser_context(), params->menu);
  if (pair.first && pair.first->parent()) {
    success = true;
    Menu_Node* parent =
        pair.second->root_node().GetById(GetIdFromString(params->parent_id));
    uint64_t index =
        params->index < 0 ? parent->children().size() : params->index;
    for (auto& id : params->ids) {
      Menu_Node* node = pair.first->GetById(GetIdFromString(id));
      if (!node) {
        return RespondNow(Error("Illegal menu item identifier: " + id));
      }
      // Prevent items (if more than one) to be added in reverse order by
      // stepping the target index once one element has been added to the model.
      // Needed when moving items into another folder (parent differs) or when
      // moving towards the start of the list in the same folder.
      bool step = node->parent() != parent ||
                  node->parent()->GetIndexOf(node).value() > index;
      pair.second->Move(node, parent, index);
      if (step) {
        index++;
      }
    }
  }

  return RespondNow(
      ArgumentList(vivaldi::menu_content::Move::Results::Create(success)));
}

ExtensionFunction::ResponseAction MenuContentCreateFunction::Run() {
  std::optional<vivaldi::menu_content::Create::Params> params(
      vivaldi::menu_content::Create::Params::Create(args()));
  bool success = false;
  std::vector<std::string> ids;
  NodeModel pair = GetMenu(browser_context(), params->menu);
  if (pair.first && pair.first->parent()) {
    success = true;
    Menu_Node* parent =
        pair.second->root_node().GetById(GetIdFromString(params->parent_id));
    int index = params->index < 0 ? parent->children().size() : params->index;

    for (auto& item : params->items) {
      std::unique_ptr<Menu_Node> node = std::make_unique<Menu_Node>(
          base::Uuid::GenerateRandomV4().AsLowercaseString(), Menu_Node::GetNewId());
      node->SetOrigin(Menu_Node::USER);
      if (item.type == vivaldi::menu_content::NodeType::kSeparator) {
        node->SetType(Menu_Node::SEPARATOR);
      } else if (item.type == vivaldi::menu_content::NodeType::kCommand) {
        node->SetType(Menu_Node::COMMAND);
        node->SetAction(item.action);
        if (item.parameter) {
          node->SetParameter(*item.parameter);
        }
      } else if (item.type == vivaldi::menu_content::NodeType::kCheckbox) {
        node->SetType(Menu_Node::CHECKBOX);
        node->SetAction(item.action);
      } else if (item.type == vivaldi::menu_content::NodeType::kRadio) {
        node->SetType(Menu_Node::RADIO);
        node->SetAction(item.action);
      } else if (item.type == vivaldi::menu_content::NodeType::kFolder) {
        node->SetType(Menu_Node::FOLDER);
        std::string action = std::string("MENU_") + std::to_string(node->id());
        node->SetAction(action);
      } else if (item.type == vivaldi::menu_content::NodeType::kContainer) {
        node->SetType(Menu_Node::CONTAINER);
        node->SetAction(item.action);
        node->SetContainerMode(vivaldi::menu_content::ToString(
            item.containermode == vivaldi::menu_content::ContainerMode::kNone
                ? vivaldi::menu_content::ContainerMode::kFolder
                : item.containermode));
        node->SetContainerEdge(vivaldi::menu_content::ToString(
            item.containeredge == vivaldi::menu_content::ContainerEdge::kNone
                ? vivaldi::menu_content::ContainerEdge::kBelow
                : item.containeredge));
      } else {
        continue;
      }
      if (item.title &&
          item.type != vivaldi::menu_content::NodeType::kSeparator) {
        node->SetTitle(base::UTF8ToUTF16(*item.title));
        node->SetHasCustomTitle(true);
      }
      ids.push_back(std::to_string(node->id()));
      pair.second->Add(std::move(node), parent, index);
      index++;
    }
  }

  return RespondNow(ArgumentList(
      vivaldi::menu_content::Create::Results::Create(success, ids)));
}

ExtensionFunction::ResponseAction MenuContentRemoveFunction::Run() {
  std::optional<vivaldi::menu_content::Remove::Params> params(
      vivaldi::menu_content::Remove::Params::Create(args()));
  bool success = false;
  NodeModel pair = GetMenu(browser_context(), params->menu);
  if (pair.first && pair.first->parent()) {
    for (auto& id : params->ids) {
      Menu_Node* node = pair.first->GetById(GetIdFromString(id));
      if (node) {
        pair.second->Remove(node);
        success = true;
      } else {
        LOG(ERROR) << "Can not remove. Unknown menu item id.";
      }
    }
  }

  return RespondNow(
      ArgumentList(vivaldi::menu_content::Remove::Results::Create(success)));
}

ExtensionFunction::ResponseAction MenuContentRemoveActionFunction::Run() {
  // Model for main menus are always loaded on startup while the one for
  // context menus is loaded on demand. Test the latter and load it first if
  // needed.
  Menu_Model* model =
      ContextMenuServiceFactory::GetForBrowserContext(browser_context());
  if (model->loaded()) {
    bool success = HandleRemoval();
    return RespondNow(ArgumentList(
        vivaldi::menu_content::RemoveAction::Results::Create(success)));
  } else {
    AddRef();  // Balanced in MenuModelLoaded().
    model->AddObserver(this);
    model->Load(false);
    return RespondLater();
  }
}

void MenuContentRemoveActionFunction::MenuModelLoaded(Menu_Model* model) {
  model->RemoveObserver(this);
  bool success = HandleRemoval();
  Respond(ArgumentList(
      vivaldi::menu_content::RemoveAction::Results::Create(success)));
  Release();  // Balanced in Run().
}

bool MenuContentRemoveActionFunction::HandleRemoval() {
  std::optional<vivaldi::menu_content::RemoveAction::Params> params(
      vivaldi::menu_content::RemoveAction::Params::Create(args()));
  Menu_Model* model =
      MainMenuServiceFactory::GetForBrowserContext(browser_context());
  if (!model->loaded()) {
    return false;
  }
  for (const std::string& action : params->actions) {
    model->RemoveAction(model->mainmenu_node(), action);
  }
  model = ContextMenuServiceFactory::GetForBrowserContext(browser_context());
  if (!model->loaded()) {
    return false;
  }
  for (const std::string& action : params->actions) {
    model->RemoveAction(model->mainmenu_node(), action);
  }
  return true;
}

ExtensionFunction::ResponseAction MenuContentUpdateFunction::Run() {
  std::optional<vivaldi::menu_content::Update::Params> params(
      vivaldi::menu_content::Update::Params::Create(args()));
  bool success = false;
  NodeModel pair = GetMenu(browser_context(), params->menu);
  if (pair.first && pair.first->parent()) {
    success = true;
    Menu_Node* node = pair.first->GetById(GetIdFromString(params->id));
    if (node) {
      if (params->changes.title) {
        success = pair.second->SetTitle(
            node, base::UTF8ToUTF16(*params->changes.title));
      }
      if (params->changes.parameter) {
        success = pair.second->SetParameter(node, *params->changes.parameter);
      }
      if (params->changes.showshortcut) {
        // Note. We set the value on the menu node. This means the whole menu
        // and all items below are managed by this. We can control this per
        // menu item (the 'node' must be used).
        success = pair.second->SetShowShortcut(pair.first,
                                               *params->changes.showshortcut);
      }
      if (success &&
          params->changes.container_mode != vivaldi::menu_content::ContainerMode::kNone) {
        success = pair.second->SetContainerMode(
            node,
            vivaldi::menu_content::ToString(params->changes.container_mode));
      }
      if (success &&
          params->changes.container_edge != vivaldi::menu_content::ContainerEdge::kNone) {
        success = pair.second->SetContainerEdge(
            node,
            vivaldi::menu_content::ToString(params->changes.container_edge));
      }
    }
  }

  return RespondNow(
      ArgumentList(vivaldi::menu_content::Update::Results::Create(success)));
}

ExtensionFunction::ResponseAction MenuContentResetFunction::Run() {
  std::optional<vivaldi::menu_content::Reset::Params> params(
      vivaldi::menu_content::Reset::Params::Create(args()));
  NodeModel pair = GetMenu(browser_context(), params->menu);
  if (pair.first && pair.first->parent()) {
    if (params->ids) {
      for (auto& id : *params->ids) {
        Menu_Node* node = pair.first->GetById(GetIdFromString(id));
        if (node) {
          AddRef();  // Balanced in MenuModelReset().
          pair.second->AddObserver(this);
          pair.second->Reset(node);
          return RespondLater();
        }
      }
    } else {
      AddRef();  // Balanced in MenuModelReset().
      pair.second->AddObserver(this);
      pair.second->Reset(pair.first);
      return RespondLater();
    }
  } else if (pair.first) {
    // Reset all and report back that name menu is to be used afterwards.
    AddRef();  // Balanced in MenuModelReset().
    pair.second->AddObserver(this);
    pair.second->Reset(params->menu);
    return RespondLater();
  } else {
    Menu_Model* model = params->is_context
      ? ContextMenuServiceFactory::GetForBrowserContext(browser_context())
      : MainMenuServiceFactory::GetForBrowserContext(browser_context());
    if (model) {
      AddRef();  // Balanced in MenuModelReset().
      model->AddObserver(this);
      model->Reset(params->menu);
      return RespondLater();
    }
  }

  return RespondNow(
      ArgumentList(vivaldi::menu_content::Reset::Results::Create(false)));
}

void MenuContentResetFunction::MenuModelReset(menus::Menu_Model* model,
                                              bool all) {
  std::optional<vivaldi::menu_content::Reset::Params> params(
      vivaldi::menu_content::Reset::Params::Create(args()));
  Respond(ArgumentList(vivaldi::menu_content::Reset::Results::Create(true)));
  model->RemoveObserver(this);
  Release();  // Balanced in Run().
}

ExtensionFunction::ResponseAction MenuContentResetAllFunction::Run() {
  std::optional<vivaldi::menu_content::ResetAll::Params> params(
      vivaldi::menu_content::ResetAll::Params::Create(args()));
  Menu_Model* model = params->is_context
      ? ContextMenuServiceFactory::GetForBrowserContext(browser_context())
      : MainMenuServiceFactory::GetForBrowserContext(browser_context());
  if (model) {
    AddRef();  // Balanced in MenuModelReset().
    model->AddObserver(this);
    model->ResetAll();
    return RespondLater();
  }

  return RespondNow(
      ArgumentList(vivaldi::menu_content::ResetAll::Results::Create(false)));
}

void MenuContentResetAllFunction::MenuModelReset(menus::Menu_Model* model,
                                                 bool all) {
  std::optional<vivaldi::menu_content::ResetAll::Params> params(
      vivaldi::menu_content::ResetAll::Params::Create(args()));
  Respond(ArgumentList(vivaldi::menu_content::ResetAll::Results::Create(true)));
  model->RemoveObserver(this);
  Release();  // Balanced in Run().
}

}  // namespace extensions
