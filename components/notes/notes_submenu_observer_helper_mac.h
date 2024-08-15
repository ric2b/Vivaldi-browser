// Copyright (c) 2019 Vivaldi. All rights reserved.

#ifndef COMPONENTS_NOTES_NOTES_SUBMENU_OBSERVER_HELPER_MAC_H_
#define COMPONENTS_NOTES_NOTES_SUBMENU_OBSERVER_HELPER_MAC_H_

#include <map>

#include "components/notes/notes_submenu_observer_helper.h"

class NotesSubMenuObserver;

// Helper class for NotesSubMenuObserver. Views specific
class NotesSubMenuObserverHelperMac : public NotesSubMenuObserverHelper {
 public:
  NotesSubMenuObserverHelperMac(
      NotesSubMenuObserver* sub_menu_observer,
      RenderViewContextMenuBase::ToolkitDelegate* toolkit_delegate);
  ~NotesSubMenuObserverHelperMac() override;
  NotesSubMenuObserverHelperMac(const NotesSubMenuObserverHelperMac&) = delete;
  NotesSubMenuObserverHelperMac& operator=(
      const NotesSubMenuObserverHelperMac&) = delete;

  bool SupportsDelayedLoading() override;

  // SimpleMenuModel::Delegate via NotesSubMenuObserverHelper
  void ExecuteCommand(int command_id, int event_flags) override;
  void OnMenuWillShow(ui::SimpleMenuModel* source) override;

 private:
  NotesSubMenuObserver* sub_menu_observer_;
};

#endif  // COMPONENTS_NOTES_NOTES_SUBMENU_OBSERVER_HELPER_MAC_H_