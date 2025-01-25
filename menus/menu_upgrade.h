// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef MENUS_MENU_UPGRADE_H_
#define MENUS_MENU_UPGRADE_H_

#include <vector>

#include "base/values.h"

namespace base {
class FilePath;
class Value;
}  // namespace base

namespace menus {

class MenuUpgrade {
 public:
  MenuUpgrade();
  ~MenuUpgrade();

  std::unique_ptr<base::Value> Run(const base::FilePath& profile_file,
                                   const base::FilePath& bundled_file,
                                   const std::string& version);

 private:
  // Returns the control node in the tree starting with 'value'
  base::Value* FindControlNode(base::Value::List& list);

  // Examines all nodes in `list` recursively and returns the one using
  // 'needle_guid'.
  base::Value::Dict* FindNodeByGuid(base::Value::List& list,
                                    const std::string& needle_guid);

  // Examines all nodes starting with `dict` and returns the one using
  // 'needle_guid'.
  base::Value::Dict* FindNodeByGuid(base::Value::Dict& dict,
                                    const std::string& needle_guid);

  // Examines all nodes starting with 'value' and returns the one using
  // 'needle_action'.
  base::Value::Dict* FindNodeByAction(base::Value::List& list,
                                      bool include_children,
                                      const std::string& needle_action);

  // Examines all elements starting with 'bundle_dict' and adds them to the
  // profile tree if not already present and not deleted.
  bool AddFromBundle(const base::Value::Dict& bundle_dict,
                     const std::string& parent_guid,
                     int bundle_index,
                     base::Value::List& profile_list);
  // Examines all elements starting with 'profile_dict' and removed them from
  // the profile tree if not present in the bundled tree and is not a custom
  // element.
  bool RemoveFromProfile(const base::Value::Dict& profile_dict,
                         const std::string& parent_guid,
                         base::Value::List& bundle_list,
                         base::Value::List& profile_list);

  // Modifies the profile based file to undo behavior we had for the first
  // releases (including 3.2).
  bool FixupProfile(base::Value::Dict& profile_dict,
                    const std::string& parent_guid,
                    base::Value::List& bundle_root,
                    base::Value::List& profile_root,
                    const std::string& menu_action);

  // Returns true if the guid is registered as a deleted element.
  bool IsDeleted(const std::string& guid);
  // Removes guid from the deleted list. Returns true if the guid is removed.
  bool PruneDeleted(const std::string& guid);
  // Adds the 'bundle_dict' to the profile tree as a child of the node using
  // 'parent_guid' at the given index or at the end if the child list.
  bool Insert(const base::Value::Dict& bundle_dict,
              const std::string& parent_guid,
              int index,
              base::Value::List& profile_list);
  // Removes the 'profile_dict' from the profile tree. It must be a child of
  // the of the node using  'parent_guid'
  bool Remove(const base::Value::Dict& profile_dict,
              const std::string& parent_guid,
              base::Value::List& profile_list);

  std::vector<std::string> deleted_;
  bool needs_fixup_ = false;
};

}  // namespace menus

#endif  // MENUS_MENU_UPGRADE_H_
