// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "menus/menu_upgrade.h"

#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
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
  // Do profile cleanup if flagged by any of the two previous steps
  if (needs_fixup_) {
    for (auto& top_node : profile_root->GetList()) {
      if (top_node.is_dict()) {
        const std::string* type = top_node.FindStringPath("type");
        const std::string* action = top_node.FindStringPath("action");
        if (type && *type == "menu" && action) {
          if (!FixupProfile(top_node, "", bundled_root.get(),
                            profile_root.get(), *action)) {
            // Log error, but do not stop.
            LOG(ERROR) << "Menu Upgrade: Fixup failed for " << *action;
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

base::Value* MenuUpgrade::FindNodeByAction(base::Value& value,
                                           bool include_children,
                                           const std::string& needle_action) {
  if (value.is_list()) {
    for (auto& item : value.GetList()) {
      base::Value* matched_value =
          FindNodeByAction(item, include_children, needle_action);
      if (matched_value) {
        return matched_value;
      }
    }
  } else if (value.is_dict()) {
    const std::string* action = value.FindStringPath("action");
    const std::string* guid = value.FindStringPath("guid");
    if (action && guid) {
      if (*action == needle_action) {
        return &value;
      }
      if (include_children) {
        base::Value* children = value.FindPath("children");
        if (children && children->is_list()) {
          base::Value* matched_value =
              FindNodeByAction(*children, true, needle_action);
          if (matched_value) {
            return matched_value;
          }
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
        if (!IsDeletedOrModified(*guid)) {
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
      // Remove() in the recursive call to RemoveFromProfile() can remove more
      // than one item in case the item to be removed has a guid that is
      // (wrongly) duplicated in the installed file. That was the case when we
      // upgraded to 3.8. For simplicity, instead of returning the delete count,
      // we check the list size.
      if (static_cast<unsigned long>(i) >= profile_value.GetList().size()) {
        i = profile_value.GetList().size() - 1;
      }
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
    } else if (origin == Menu_Node::MODIFIED_BUNDLE &&
               !IsDeletedOrModified(*guid)) {
      // This means the item has gotten a new guid (which we did intentionally
      // at an early stage of development). We do not want that.
      needs_fixup_ = true;
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

bool MenuUpgrade::FixupProfile(base::Value& profile_value,
                               const std::string& parent_guid,
                               base::Value* bundle_root,
                               base::Value* profile_root,
                               const std::string& menu_action) {
  if (profile_value.is_list()) {
    for (int i = profile_value.GetList().size() - 1; i >= 0; i--) {
      auto& item = profile_value.GetList()[i];
      if (!FixupProfile(item, parent_guid, bundle_root, profile_root,
                        menu_action)) {
        return false;
      }
    }
  } else {
    const std::string* guid = profile_value.FindStringKey("guid");
    if (!guid) {
      return false;
    }

    int origin = profile_value.FindIntKey("origin").value_or(Menu_Node::BUNDLE);
    if (origin == Menu_Node::MODIFIED_BUNDLE && !IsDeletedOrModified(*guid)) {
      // We used to reassign the guid when modfying an item. We get here because
      // the guid in question is not in the deleted list (which contains guids
      // for modified or deleted bundled items). For sync this is a problem as
      // it can trigger duplicates. We try to undo the guid change here.
      //
      // Get the action of the item and look it up in the bundled menu. An
      // action is only used once in the bundled menu with one exception
      // (COMMAND_SHOW_BOOKMARKS) so we first find the folder and next the item
      // in that folder because of that exception.
      const std::string* action = profile_value.FindStringPath("action");
      if (!action) {
        return false;
      }
      base::Value* menu = FindNodeByAction(*bundle_root, true, menu_action);
      if (!menu) {
        return false;
      }

      base::Value* folder = FindNodeByGuid(*menu, parent_guid);
      if (folder) {
        base::Value* children = folder->FindPath("children");
        if (children && children->is_list()) {
          base::Value* match = FindNodeByAction(*children, false, *action);
          if (match) {
            const std::string* match_guid = match->FindStringKey("guid");
            if (match_guid && IsDeletedOrModified(*match_guid)) {
              // We now have the bundled guid for the item and we know it is in
              // the modified list. Let the profile cunterpart use this guid
              // once again.
              profile_value.SetStringKey("guid", *match_guid);
            }
          }
        }
      }
    }
    base::Value* children = profile_value.FindPath("children");
    if (children && children->is_list()) {
      if (!FixupProfile(*children, *guid, bundle_root, profile_root,
                        menu_action)) {
        return false;
      }
    }
  }

  return true;
}

bool MenuUpgrade::IsDeletedOrModified(const std::string& guid) {
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
      for (auto& child : children->GetList()) {
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
  if (parent_guid.empty()) {
    // An entire menu.
    if (profile_root->is_list()) {
      const std::string* guid = profile_value.FindStringPath("guid");
      if (!guid) {
        return false;
      }
      for (int i = profile_root->GetList().size() - 1; i >= 0; i--) {
        // Not all toplevel items have a guid so test it exists before matching.
        const auto& item = profile_root->GetList()[i];
        const std::string* item_guid = item.FindStringPath("guid");
        if (item_guid && *item_guid == *guid) {
          // We used to return true when excatly one element was removed, but
          // due to some duplicate ids that got added to a bundled file, some
          // in the same sub menu, we now accept that we can delete more than
          // one.
          return profile_root->EraseListValue(profile_value) >= 1;
        }
      }
    }
  } else {
    // A folder or item within a menu.
    base::Value* matched_value = FindNodeByGuid(*profile_root, parent_guid);
    if (matched_value) {
      base::Value* children = matched_value->FindPath("children");
      if (children && children->is_list()) {
        // We used to return true when excatly one element was removed, but
        // due to some duplicate ids that got added to a bundled file, some
        // in the same sub menu, we now accept that we can delete more than one.
        return children->EraseListValue(profile_value) >= 1;
      }
    }
  }
  return false;
}

}  // namespace menus
