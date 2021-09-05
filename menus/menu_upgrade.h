// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef MENUS_MENU_UPGRADE_H_
#define MENUS_MENU_UPGRADE_H_

#include <vector>

namespace base {
class FilePath;
class Value;
}

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
  base::Value* FindControlNode(base::Value& value);

  // Examines all nodes starting with 'value' and returns the one using
  // 'needle_guid'.
  base::Value* FindNodeByGuid(base::Value& value,
                              const std::string& needle_guid);

  // Examines all nodes starting with 'value' and returns the one using
  // 'needle_action'.
  base::Value* FindNodeByAction(base::Value& value,
                                bool include_children,
                                const std::string& needle_action);

  // Examines all elements starting with 'bundle_value' and adds them to the
  // profile tree if not already present and not deleted.
  bool AddFromBundle(const base::Value& bundle_value,
                     const std::string& parent_guid,
                     int bundle_index,
                     base::Value* profile_root);
  // Examines all elements starting with 'profile_value' and removed them from
  // the profile tree if not present in the bundled tree and is not a custom
  // element.
  bool RemoveFromProfile(const base::Value& profile_value,
                         const std::string& parent_guid,
                         base::Value* bundle_root,
                         base::Value* profile_root);

  // Modifies the profile based file to undo behavior we had for the first
  // releases (including 3.2).
  bool FixupProfile(base::Value& profile_value,
                    const std::string& parent_guid,
                    base::Value* bundle_root,
                    base::Value* profile_root,
                    const std::string& menu_action);

  // Returns true if the guid is registered as a deleted or modified element.
  bool IsDeletedOrModified(const std::string& guid);
  // Adds the 'bundle_value' to the profile tree as a child of the node using
  // 'parent_guid' at the given index or at the end if the child list.
  bool Insert(const base::Value& bundle_value,
              const std::string& parent_guid,
              int index,
              base::Value* profile_root);
  // Removes the 'profile_value' from the profile tree. It must be a child of
  // the of the node using  'parent_guid'
  bool Remove(const base::Value& profile_value,
              const std::string& parent_guid,
              base::Value* profile_root);

  std::vector<std::string> deleted_;
  bool needs_fixup_ = false;
};

}  // namespace menus

#endif  // MENUS_MENU_UPGRADE_H_
