// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "ui/views/controls/menu/menu_delegate.h"

namespace views {

MenuItemView* MenuDelegate::GetVivaldiSiblingMenu(
    MenuItemView* menu,
    const gfx::Point& screen_point,
    gfx::Rect* rect,
    MenuAnchorPosition* anchor) {
  return nullptr;
}

MenuItemView* MenuDelegate::GetNextSiblingMenu(bool next,
                                               bool* has_mnemonics,
                                               gfx::Rect* rect,
                                               MenuAnchorPosition* anchor) {
  return nullptr;
}

bool MenuDelegate::VivaldiShouldTryPositioningInMenuBar() const {
  return false;
}

bool MenuDelegate::VivaldiShouldTryPositioningContextMenu() const {
  return false;
}

void MenuDelegate::VivaldiGetContextMenuPosition(
    gfx::Rect* menu_bounds,
    const gfx::Rect& monitor_bounds,
    const gfx::Rect& anchor_bounds) const {}




}  // namespace views