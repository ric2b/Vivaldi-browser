// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "ui/views/controls/menu/menu_model_adapter.h"

#include "ui/base/models/menu_model.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"

namespace views {

void MenuModelAdapter::VivaldiUpdateMenu(MenuItemView* menu,
  ui::MenuModel* model) {
  // Clear the menu.
  if (menu->HasSubmenu()) {
    const int subitem_count = menu->GetSubmenu()->child_count();
    for (int i = 0; i < subitem_count; ++i)
      menu->RemoveMenuItemAt(0);
  }

  // Leave entries in the map if the menu is being shown.  This
  // allows the map to find the menu model of submenus being closed
  // so ui::MenuModel::MenuClosed() can be called.
  if (!menu->GetMenuController())
    menu_map_.clear();
  menu_map_[menu] = model;

  // Repopulate the menu.
  BuildMenuImpl(menu, model);
}

}  // namespace views
