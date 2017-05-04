// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "update_notifier/native_menu.h"

#include <string>

namespace vivaldi_update_notifier {

NativeMenu::NativeMenu() : menu_(NULL), displayed_(false) {
  Reset();
}

NativeMenu::~NativeMenu() {
  DestroyMenu(menu_);
}

void NativeMenu::Reset() {
  DestroyMenu(menu_);
  menu_item_strings_.clear();
  menu_ = CreatePopupMenu();
}

bool NativeMenu::AppendSeparator() {
  MENUITEMINFO menu_item_info = {};
  menu_item_info.cbSize = sizeof(menu_item_info);
  menu_item_info.fMask = MIIM_FTYPE;
  menu_item_info.fType = MFT_SEPARATOR;
  return InsertMenuItem(menu_, GetMenuItemCount(menu_), TRUE,
                        &menu_item_info) == TRUE;
}

bool NativeMenu::AppendStringMenuItem(const base::string16& string,
                                      UINT state,
                                      UINT item_id) {
  menu_item_strings_.push_back(string);

  MENUITEMINFO menu_item_info = {};
  menu_item_info.cbSize = sizeof(menu_item_info);
  menu_item_info.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING | MIIM_STATE;
  menu_item_info.fType = MFT_STRING;
  menu_item_info.fState = state;
  menu_item_info.wID = item_id;
  menu_item_info.dwTypeData = &menu_item_strings_.back()[0];
  return InsertMenuItem(menu_, GetMenuItemCount(menu_), TRUE,
                        &menu_item_info) == TRUE;
}

void NativeMenu::ShowMenu(int x, int y, HWND hwnd) {
  displayed_ = true;
  TrackPopupMenuEx(menu_, 0, x, y, hwnd, NULL);
  displayed_ = false;
}
}  // namespace vivaldi_update_notifier
