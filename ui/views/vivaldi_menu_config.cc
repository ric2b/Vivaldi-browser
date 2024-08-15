// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "ui/views/vivaldi_menu_config.h"
#include "ui/views/controls/menu/menu_config.h"

namespace vivaldi {

// Currently only in use for Mac
void OverrideMenuConfig(views::MenuConfig* config) {
  // Commented out for ch126. minimum_text_item_height does not make a
  // difference, but item_vertical_margin does (using default value is best).
  //config->minimum_text_item_height = 20;
  //config->item_vertical_margin = 1;
}

}  // namespace vivaldi