// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef MENUS_MENU_MODEL_H_
#define MENUS_MENU_MODEL_H_

#include <memory>
#include <set>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "menus/menu_model_observer.h"
#include "menus/menu_node.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace menus {

class MenuLoadDetails;
class MenuStorage;

class Menu_Model : public KeyedService {
 public:
  enum Mode { kMainMenu, kContextMenu };

  explicit Menu_Model(content::BrowserContext* context, Mode mode);
  ~Menu_Model() override;
  Menu_Model(const Menu_Model&) = delete;
  Menu_Model& operator=(const Menu_Model&) = delete;

  void Load(bool is_reset);
  bool Save();
  void LoadFinished(std::unique_ptr<MenuLoadDetails> details);

  bool Move(const Menu_Node* node, const Menu_Node* new_parent, size_t index);
  Menu_Node* Add(std::unique_ptr<Menu_Node> node,
                 Menu_Node* parent,
                 size_t index);
  bool SetTitle(Menu_Node* node, const std::u16string& title);
  bool SetParameter(Menu_Node* node, const std::string& parameter);
  bool SetShowShortcut(Menu_Node* node, bool show_shortcut);
  bool SetContainerMode(Menu_Node* node, const std::string& mode);
  bool SetContainerEdge(Menu_Node* node, const std::string& edge);
  bool Remove(Menu_Node* node);
  bool RemoveAction(Menu_Node* root, const std::string& action);
  bool Reset(const Menu_Node* node);
  bool Reset(const std::string& menu);
  bool ResetAll();

  Mode mode() const { return mode_; }
  bool loaded() const { return loaded_; }
  bool IsValidIndex(const Menu_Node* parent, size_t index);

  void AddObserver(MenuModelObserver* observer);
  void RemoveObserver(MenuModelObserver* observer);

  Menu_Node* GetMenuByResourceName(const std::string& menu);
  // Returns the parent of all fixed nodes.
  Menu_Node& root_node() { return root_; }
  // Returns the fixed node that is the ancestor of main menus.
  Menu_Node* mainmenu_node() { return mainmenu_node_; }

  Menu_Control* GetControl() { return control_.get(); }

 private:
  void RemoveBundleTag(Menu_Node* node, bool include_children);
  void ClearDeleted(const Menu_Node* node, bool include_children);
  void RemoveGuidDuplication(const Menu_Node* node);

  std::unique_ptr<MenuLoadDetails> CreateLoadDetails(const std::string& menu,
                                                     bool is_reset);
  std::unique_ptr<MenuLoadDetails> CreateLoadDetails(int64_t id/*, bool is_reset*/);
  bool loaded_ = false;
  base::ObserverList<MenuModelObserver> observers_;
  const raw_ptr<content::BrowserContext> context_;
  Mode mode_;
  std::unique_ptr<MenuStorage> store_;
  Menu_Node root_;
  // Managed by the root node. Provides easy access.
  raw_ptr<Menu_Node> mainmenu_node_ = nullptr;
  std::unique_ptr<Menu_Control> control_;
};

}  // namespace menus

#endif  // MENUS_MENU_MODEL_H_
