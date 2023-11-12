// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#include "sessions/index_node.h"

namespace sessions {

int64_t Index_Node::id_counter = Index_Node::kFirstDynamicNodeId;

const char Index_Node::kRootNodeGuid[] =
  "00000000-0000-5000-a000-000000000001";
const char Index_Node::kItemsNodeGuid[] =
  "00000000-0000-5000-a000-000000000002";
const char Index_Node::kTrashNodeGuid[] =
  "00000000-0000-5000-a000-000000000003";
const char Index_Node::kAutosaveNodeGuid[] =
  "00000000-0000-5000-a000-000000000004";
const char Index_Node::kBackupNodeGuid[] =
  "00000000-0000-5000-a000-000000000005";
const char Index_Node::kPersistentNodeGuid[] =
  "00000000-0000-5000-a000-000000000006";

Index_Node::Index_Node(const std::string& guid, int64_t id, Type type)
    : guid_(guid), id_(id), type_(type) {}

Index_Node::~Index_Node() {}

Index_Node* Index_Node::GetById(int64_t id) {
  if (id_ == id) {
    return this;
  }
  for (auto& child : children()) {
    Index_Node* node = child->GetById(id);
    if (node) {
      return node;
    }
  }
  return nullptr;
}

Index_Node* Index_Node::GetByGuid(const std::string& guid) {
  if (guid_ == guid) {
    return this;
  }
  for (auto& child : children()) {
    Index_Node* node = child->GetByGuid(guid);
    if (node) {
      return node;
    }
  }
  return nullptr;
}

void Index_Node::Copy(const Index_Node* from) {
  // We do not assign a container guid here.
  SetTitle(from->GetTitle());
  SetFilename(from->filename());
  SetCreateTime(from->create_time());
  SetModifyTime(from->modify_time());
  SetWindowsCount(from->windows_count());
  SetTabsCount(from->tabs_count());
  SetQuarantineCount(from->quarantine_count());
  base::Value::List workspaces(from->workspaces().Clone());
  SetWorkspaces(std::move(workspaces));
  base::Value::Dict group_names(from->group_names().Clone());
  SetGroupNames(std::move(group_names));
}

void Index_Node::DumpTree(int indent) {
  printf("%*s%s %d %s\n", indent, "", guid_.c_str(), static_cast<int>(id_),
      filename_.c_str());
  if (children().size() > 0) {
    for (const auto& child : children()) {
      child->DumpTree(indent + 1);
    }
  }
}

}  // namespace sessions
