//
// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved.
//
#include "extensions/api/menu_content/menu_content_api.h"

#include "base/guid.h"
#include "base/lazy_instance.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/tools/vivaldi_tools.h"
#include "menus/context_menu_service_factory.h"
#include "menus/main_menu_service_factory.h"
#include "menus/menu_model.h"

namespace extensions {

using content::BrowserContext;
using extensions::vivaldi::menu_content::NodeType;
using extensions::vivaldi::menu_content::MenuTreeNode;
using menus::ContextMenuServiceFactory;
using menus::MainMenuServiceFactory;
using menus::Menu_Model;
using menus::Menu_Node;

namespace {
typedef std::pair<Menu_Node*, Menu_Model*> NodeModel;

NodeModel GetMenu(BrowserContext* browser_context,
    const std::string& named_menu) {
  Menu_Model *model;
  Menu_Node* menu;

  model = MainMenuServiceFactory::GetForBrowserContext(browser_context);
  menu = model->GetNamedMenu(named_menu);
  if (menu) {
    return std::make_pair(menu, model);
  }
  model = ContextMenuServiceFactory::GetForBrowserContext(browser_context);
  menu = model->GetNamedMenu(named_menu);
  if (menu) {
    return std::make_pair(menu, model);
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
    tree_node.custom.reset(new bool(true));
  }
  if (menu_node->hasCustomTitle()) {
    tree_node.title.reset(
        new std::string(base::UTF16ToUTF8(menu_node->GetTitle())));
  }
  switch (menu_node->type()) {
    case menus::Menu_Node::MENU:
      tree_node.type = vivaldi::menu_content::NODE_TYPE_MENU;
      break;
    case menus::Menu_Node::COMMAND:
      tree_node.type = vivaldi::menu_content::NODE_TYPE_COMMAND;
      tree_node.parameter.reset(new std::string(menu_node->parameter()));
      break;
    case menus::Menu_Node::CHECKBOX:
      tree_node.type = vivaldi::menu_content::NODE_TYPE_CHECKBOX;
      break;
    case menus::Menu_Node::RADIO:
      tree_node.type = vivaldi::menu_content::NODE_TYPE_RADIO;
      tree_node.radiogroup.reset(new std::string(menu_node->radioGroup()));
      break;
    case menus::Menu_Node::FOLDER:
      tree_node.type = vivaldi::menu_content::NODE_TYPE_FOLDER;
      break;
    case menus::Menu_Node::SEPARATOR:
      tree_node.type = vivaldi::menu_content::NODE_TYPE_SEPARATOR;
      break;
    case menus::Menu_Node::CONTAINER:
      tree_node.type = vivaldi::menu_content::NODE_TYPE_CONTAINER;
      tree_node.containermode = vivaldi::menu_content::ParseContainerMode(
          menu_node->containerMode());
      tree_node.containeredge = vivaldi::menu_content::ParseContainerEdge(
          menu_node->containerEdge());
      break;
    default:
      tree_node.type = vivaldi::menu_content::NODE_TYPE_NONE;
      break;
  }

  if (menu_node->is_folder() || menu_node->is_menu()) {
    std::vector<MenuTreeNode> children;
    for (auto& child : menu_node->children()) {
      children.push_back(MakeAPITreeNode(child.get()));
    }
    tree_node.children.reset(
        new std::vector<MenuTreeNode>(std::move(children)));
  }

  return tree_node;
}

}  // namespace

static base::LazyInstance<BrowserContextKeyedAPIFactory<
    MenuContentAPI>>::DestructorAtExit g_menu_content =
        LAZY_INSTANCE_INITIALIZER;

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
  main_menu_model_ = MainMenuServiceFactory::GetForBrowserContext(
    browser_context_);
  main_menu_model_->AddObserver(this);

