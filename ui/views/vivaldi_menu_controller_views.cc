#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_controller_delegate.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"

static bool FilterSynthesizedMoveEvent = false;

namespace views {

// To be used for navigating a menu bar using the keyboard
bool MenuController::StepSiblingMenu(bool next) {
  if (!menu_stack_.empty()) {
    return false;
  }

  SubmenuView* source = pending_state_.item->HasSubmenu() ?
      pending_state_.item->GetSubmenu() :
          pending_state_.item->GetParentMenuItem() ?
            pending_state_.item->GetParentMenuItem()->GetSubmenu() : nullptr;
  if (!source) {
    return false;
  }

  gfx::Rect rect;
  bool has_mnemonics;
  MenuItemView* alt_menu =
    source->GetMenuItem()->GetDelegate()->GetNextSiblingMenu(
        next, &has_mnemonics, &rect);
  if (!alt_menu ||
      (state_.item && state_.item->GetRootMenuItem() == alt_menu))
    return false;
  delegate_->SiblingMenuCreated(alt_menu);
  MenuAnchorPosition anchor = views::MenuAnchorPosition::kTopLeft;
  did_capture_ = false;
  UpdateInitialLocation(rect, anchor, false);
  alt_menu->PrepareForRun(false, has_mnemonics,
      source->GetMenuItem()->GetRootMenuItem()->show_mnemonics_);
  alt_menu->controller_ = AsWeakPtr();
  SetSelection(alt_menu, SELECTION_OPEN_SUBMENU | SELECTION_UPDATE_IMMEDIATELY);

  // When navigating from one menu to another in menu bar style using the
  // keyboard, a few synthesized move events are generated after the new menu
  // opens. If the mouse cursor is inside the rect of another menu bar button
  // (a quite common pattern) then the new menu will be closed and the one
  // belonging to the hovered button will open unless we filter the events
  FilterSynthesizedMoveEvent = true;

  return true;
}

// Returns true if further event handling should be blocked.
bool MenuController::HandleSynthesizedEvent(const ui::MouseEvent& event) {
  if (FilterSynthesizedMoveEvent) {
    if (event.IsSynthesized()) {
      return true;
    }
    FilterSynthesizedMoveEvent = false;
  }
  return false;
}

}  // namespace views