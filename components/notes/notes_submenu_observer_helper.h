// Copyright (c) 2019 Vivaldi. All rights reserved.

#ifndef COMPONENTS_NOTES_NOTES_SUBMENU_OBSERVER_HELPER_H_
#define COMPONENTS_NOTES_NOTES_SUBMENU_OBSERVER_HELPER_H_

#include <map>

#include "components/renderer_context_menu/render_view_context_menu_base.h"
#include "ui/menus/simple_menu_model.h"

class NotesSubMenuObserver;
class NotesSubMenuObserverHelper;

NotesSubMenuObserverHelper* CreateSubMenuObserverHelper(
    NotesSubMenuObserver* sub_menu_observer,
    RenderViewContextMenuBase::ToolkitDelegate* toolkit_delegate);

class NotesSubMenuObserverHelper : public ui::SimpleMenuModel::Delegate {
 public:
  NotesSubMenuObserverHelper() = default;
  ~NotesSubMenuObserverHelper() override = default;

  virtual bool SupportsDelayedLoading() = 0;

  // SimpleMenuModel::Delegate
  void ExecuteCommand(int command_id, int event_flags) override {}
};

#endif  // COMPONENTS_NOTES_NOTES_SUBMENU_OBSERVER_HELPER_H_