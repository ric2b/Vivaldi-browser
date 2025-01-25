#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_controller_delegate.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"

static bool FilterSynthesizedMoveEvent = false;

namespace views {

// static
bool MenuController::vivaldi_compact_layout_ = false;
// static
bool MenuController::vivaldi_context_menu_ = false;

// static
void MenuController::VivaldiSetCompactLayout(bool compact_layout) {
  vivaldi_compact_layout_ = compact_layout;
}

// static
bool MenuController::VivaldiGetCompactLayout() {
  return vivaldi_compact_layout_;
}

// The MenuController has a context menu flag, but it is only set when the menu
// is about to open. We need this information a bit earlier so we have a
// separate flag.
// static
void MenuController::VivaldiSetContextMenu(bool context_menu) {
  vivaldi_context_menu_ = context_menu;
}

// static
bool MenuController::VivaldiGetContextMenu() {
  return vivaldi_context_menu_;
}

void MenuController::VivaldiAdjustMenubarMenuGeometry(
    gfx::Rect* menu_bounds,
    const gfx::Rect& monitor_bounds,
    const gfx::Rect& anchor_bounds) {
  // Adjust x to avoid horizontal clipping
  if (menu_bounds->right() > monitor_bounds.right())
    menu_bounds->set_x(monitor_bounds.right() - menu_bounds->width());
  // Adjust y to use area with most available space.
  int above = anchor_bounds.y() - monitor_bounds.y();
  int below = monitor_bounds.bottom() - anchor_bounds.bottom();
  if (above > below) {
    menu_bounds->set_y(monitor_bounds.y());
    menu_bounds->set_height(above);
  } else {
    menu_bounds->set_y(anchor_bounds.bottom());
    menu_bounds->set_height(monitor_bounds.bottom() - menu_bounds->y());
  }
}

// Wrapper for access to private function
void MenuController::VivaldiOpenMenu(MenuItemView* item) {
  SetSelection(item, views::MenuController::SELECTION_OPEN_SUBMENU |
                         views::MenuController::SELECTION_UPDATE_IMMEDIATELY);
}

bool MenuController::VivaldiHandleKeyPressed(ui::KeyboardCode key_code) {
  MenuItemView* item = pending_state_.item;
  DCHECK(item);
  if (!item) {
    return false;
  };

  if ((key_code == ui::VKEY_RIGHT && base::i18n::IsRTL()) ||
      (key_code == ui::VKEY_LEFT && !base::i18n::IsRTL())) {
    // Focus is sometimes missing in a newly open submenu and in that case
    // the parent handles it. Note that we test for GetParentMenuItem() as
    // this function is used for a menubar as well.
    if (item->HasSubmenu() && item->SubmenuIsShowing() &&
        item->GetParentMenuItem()) {
      CloseSubmenu();
      return true;
    }
    // Menubar navigation
    if (!item->GetParentMenuItem() ||
        item->GetParentMenuItem() == item->GetRootMenuItem()) {
      VivaldiStepSiblingMenu(false);
      return true;
    }
  } else if ((key_code == ui::VKEY_RIGHT && !base::i18n::IsRTL()) ||
             (key_code == ui::VKEY_LEFT && base::i18n::IsRTL())) {
    // Menubar navigation
    if ((!item->HasSubmenu() || item == item->GetRootMenuItem()) &&
        (!item->GetParentMenuItem() ||
         item->GetParentMenuItem() == item->GetRootMenuItem())) {
      VivaldiStepSiblingMenu(true);
      return true;
    }
  }
  return false;
}

// To be used for navigating a menu bar using the keyboard
bool MenuController::VivaldiStepSiblingMenu(bool next) {
  if (!menu_stack_.empty()) {
    return false;
  }

  SubmenuView* source =
      pending_state_.item->HasSubmenu()
          ? pending_state_.item->GetSubmenu()
          : pending_state_.item->GetParentMenuItem()
                ? pending_state_.item->GetParentMenuItem()->GetSubmenu()
                : nullptr;
  if (!source) {
    return false;
  }

  gfx::Rect rect;
  bool has_mnemonics;
  views::MenuAnchorPosition anchor;
  MenuItemView* alt_menu =
      source->GetMenuItem()->GetDelegate()->GetNextSiblingMenu(
          next, &has_mnemonics, &rect, &anchor);
  if (!alt_menu || (state_.item && state_.item->GetRootMenuItem() == alt_menu))
    return false;
  delegate_->SiblingMenuCreated(alt_menu);
  did_capture_ = false;
  UpdateInitialLocation(rect, anchor, false);
  alt_menu->PrepareForRun(
      has_mnemonics,
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
bool MenuController::VivaldiHandleSynthesizedEvent(const ui::MouseEvent& event) {
  if (FilterSynthesizedMoveEvent) {
    if (event.IsSynthesized()) {
      return true;
    }
    FilterSynthesizedMoveEvent = false;
  }
  return false;
}

}  // namespace views