// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "menus/menu_upgrade.h"

#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/values.h"
#include "menus/menu_node.h"

namespace menus {

MenuUpgrade::MenuUpgrade() {}
MenuUpgrade::~MenuUpgrade() {}

std::unique_ptr<base::Value> MenuUpgrade::Run(
    const base::FilePath& profile_file,
    const base::FilePath& bundled_file,
    const std::string& version) {
  JSONFileValueDeserializer profile_serializer(profile_file);
  JSONFileValueDeserializer bundled_serializer(bundled_file);
  std::unique_ptr<base::Value> profile_root(
      profile_serializer.Deserialize(NULL, NULL));
  std::unique_ptr<base::Value> bundled_root(
      bundled_serializer.Deserialize(NULL, NULL));

  if (profile_root.get() && bundled_root.get()) {
    // First we need access to the control segment of the profile file.
    base::Value* profile_control = FindControlNode(*profile_root.get());
    if (!profile_control) {
      LOG(ERROR) << "Menu Upgrade: Aborted, control segment missing";
      return nullptr;
    }
    // Set new version
    profile_control->SetStringKey("version", version);
    // Get all deleted nodes to prevent adding them once more.
    const base::Value* deleted = profile_control->FindPath("deleted");
    if (deleted && deleted->is_list()) {
      const base::Value::ConstListView& list = deleted->GetList();
      for (int i = list.size() - 1; i >= 0; i--) {
        std::string s = list[i].GetString();
        deleted_.push_back(s);
      }
    }
    // Add new elements in the profile based tree.
    for (const auto& top_node : bundled_root->GetList()) {
      if (top_node.is_dict()) {
        const std::string* type = top_node.FindStringPath("type");
        if (type && *type == "menu") {
          if (!AddFromBundle(top_node, "", 0, profile_root.get())) {
            LOG(ERROR) << "Menu Upgrade: Aborted, failed to add elements";
            return nullptr;
          }
        }
      }
    }
    // Remove old elements in the profile based tree.
    for (const auto& top_node : profile_root->GetList()) {
      if (top_node.is_dict()) {
        const std::string* type = top_node.FindStringPath("type");
        if (type && *type == "menu") {
          if (!RemoveFromProfile(top_node, "", bundled_root.get(),
              profile_root.get())) {
            LOG(ERROR) << "Menu Upgrade: Aborted, failed to remove elements";
            return nullptr;
          }
        }
      }
    }
  }

  return profile_root;
}

base::Value* MenuUpgrade::FindControlNode(base::Value& value) {
  if (value.is_list()) {
    for (auto& top_node : value.GetList()) {
      if (top_node.is_dict()) {
        std::string* type = top_node.FindStringPath("type");
        if (type && *type == "control") {
          return &top_node;
        }
      }
    }
  }
  return nullptr;
}

base::Value* MenuUpgrade::FindNodeByGuid(base::Value& value,
                                         const std::string& needle_guid) {
  if (value.is_list()) {
    for (auto& item : value.GetList()) {
      base::Value* matched_value = FindNodeByGuid(item, needle_guid);
      if (matched_value) {
        return matched_value;
      }
    }
  } else if (value.is_dict()) {
    const std::string* guid = value.FindStringPath("guid");
    if (guid) {
      if (*guid == needle_guid) {
        return &value;
      }
      base::Value* children = value.FindPath("children");
      if (children && children->is_list()) {
        base::Value* matched_value = FindNodeByGuid(*children, needle_guid);
        if (matched_value) {
          return matched_value;
        }
      }
    }
  }
  return nullptr;
}

bool MenuUpgrade::AddFromBundle(const base::Value& bundle_value,
                                const std::string& parent_guid,
                                int bundle_index,
                                base::Value* profile_root) {
  if (bundle_value.is_list()) {
    int index = 0;
    for (const auto& item : bundle_value.GetList()) {
      if (!AddFromBundle(item, parent_guid, index, profile_root)) {
        return false;
      }
      index++;
    }
  } else if (bundle_value.is_dict()) {
    const std::string* guid = bundle_value.FindStringPath("guid");
    if (guid) {
      base::Value* matched_value = FindNodeByGuid(*profile_root, *guid);
      if (!matched_value) {
        if (!IsDeleted(*guid)) {
          // We will try to insert a new item into the profile based file.
          if (!Insert(bundle_value, parent_guid, bundle_index, profile_root)) {
            return false;
          }
        }
      }
      const base::Value* children = bundle_value.FindPath("children");
      if (children && children->is_list()) {
        if (!AddFromBundle(*children, *guid, 0, profile_root)) {
          return false;
        }
      }
    }
  }
  return true;
}

bool MenuUpgrade::RemoveFromProfile(const base::Value& profile_value,
                                    const std::string& parent_guid,
                                    base::Value* bundle_root,
                                    base::Value* profile_root) {
  if (profile_value.is_list()) {
    for (int i = profile_value.GetList().size() - 1; i >= 0; i--) {
      const auto& item = profile_value.GetList()[i];
      if (!RemoveFromProfile(item, parent_guid, bundle_root, profile_root)) {
        return false;
      }
    }
  } else {
    const std::string* guid = profile_value.FindStringPath("guid");
    if (!guid) {
      return false;
    }

    int origin = profile_value.FindIntKey("origin").value_or(Menu_Node::BUNDLE);
    if (origin == Menu_Node::BUNDLE) {
      if (!FindNodeByGuid(*bundle_root, *guid)) {
        return Remove(profile_value, parent_guid, profile_root);
      }
    }
    const base::Value* children = profile_value.FindPath("children");
    if (children && children->is_list()) {
      if (!RemoveFromProfile(*children, *guid, bundle_root, profile_root)) {
        return false;
      }
    }
  }

  return true;
}

bool MenuUpgrade::IsDeleted(const std::string& guid) {
  for (int i = deleted_.size() - 1; i >= 0; i--) {
    if (guid == deleted_[i]) {
      return true;
    }
  }
  return false;
}


bool MenuUpgrade::Insert(const base::Value& bundle_value,
                         const std::string& parent_guid,
                         int index,
                         base::Value* profile_root) {

  if (parent_guid.empty()) {
    // Special case for top level. Index is not imporant.
    auto c = bundle_value.Clone();
    profile_root->Append(std::move(c));
    return true;
  }

  base::Value* matched_value = FindNodeByGuid(*profile_root, parent_guid);
  if (matched_value) {
    base::Value* children = matched_value->FindPath("children");
    if (children && children->is_list()) {
      base::Value list(base::Value::Type::LIST);
      int i = 0;
      bool added = false;
      for (auto& child : children->TakeList()) {
        if (i++ == index) {
          added = true;
          auto c = bundle_value.Clone();
          list.Append(std::move(c));
        }
        list.Append(std::move(child));
      }
      if (!added) {
        auto c = bundle_value.Clone();
        list.Append(std::move(c));
      }
      matched_value->SetKey("children", std::move(list));
      return true;
    }
  }

  return false;
}

bool MenuUpgrade::Remove(const base::Value& profile_value,
                         const std::string& parent_guid,
                         base::Value* profile_root) {
  base::Value* matched_value = FindNodeByGuid(*profile_root, parent_guid);
  if (matched_value) {
    base::Value* children = matched_value->FindPath("children");
    if (children && children->is_list()) {
      return children->EraseListValue(profile_value) == 1;
    }
  }
  return false;
}

}  // namespace menus
