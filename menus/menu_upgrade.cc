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

  if (!profile_root || !bundled_root) {
    LOG(ERROR) << "Menu Upgrade: failed to read JSON files";
    return nullptr;
  }

  if (!profile_root->is_list() || !bundled_root->is_list()) {
    LOG(ERROR) << "Menu Upgrade: invalid JSON structure";
    return nullptr;
  }

  base::Value::List& profile_list = profile_root->GetList();
  base::Value::List& bundled_list = bundled_root->GetList();

  // First we need access to the control segment of the profile file.
  base::Value* profile_control = FindControlNode(profile_list);
  if (!profile_control) {
    LOG(ERROR) << "Menu Upgrade: Aborted, control segment missing";
    return nullptr;
  }
  // Set new version
  profile_control->GetDict().Set("version", version);
  // Get all deleted nodes to prevent adding them once more. We may modify
  // this list so it is written back to profile below.
  base::Value* deleted = profile_control->GetDict().Find("deleted");
  if (deleted && deleted->is_list()) {
    const auto& list = deleted->GetList();
    for (int i = list.size() - 1; i >= 0; i--) {
      std::string s = list[i].GetString();
      deleted_.push_back(s);
    }
  }
  // Add new elements in the profile based tree.
  for (const auto& top_node : bundled_root->GetList()) {
    if (const base::Value::Dict* top_dict = top_node.GetIfDict()) {
      const std::string* type = top_dict->FindString("type");
      if (type && *type == "menu") {
        if (!AddFromBundle(*top_dict, "", 0, profile_list)) {
          LOG(ERROR) << "Menu Upgrade: Aborted, failed to add elements";
          return nullptr;
        }
      }
    }
  }
  // Remove old elements in the profile based tree.
  for (const auto& top_node : profile_list) {
    if (const base::Value::Dict* top_dict = top_node.GetIfDict()) {
      const std::string* type = top_dict->FindString("type");
      if (type && *type == "menu") {
        if (!RemoveFromProfile(*top_dict, "", bundled_list, profile_list)) {
          LOG(ERROR) << "Menu Upgrade: Aborted, failed to remove elements";
          return nullptr;
        }
      }
    }
  }

  // Do profile cleanup if flagged by any of the two previous steps
  if (needs_fixup_) {
    for (auto& top_node : profile_list) {
      if (top_node.is_dict()) {
        const std::string* type = top_node.GetDict().FindString("type");
        const std::string* action = top_node.GetDict().FindString("action");
        if (type && *type == "menu" && action) {
          if (!FixupProfile(top_node.GetDict(), "", bundled_root->GetList(),
                            profile_root->GetList(), *action)) {
            // Log error, but do not stop.
            LOG(ERROR) << "Menu Upgrade: Fixup failed for " << *action;
          }
        }
      }
    }
  }

  // Update deleted list in case we have removed one or more entries.
  // Note. The existing pointer to the control segment may no longer be valid
  // after adding nodes so we must set it up once again.
  profile_control = FindControlNode(profile_list);
  if (!profile_control) {
    LOG(ERROR) << "Menu Upgrade: Aborted, control segment missing after upgrade";
    return nullptr;
  }
  deleted = profile_control->GetDict().Find("deleted");
  if (deleted && deleted->is_list()) {
     base::Value deleted_list(base::Value::Type::LIST);
     for (const auto& elm : deleted_) {
      deleted_list.GetList().Append(elm);
    }
    profile_control->GetDict().Set("deleted", std::move(deleted_list));
  }

  return profile_root;
}

base::Value* MenuUpgrade::FindControlNode(base::Value::List& list) {
  for (auto& top_node : list) {
    if (top_node.is_dict()) {
      std::string* type = top_node.GetDict().FindString("type");
      if (type && *type == "control") {
        return &top_node;
      }
    }
  }
  return nullptr;
}

base::Value::Dict* MenuUpgrade::FindNodeByGuid(base::Value::List& list,
                                               const std::string& needle_guid) {
  for (auto& item : list) {
    if (base::Value::Dict* dict = item.GetIfDict()) {
      if (base::Value::Dict* matched = FindNodeByGuid(*dict, needle_guid)) {
        return matched;
      }
    }
  }
  return nullptr;
}

base::Value::Dict* MenuUpgrade::FindNodeByGuid(base::Value::Dict& dict,
                                               const std::string& needle_guid) {
  if (const std::string* guid = dict.FindString("guid")) {
    if (*guid == needle_guid) {
      return &dict;
    }
    base::Value::List* children = dict.FindList("children");
    if (children) {
      if (base::Value::Dict* matched = FindNodeByGuid(*children, needle_guid)) {
        return matched;
      }
    }
  }
  return nullptr;
}

