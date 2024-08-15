// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "menus/menu_node.h"

namespace menus {

int64_t Menu_Node::id_counter = Menu_Node::kFirstDynamicNodeId;

const char Menu_Node::kRootNodeGuid[] = "00000000-0000-5000-a000-000000000001";
const char Menu_Node::kMainmenuNodeGuid[] =
    "00000000-0000-5000-a000-000000000002";

Menu_Control::Menu_Control() {}
Menu_Control::~Menu_Control() {}

Menu_Node::Menu_Node(const std::string& guid, int64_t id)
    : type_(UNKNOWN), origin_(BUNDLE), guid_(guid), id_(id) {}

Menu_Node::~Menu_Node() {}

Menu_Node* Menu_Node::GetById(int64_t id) {
  if (id_ == id) {
    return this;
  }
  for (auto& child : children()) {
    Menu_Node* node = child->GetById(id);
    if (node) {
      return node;
    }
  }
  return nullptr;
}

Menu_Node* Menu_Node::GetByGuid(const std::string& guid) {
  if (guid_ == guid) {
    return this;
  }
  for (auto& child : children()) {
    Menu_Node* node = child->GetByGuid(guid);
    if (node) {
      return node;
    }
  }
  return nullptr;
}

Menu_Node* Menu_Node::GetByAction(const std::string& action) {
  if (action_ == action) {
    return this;
  }
  for (auto& child : children()) {
    Menu_Node* node = child->GetByAction(action);
    if (node) {
      return node;
    }
  }
  return nullptr;
}

const Menu_Node* Menu_Node::GetMenu() const {
  if (is_menu()) {
    return this;
  } else if (is_root()) {
    return nullptr;
  } else {
    return parent()->GetMenu();
  }
}

Menu_Node* Menu_Node::GetMenuByResourceName(const std::string& menu) {
  Menu_Node* node = this;
  while (parent() && node->id() != mainmenu_node_id()) {
    node = parent();
  }
  if (node->id() == mainmenu_node_id()) {
    for (const auto& menu_node : node->children()) {
      // Resource name is stored in the action field for menu nodes.
      if (menu_node->action() == menu) {
        return menu_node.get();
      }
    }
  }
  return nullptr;
}

void Menu_Node::SetShowShortcut(std::optional<bool> show_shortcut) {
  show_shortcut_ = show_shortcut;
  for (auto& child : children()) {
    child->SetShowShortcut(show_shortcut);
  }
}

void Menu_Node::DumpTree(int indent) {
  if (type_ == CONTAINER) {
    printf("%*s%d %d (mode: %s)\n", indent, "", type_, static_cast<int>(id_),
           container_mode_.c_str());
  } else if (type_ == RADIO) {
    printf("%*s%d %d (radio group: %s)\n", indent, "", type_,
           static_cast<int>(id_), radio_group_.c_str());
  } else if (type_ == SEPARATOR) {
    printf("%*s%d %s\n", indent, "", type_, "separator");
  } else {
    printf("%*s%d %d %s\n", indent, "", type_, static_cast<int>(id_),
           action_.c_str());
  }
  if (children().size() > 0) {
    for (const auto& child : children()) {
      child->DumpTree(indent + 1);
    }
  }
}

}  // namespace menus
