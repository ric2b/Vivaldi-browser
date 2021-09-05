// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "menus/menu_node.h"

namespace menus {

int64_t Menu_Node::id_counter = 1;

Menu_Control::Menu_Control() {}
Menu_Control::~Menu_Control() {}

Menu_Node::Menu_Node()
  : type_(UNKNOWN),
    origin_(BUNDLE) {}

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

void Menu_Node::DumpTree(int indent) {
  if (type_ == CONTAINER) {
    printf("%*s%d %d (mode: %s)\n", indent, "", type_, static_cast<int>(id_),
            container_mode_.c_str());
  } else if (type_ == RADIO) {
    printf("%*s%d %d (radio group: %s)\n", indent, "", type_,
            static_cast<int>(id_), radio_group_.c_str());
  } else if (type_== SEPARATOR) {
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
