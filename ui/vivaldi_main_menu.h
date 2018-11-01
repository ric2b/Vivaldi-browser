//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//

#ifndef UI_VIVALDI_MAIN_MENU_H_
#define UI_VIVALDI_MAIN_MENU_H_

#include <string>
#include <vector>

namespace show_menu {
class MenuItem;
}

namespace vivaldi {

void CreateVivaldiMainMenu(
    Profile* profile,
    std::vector<extensions::vivaldi::show_menu::MenuItem>* items,
    const std::string& mode);

}  // namespace vivaldi

#endif  // UI_VIVALDI_MAIN_MENU_H_