  DCHECK(!context_menu_model_);
  context_menu_model_ = ContextMenuServiceFactory::GetForBrowserContext(
    browser_context_);
  context_menu_model_->AddObserver(this);

  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

void MenuContentAPI::MenuModelChanged(menus::Menu_Model* model,
                                      int64_t select_id,
                                      const std::string& named_menu) {
  MenuContentAPI::SendOnChanged(browser_context_, model, select_id,
                                named_menu);
}

// static
void MenuContentAPI::SendOnChanged(BrowserContext* browser_context,
                                   Menu_Model* model,
                                   int64_t select_id,
                                   const std::string& named_menu) {
  Menu_Node* menu = model ? model->GetNamedMenu(named_menu) : nullptr;
  if (menu) {
    // We want the children of the menu node
    std::vector<MenuTreeNode> nodes;
    if (menu->is_menu()) {
      for (auto& child : menu->children()) {
        nodes.push_back(MakeAPITreeNode(child.get()));
      }
    }
    ::vivaldi::BroadcastEvent(
      vivaldi::menu_content::OnChanged::kEventName,
      vivaldi::menu_content::OnChanged::Create(named_menu,
                                               base::NumberToString(menu->id()),
                                               base::NumberToString(select_id),
                                               nodes),
      browser_context);
  }
}

// static
BrowserContextKeyedAPIFactory<MenuContentAPI>*
    MenuContentAPI::GetFactoryInstance() {
  return g_menu_content.Pointer();
}

ExtensionFunction::ResponseAction MenuContentGetFunction::Run() {
  std::unique_ptr<vivaldi::menu_content::Get::Params> params(
      vivaldi::menu_content::Get::Params::Create(*args_));
  NodeModel pair = GetMenu(browser_context(), params->named_menu);
  if (pair.first) {
    SendResponse(pair.second, params->named_menu);
  } else {
    // No model contains the requested menu. The context menu model is loaded
    // on demand so we may have to do that now.
    Menu_Model* model = ContextMenuServiceFactory::GetForBrowserContext(
        browser_context());
    if (model->loaded()) {
      Respond(Error("Menu not available - " + params->named_menu));
    } else {
      AddRef();  // Balanced in MenuModelLoaded().
      model->AddObserver(this);
      model->Load();
      return RespondLater();
    }
  }

  return AlreadyResponded();
}

void MenuContentGetFunction::MenuModelLoaded(Menu_Model* model) {
  std::unique_ptr<vivaldi::menu_content::Get::Params> params(
      vivaldi::menu_content::Get::Params::Create(*args_));
  SendResponse(model, params->named_menu);
  model->RemoveObserver(this);
  Release();  // Balanced in Run().
}

void MenuContentGetFunction::SendResponse(Menu_Model* model,
                                          const std::string& named_menu) {
  Menu_Node* menu = model->GetNamedMenu(named_menu);
  if (!menu) {
    Respond(Error("Menu not available - " + named_menu));
  } else {
    vivaldi::menu_content::Role role =
        vivaldi::menu_content::ParseRole(menu->role());
    if (role == vivaldi::menu_content::ROLE_NONE) {
      Respond(Error("Unknown menu role"));
      return;
    }

    // We want the children of the menu node
    std::vector<MenuTreeNode> nodes;
    if (menu->is_menu()) {
      for (auto& child : menu->children()) {
        nodes.push_back(MakeAPITreeNode(child.get()));
      }
    }
    Respond(ArgumentList(vivaldi::menu_content::Get::Results::Create(
        base::NumberToString(menu->id()), role, nodes)));
  }
}

ExtensionFunction::ResponseAction MenuContentMoveFunction::Run() {
  std::unique_ptr<vivaldi::menu_content::Move::Params> params(
      vivaldi::menu_content::Move::Params::Create(*args_));
  bool success = false;
  NodeModel pair = GetMenu(browser_context(), params->named_menu);
  if (pair.first && pair.first->parent()) {
    success = true;
    for (auto& id : params->ids) {
      Menu_Node* node = pair.first->GetById(GetIdFromString(id));
      Menu_Node* parent = pair.second->root_node().GetById(
          GetIdFromString(params->parent_id));
      pair.second->Move(node, parent, params->index);
    }
  }

  return RespondNow(ArgumentList(
      vivaldi::menu_content::Move::Results::Create(success)));
}

ExtensionFunction::ResponseAction MenuContentCreateFunction::Run() {
  std::unique_ptr<vivaldi::menu_content::Create::Params> params(
      vivaldi::menu_content::Create::Params::Create(*args_));
  bool success = false;
  std::vector<std::string> ids;
  NodeModel pair = GetMenu(browser_context(), params->named_menu);
  if (pair.first && pair.first->parent()) {
    success = true;
    Menu_Node* parent = pair.second->root_node().GetById(
        GetIdFromString(params->parent_id));
    int index = params->index < 0 ? parent->children().size() : params->index;

    for (auto& item : params->items) {
      std::unique_ptr<Menu_Node> node = std::make_unique<Menu_Node>(
          base::GenerateGUID(), Menu_Node::GetNewId());
      node->SetOrigin(Menu_Node::USER);
      if (item.type == vivaldi::menu_content::NODE_TYPE_SEPARATOR) {
        node->SetType(Menu_Node::SEPARATOR);
      } else if (item.type == vivaldi::menu_content::NODE_TYPE_COMMAND) {
        node->SetType(Menu_Node::COMMAND);
        node->SetAction(item.action);
        if (item.parameter) {
          node->SetParameter(*item.parameter);
        }
      } else if (item.type == vivaldi::menu_content::NODE_TYPE_CHECKBOX) {
        node->SetType(Menu_Node::CHECKBOX);
        node->SetAction(item.action);
      } else if (item.type == vivaldi::menu_content::NODE_TYPE_RADIO) {
        node->SetType(Menu_Node::RADIO);
        node->SetAction(item.action);
      } else if (item.type == vivaldi::menu_content::NODE_TYPE_FOLDER) {
        node->SetType(Menu_Node::FOLDER);
        std::string action = std::string("MENU_") + std::to_string(node->id());
        node->SetAction(action);
      } else if (item.type == vivaldi::menu_content::NODE_TYPE_CONTAINER) {
        node->SetType(Menu_Node::CONTAINER);
        node->SetAction(item.action);
        node->SetContainerMode(vivaldi::menu_content::ToString(
            item.containermode == vivaldi::menu_content::CONTAINER_MODE_NONE ?
            vivaldi::menu_content::CONTAINER_MODE_FOLDER :
            item.containermode));
        node->SetContainerEdge(vivaldi::menu_content::ToString(
          item.containeredge == vivaldi::menu_content::CONTAINER_EDGE_NONE ?
          vivaldi::menu_content::CONTAINER_EDGE_BELOW :
          item.containeredge));
      } else {
        continue;
      }
      if (item.title &&
          item.type != vivaldi::menu_content::NODE_TYPE_SEPARATOR) {
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
  std::unique_ptr<vivaldi::menu_content::Remove::Params> params(
      vivaldi::menu_content::Remove::Params::Create(*args_));
  bool success = false;
  NodeModel pair = GetMenu(browser_context(), params->named_menu);
  if (pair.first && pair.first->parent()) {
    success = true;
    for (auto& id : params->ids) {
      Menu_Node* node = pair.first->GetById(GetIdFromString(id));
      pair.second->Remove(node);
    }
  }

  return RespondNow(ArgumentList(
      vivaldi::menu_content::Remove::Results::Create(success)));
}

ExtensionFunction::ResponseAction MenuContentUpdateFunction::Run() {
  std::unique_ptr<vivaldi::menu_content::Update::Params> params(
      vivaldi::menu_content::Update::Params::Create(*args_));
  bool success = false;
  NodeModel pair = GetMenu(browser_context(), params->named_menu);
  if (pair.first && pair.first->parent()) {
    success = true;
    Menu_Node* node = pair.first->GetById(GetIdFromString(params->id));
    if (node) {
      if (params->changes.title) {
        success = pair.second->SetTitle(node,
            base::UTF8ToUTF16(*params->changes.title));
      }
      if (params->changes.parameter) {
        success = pair.second->SetParameter(node, *params->changes.parameter);
      }
      if (success && params->changes.container_mode) {
        success = pair.second->SetContainerMode(node,
            vivaldi::menu_content::ToString(params->changes.container_mode));
      }
      if (success && params->changes.container_edge) {
        success = pair.second->SetContainerEdge(node,
            vivaldi::menu_content::ToString(params->changes.container_edge));
      }
    }
  }

  return RespondNow(ArgumentList(
      vivaldi::menu_content::Update::Results::Create(success)));
}

ExtensionFunction::ResponseAction MenuContentResetFunction::Run() {
  std::unique_ptr<vivaldi::menu_content::Reset::Params> params(
      vivaldi::menu_content::Reset::Params::Create(*args_));
  bool success = false;
  NodeModel pair = GetMenu(browser_context(), params->named_menu);
  if (pair.first && pair.first->parent()) {
    success = true;
    if (params->ids) {
      for (auto& id : *params->ids) {
        Menu_Node* node = pair.first->GetById(GetIdFromString(id));
        if (node) {
          pair.second->Reset(node);
        }
      }
    } else {
      pair.second->Reset(pair.first);
    }
  } else if (pair.first) {
    // Reset all and report back that name menu is to be used afterwards.
    pair.second->Reset(params->named_menu);
  }

  return RespondNow(ArgumentList(
      vivaldi::menu_content::Reset::Results::Create(success)));
}

}  // namespace extensions