base::Value::Dict* MenuUpgrade::FindNodeByAction(
    base::Value::List& list,
    bool include_children,
    const std::string& needle_action) {
  for (auto& item : list) {
    base::Value::Dict* dict = item.GetIfDict();
    if (!dict)
      continue;

    const std::string* action = dict->FindString("action");
    const std::string* guid = dict->FindString("guid");
    if (!action || !guid)
      continue;
    if (*action == needle_action) {
      return dict;
    }
    if (include_children) {
      if (base::Value::List* children = dict->FindList("children")) {
        if (base::Value::Dict* matched =
                FindNodeByAction(*children, include_children, needle_action)) {
          return matched;
        }
      }
    }
  }
  return nullptr;
}

bool MenuUpgrade::AddFromBundle(const base::Value::Dict& bundle_dict,
                                const std::string& parent_guid,
                                int bundle_index,
                                base::Value::List& profile_list) {
  const std::string* guid = bundle_dict.FindString("guid");
  if (guid && !IsDeleted(*guid)) {
    if (!FindNodeByGuid(profile_list, *guid)) {
      // Any children of bundle_dict must be added recursivly so that we can
      // test if they should be added.
      if (bundle_dict.FindList("children")) {
        base::Value::Dict copy(bundle_dict.Clone());
        base::Value::List empty;
        copy.Set("children", std::move(empty));
        if (!Insert(copy, parent_guid, bundle_index, profile_list)) {
          return false;
        }
      } else {
        if (!Insert(bundle_dict, parent_guid, bundle_index, profile_list)) {
          return false;
        }
      }
    }
    bundle_index = 0;
    if (const base::Value::List* children = bundle_dict.FindList("children")) {
      for (auto& child : *children) {
        if (child.is_dict()) {
          if (!AddFromBundle(child.GetDict(), *guid, bundle_index,
                             profile_list)) {
            return false;
          }
        }
        bundle_index ++;
      }
    }
  }

  return true;
}


bool MenuUpgrade::RemoveFromProfile(const base::Value::Dict& profile_dict,
                                    const std::string& parent_guid,
                                    base::Value::List& bundle_list,
                                    base::Value::List& profile_list) {
  const std::string* guid = profile_dict.FindString("guid");
  if (!guid) {
    return false;
  }

  int origin = profile_dict.FindInt("origin").value_or(Menu_Node::BUNDLE);
  if (origin == Menu_Node::BUNDLE) {
    if (!FindNodeByGuid(bundle_list, *guid)) {
      return Remove(profile_dict, parent_guid, profile_list);
    }
  } else if (origin == Menu_Node::MODIFIED_BUNDLE) {
    if (IsDeleted(*guid)) {
      // Modified (not added) item. If it has been deleted from the bundle it
      // should be removed as well in profile. Note, we do not do this if user
      // has added an element, only modified it.
      if (!FindNodeByGuid(bundle_list, *guid)) {
        std::string guid_copy(*guid); // Points to data that will be removed.
        bool success = Remove(profile_dict, parent_guid, profile_list);
        if (success) {
          PruneDeleted(guid_copy);
        }
        return success;
      }
    } else {
      // This means the item has gotten a new guid (which we did intentionally
      // at an early stage of development). We do not want that.
      needs_fixup_ = true;
    }
  }
  if (const base::Value::List* children = profile_dict.FindList("children")) {
    for (size_t i = children->size(); i != 0;) {
      --i;
      if (const base::Value::Dict* child = (*children)[i].GetIfDict()) {
        if (!RemoveFromProfile(*child, *guid, bundle_list, profile_list)) {
          return false;
        }
        // Remove() in the recursive call to RemoveFromProfile() can remove more
        // than one item in case the item to be removed has a guid that is
        // (wrongly) duplicated in the installed file. That was the case when we
        // upgraded to 3.8. For simplicity, instead of returning the delete
        // count, we check the list size.
        if (i > children->size()) {
          i = children->size();
        }
      }
    }
  }

  return true;
}

