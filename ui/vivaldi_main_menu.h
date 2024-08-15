//
// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.
//

#ifndef UI_VIVALDI_MAIN_MENU_H_
#define UI_VIVALDI_MAIN_MENU_H_

#include "extensions/schema/menubar.h"

class Profile;

namespace vivaldi {

void CreateVivaldiMainMenu(
    Profile* profile,
    std::vector<extensions::vivaldi::menubar::MenuItem>* items,
    int min_id,
    int max_id);

}  // namespace vivaldi

#endif  // UI_VIVALDI_MAIN_MENU_H_
