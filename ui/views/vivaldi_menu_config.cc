#include "ui/views/vivaldi_menu_config.h"

#include "ui/views/controls/menu/menu_config.h"

namespace vivaldi {

// Currently only in use for Mac
void OverrideMenuConfig(views::MenuConfig* config) {
  config->minimum_text_item_height = 20;
  config->item_top_margin = 1;
  config->item_bottom_margin = 1;
}

} // vivaldi