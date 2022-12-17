// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "ui/views/controls/menu/menu_model_adapter.h"

#include "ui/base/models/menu_model.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"

namespace views {

void MenuModelAdapter::VivaldiSetModel(MenuItemView* menu,
                                       ui::MenuModel* model) {
  menu_map_[menu] = model;
}

void MenuModelAdapter::VivaldiUpdateMenu(MenuItemView* menu,
                                         ui::MenuModel* model) {
  // Clear the menu.
  if (menu->HasSubmenu()) {
    for (auto* subitem : menu->GetSubmenu()->GetMenuItems())
      menu->RemoveMenuItem(subitem);
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

void MenuModelAdapter::VivaldiSelectionChanged(MenuItemView* menu) {
  // Ignore selection of the root menu.
  if (menu == menu->GetRootMenuItem())
    return;

  const int id = menu->GetCommand();
  ui::MenuModel* model = menu_model_;
  size_t index = 0;
  if (ui::MenuModel::GetModelAndIndexForCommandId(id, &model, &index)) {
    model->VivaldiHighlightChangedTo(index);
    return;
  }

  NOTREACHED();
}

}  // namespace views
