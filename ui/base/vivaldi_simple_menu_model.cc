// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "ui/menus/simple_menu_model.h"

namespace ui {

void SimpleMenuModel::Delegate::VivaldiCommandIdHighlighted(int command_id) {}

void SimpleMenuModel::VivaldiHighlightChangedTo(size_t index) {
  if (delegate_)
    delegate_->VivaldiCommandIdHighlighted(GetCommandIdAt(index));
}

}  // namespace ui
