// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef VIVALDI_BROWSER_UI_VIVALDI_MAIN_MENU_BUILDER_H
#define VIVALDI_BROWSER_UI_VIVALDI_MAIN_MENU_BUILDER_H

#include "chrome/browser/app_controller_mac.h"

namespace vivaldi {
void BuildVivaldiMainMenu(NSApplication* nsapp, AppController* app_controller);
}

#endif  // VIVALDI_BROWSER_UI_VIVALDI_MAIN_MENU_BUILDER_H
