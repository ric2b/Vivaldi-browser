// Copyright (c) 2016 Vivaldi. All rights reserved.

#ifndef COMPONENTS_NOTES_NOTES_SUBMENU_OBSERVER_H_
#define COMPONENTS_NOTES_NOTES_SUBMENU_OBSERVER_H_

#include <map>
#include <vector>

#include "browser/menus/vivaldi_menu_enums.h"
#include "components/notes/note_node.h"
#include "components/notes/notes_submenu_observer_helper.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "ui/menus/simple_menu_model.h"

class RenderViewContextMenuProxy;

// A class that implements the 'Insert Note' to text area options submenu.
// This class creates the submenu, adds it to the parent menu,
// and handles events.
class NotesSubMenuObserver : public RenderViewContextMenuObserver {
 public:
  NotesSubMenuObserver(
      RenderViewContextMenuProxy* proxy,
      RenderViewContextMenuBase::ToolkitDelegate* toolkit_delegate);
  ~NotesSubMenuObserver() override;
  NotesSubMenuObserver(const NotesSubMenuObserver&) = delete;
  NotesSubMenuObserver& operator=(const NotesSubMenuObserver&) = delete;

  // For configurable menus. Allows placement of the notes tree at a non-std
  // location in the menu tree.
  void SetRootModel(ui::SimpleMenuModel* model, int id, bool is_folder);

  // RenderViewContextMenuObserver implementation.
  void InitMenu(const content::ContextMenuParams& params) override;
  bool IsCommandIdSupported(int command_id) override;
  bool IsCommandIdChecked(int command_id) override;
  bool IsCommandIdEnabled(int command_id) override;
  void ExecuteCommand(int command_id) override;

  void RootMenuWillOpen();
  void PopulateModel(ui::SimpleMenuModel* model);
  ui::SimpleMenuModel* get_root_model() {
    return root_menu_model_ ? root_menu_model_.get() : models_.front().get();
  }
  int GetRootId();

 private:
  typedef std::map<ui::MenuModel*, const vivaldi::NoteNode*>
      MenuModelToNotesMap;
  typedef std::map<int, bool> IdToBoolMap;

  std::unique_ptr<NotesSubMenuObserverHelper> helper_;
  // The interface for adding a submenu to the parent.
  const raw_ptr<RenderViewContextMenuProxy> proxy_;
  raw_ptr<ui::SimpleMenuModel> root_menu_model_ = nullptr;
  bool root_is_folder_ = false;
  // Command id of element inserted into the parent menu
  int root_id_;
  int min_notes_id_ = 0;
  int max_notes_id_ = 0;
  std::vector<std::unique_ptr<ui::SimpleMenuModel>> models_;
  MenuModelToNotesMap menumodel_to_note_map_;
  IdToBoolMap root_id_map_;
};

#endif  // COMPONENTS_NOTES_NOTES_SUBMENU_OBSERVER_H_
