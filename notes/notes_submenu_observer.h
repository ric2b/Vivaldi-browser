// Copyright (c) 2016 Vivaldi. All rights reserved.

#ifndef NOTES_NOTES_SUBMENU_OBSERVER_H_
#define NOTES_NOTES_SUBMENU_OBSERVER_H_

#include <map>
#include <vector>

#include "browser/menus/vivaldi_menu_enums.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "notes/notesnode.h"
#include "notes/notes_submenu_observer_helper.h"
#include "ui/base/models/simple_menu_model.h"

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

  // RenderViewContextMenuObserver implementation.
  void InitMenu(const content::ContextMenuParams& params) override;
  bool IsCommandIdSupported(int command_id) override;
  bool IsCommandIdChecked(int command_id) override;
  bool IsCommandIdEnabled(int command_id) override;
  void ExecuteCommand(int command_id) override;

  void PopulateModel(ui::SimpleMenuModel* model);
  ui::SimpleMenuModel* get_root_model() { return models_.front().get(); }
  int get_root_id() { return root_id_; }

private:
  typedef std::map<ui::MenuModel*, vivaldi::Notes_Node*> MenuModelToNotesMap;

  std::unique_ptr<NotesSubMenuObserverHelper> helper_;
  // The interface for adding a submenu to the parent.
  RenderViewContextMenuProxy* proxy_;
  // Command id of element inserted into the parent menu
  int root_id_;
  int min_notes_id_ = 0;
  int max_notes_id_ = 0;
  std::vector<std::unique_ptr<ui::SimpleMenuModel>> models_;
  MenuModelToNotesMap menumodel_to_note_map_;

  DISALLOW_COPY_AND_ASSIGN(NotesSubMenuObserver);
};

#endif  // NOTES_NOTES_SUBMENU_OBSERVER_H_