bool MenuUpgrade::FixupProfile(base::Value::Dict& profile_dict,
                               const std::string& parent_guid,
                               base::Value::List& bundle_root,
                               base::Value::List& profile_root,
                               const std::string& menu_action) {
  const std::string* guid = profile_dict.FindString("guid");
  if (!guid) {
    return false;
  }

  int origin = profile_dict.FindInt("origin").value_or(Menu_Node::BUNDLE);
  if (origin == Menu_Node::MODIFIED_BUNDLE && !IsDeleted(*guid)) {
    // We used to reassign the guid when modfying an item. We get here because
    // the guid in question is not in the deleted list (which contains guids
    // for modified or deleted bundled items). For sync this is a problem as
    // it can trigger duplicates. We try to undo the guid change here.
    //
    // Get the action of the item and look it up in the bundled menu. An
    // action is only used once in the bundled menu with one exception
    // (COMMAND_SHOW_BOOKMARKS) so we first find the folder and next the item
    // in that folder because of that exception.
    const std::string* action = profile_dict.FindString("action");
    if (!action) {
      return false;
    }
    base::Value::Dict* menu = FindNodeByAction(bundle_root, true, menu_action);
    if (!menu) {
      return false;
    }

    base::Value::Dict* folder = FindNodeByGuid(*menu, parent_guid);
    if (folder) {
      if (auto* children = folder->FindList("children")) {
        base::Value::Dict* match = FindNodeByAction(*children, false, *action);
        if (match) {
          const std::string* match_guid = match->FindString("guid");
          if (match_guid && IsDeleted(*match_guid)) {
            // We now have the bundled guid for the item and we know it is in
            // the modified list. Let the profile cunterpart use this guid
            // once again.
            profile_dict.Set("guid", *match_guid);
          }
        }
      }
    }
  }

  if (base::Value::List* children = profile_dict.FindList("children")) {
    for (size_t i = children->size(); i != 0;) {
      --i;
      if (base::Value::Dict* item_dict = (*children)[i].GetIfDict()) {
        if (!FixupProfile(*item_dict, *guid, bundle_root, profile_root,
                          menu_action)) {
          return false;
        }
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

bool MenuUpgrade::PruneDeleted(const std::string& guid) {
  for (int i = deleted_.size() - 1; i >= 0; i--) {
    if (guid == deleted_[i]) {
      deleted_.erase(deleted_.begin() + i);
      return true;
    }
  }
  return false;
}


// Inserts 'bundle_dict' into the profile list. 'bundle_dict' is assumed to not
// exist (guid wise) in the profile list.
bool MenuUpgrade::Insert(const base::Value::Dict& bundle_dict,
                         const std::string& parent_guid,
                         int index,
                         base::Value::List& profile_list) {
  if (parent_guid.empty()) {
    // Special case for top level. Index is not imporant.
    profile_list.Append(bundle_dict.Clone());
    return true;
  }

  base::Value::Dict* matched = FindNodeByGuid(profile_list, parent_guid);
  if (matched) {
    base::Value* children = matched->Find("children");
    if (children && children->is_list()) {
      base::Value::List list;
      int i = 0;
      bool added = false;
      for (auto& child : children->GetList()) {
        if (i++ == index) {
          added = true;
          list.Append(bundle_dict.Clone());
        }
        list.Append(std::move(child));
      }
      if (!added) {
        list.Append(bundle_dict.Clone());
      }
      matched->Set("children", std::move(list));
      return true;
    }
  }

  return false;
}

namespace {

size_t EraseDictionaryFromList(const base::Value::Dict& dict,
                               base::Value::List& list) {
  return list.EraseIf(
      [&](base::Value& v) { return v.is_dict() && v.GetDict() == dict; });
}

}  // namespace

bool MenuUpgrade::Remove(const base::Value::Dict& profile_dict,
                         const std::string& parent_guid,
                         base::Value::List& profile_list) {
  if (parent_guid.empty()) {
    // An entire menu.
    const std::string* guid = profile_dict.FindString("guid");
    if (!guid) {
      return false;
    }
    for (size_t i = profile_list.size(); i != 0;) {
      --i;
      // Not all toplevel items have a guid so test it exists before matching.
      const auto& item = profile_list[i];
      const std::string* item_guid = item.GetDict().FindString("guid");
      if (item_guid && *item_guid == *guid) {
        // We used to return true when excatly one element was removed, but
        // due to some duplicate ids that got added to a bundled file, some
        // in the same sub menu, we now accept that we can delete more than
        // one.
        return EraseDictionaryFromList(profile_dict, profile_list) >= 1;
      }
    }
  } else {
    // A folder or item within a menu.
    base::Value::Dict* matched = FindNodeByGuid(profile_list, parent_guid);
    if (matched) {
      base::Value::List* children = matched->FindList("children");
      if (children) {
        // We used to return true when exactly one element was removed, but
        // due to some duplicate ids that got added to a bundled file, some
        // in the same sub menu, we now accept that we can delete more than one.
        return EraseDictionaryFromList(profile_dict, *children) >= 1;
      }
    }
  }
  return false;
}

}  // namespace menus
