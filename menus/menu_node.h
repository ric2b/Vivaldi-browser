// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef MENUS_MENU_NODE_H_
#define MENUS_MENU_NODE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/synchronization/waitable_event.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/models/tree_node_model.h"

class Profile;

namespace base {
class Value;
class SequencedTaskRunner;
}  // namespace base

namespace menus {

class MenuStorage;
class MenuLoadDetails;

struct Menu_Control {
  Menu_Control();
  ~Menu_Control();

  std::string format;
  std::string version;
  std::vector<std::string> deleted;
};

class Menu_Node : public ui::TreeNode<Menu_Node> {
 public:
  enum Type {
    UNKNOWN = 0,
    MENU,
    COMMAND,
    CHECKBOX,
    RADIO,
    FOLDER,
    SEPARATOR,
    CONTAINER
  };
  enum Origin { BUNDLE = 0, MODIFIED_BUNDLE, USER };

  Menu_Node(const std::string& guid, int64_t id);
  ~Menu_Node() override;
  Menu_Node(const Menu_Node&) = delete;
  Menu_Node& operator=(const Menu_Node&) = delete;

  void DumpTree(int indent = 0);

  void SetType(Type type) { type_ = type; }
  Type type() const { return type_; }

  int64_t id() const { return id_; }

  // SetGuid() should only be used if we have to change guid as a result of
  // resolving a guid duplication.
  void SetGuid(std::string guid) { guid_ = guid; }
  const std::string& guid() const { return guid_; }

  void SetOrigin(Origin origin) { origin_ = origin; }
  Origin origin() const { return origin_; }

  void SetAction(std::string action) { action_ = action; }
  const std::string& action() const { return action_; }

  void SetParameter(std::string parameter) { parameter_ = parameter; }
  const std::string& parameter() const { return parameter_; }

  void SetRole(std::string role) { role_ = role; }
  const std::string& role() const { return role_; }

  void SetRadioGroup(std::string radio_group) { radio_group_ = radio_group; }
  const std::string& radioGroup() const { return radio_group_; }

  void SetContainerMode(std::string container_mode) {
    container_mode_ = container_mode;
  }
  const std::string& containerMode() const { return container_mode_; }

  void SetContainerEdge(std::string container_edge) {
    container_edge_ = container_edge;
  }
  const std::string& containerEdge() const { return container_edge_; }

  void SetHasCustomTitle(bool has_custom_title) {
    has_custom_title_ = has_custom_title;
  }
  bool hasCustomTitle() const { return has_custom_title_; }

  void SetShowShortcut(std::optional<bool> show_shortcut);
  std::optional<bool> showShortcut() const { return show_shortcut_; }

  // Returns the node in the tree of nodes that match the id.
  Menu_Node* GetById(int64_t id);
  // Returns the node in the tree of nodes that match the guid.
  Menu_Node* GetByGuid(const std::string& guid);
  // Returns the first node in the tree of nodes that match the action.
  Menu_Node* GetByAction(const std::string& action);
  // Returns the menu that this node belongs to.
  const Menu_Node* GetMenu() const;
  // Returns the menu in the node tree that matches the name.
  Menu_Node* GetMenuByResourceName(const std::string& menu);

  bool is_menu() const { return type_ == MENU; }
  bool is_command() const { return type_ == COMMAND; }
  bool is_checkbox() const { return type_ == CHECKBOX; }
  bool is_radio() const { return type_ == RADIO; }
  bool is_folder() const { return type_ == FOLDER; }
  bool is_separator() const { return type_ == SEPARATOR; }
  bool is_container() const { return type_ == CONTAINER; }

  static std::string root_node_guid() { return kRootNodeGuid; }
  static std::string mainmenu_node_guid() { return kMainmenuNodeGuid; }
  static int64_t root_node_id() { return kRootNodeId; }
  static int64_t mainmenu_node_id() { return kMainmenuNodeId; }
  static int64_t GetNewId() { return ++id_counter; }

 private:
  friend class Menu_Model;

  enum FixedId { kRootNodeId = 1, kMainmenuNodeId = 2, kFirstDynamicNodeId };
  static const char kRootNodeGuid[];
  static const char kMainmenuNodeGuid[];

  Type type_;
  Origin origin_;
  std::string title_;
  std::string role_;
  std::string action_;
  std::string parameter_;
  std::string radio_group_;
  std::string container_mode_;
  std::string container_edge_;
  std::string guid_;
  // Optional to avoid writing data to file/sync when not needed.
  std::optional<bool> show_shortcut_;
  int64_t id_;
  bool has_custom_title_ = false;

  // Next available id is copied from this value.
  static int64_t id_counter;
};

}  // namespace menus

#endif  // MENUS_MENU_NODE_H_
