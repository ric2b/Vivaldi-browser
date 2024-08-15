#include "components/notes/notes_submenu_observer_helper_mac.h"

#include "components/notes/notes_submenu_observer.h"

NotesSubMenuObserverHelper* CreateSubMenuObserverHelper(
    NotesSubMenuObserver* sub_menu_observer,
    RenderViewContextMenuBase::ToolkitDelegate* toolkit_delegate) {
  return new NotesSubMenuObserverHelperMac(sub_menu_observer, toolkit_delegate);
}

NotesSubMenuObserverHelperMac::NotesSubMenuObserverHelperMac(
    NotesSubMenuObserver* sub_menu_observer,
    RenderViewContextMenuBase::ToolkitDelegate* toolkit_delegate)
    : sub_menu_observer_(sub_menu_observer) {
  // TODO(espen): Examine Mac menu code to see if possible to add delayed
  // loading support.
}

NotesSubMenuObserverHelperMac::~NotesSubMenuObserverHelperMac() {}

bool NotesSubMenuObserverHelperMac::SupportsDelayedLoading() {
  return false;
}

void NotesSubMenuObserverHelperMac::ExecuteCommand(int command_id,
                                                   int event_flags) {
  sub_menu_observer_->ExecuteCommand(command_id);
}

void NotesSubMenuObserverHelperMac::OnMenuWillShow(
    ui::SimpleMenuModel* menu_model) {
  if (!SupportsDelayedLoading()) {
    return;
  }
}
