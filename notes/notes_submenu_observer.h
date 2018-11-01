// Copyright (c) 2016 Vivaldi. All rights reserved.

#ifndef NOTES_NOTES_SUBMENU_OBSERVER_H_
#define NOTES_NOTES_SUBMENU_OBSERVER_H_

#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "notes/notesnode.h"
#include "ui/base/models/simple_menu_model.h"

class RenderViewContextMenuProxy;

// A class that implements the 'Insert Note' to text area options submenu.
// This class creates the submenu, adds it to the parent menu,
// and handles events.
class NotesSubMenuObserver : public RenderViewContextMenuObserver {
 public:
  NotesSubMenuObserver(RenderViewContextMenuProxy* proxy,
                       ui::SimpleMenuModel::Delegate* delegate);
  ~NotesSubMenuObserver() override;

  // RenderViewContextMenuObserver implementation.
  void InitMenu(const content::ContextMenuParams& params) override;
  bool IsCommandIdSupported(int command_id) override;
  bool IsCommandIdChecked(int command_id) override;
  bool IsCommandIdEnabled(int command_id) override;
  void ExecuteCommand(int command_id) override;

 private:
  // The interface for adding a submenu to the parent.
  RenderViewContextMenuProxy* proxy_;

  void AddMenuItems(vivaldi::Notes_Node* node, ui::SimpleMenuModel* menu_model);

  // The submenu of the 'Notes'. This class adds items to this
  // submenu and adds it to the parent menu.
  ui::SimpleMenuModel submenu_model_;
  ui::SimpleMenuModel::Delegate* delegate_;

  int min_notes_id_ = 0;
  int max_notes_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(NotesSubMenuObserver);
};

#endif  // NOTES_NOTES_SUBMENU_OBSERVER_H_
