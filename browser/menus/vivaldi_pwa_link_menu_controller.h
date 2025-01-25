//
// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef BROWSER_MENUS_VIVALDI_PWA_LINK_MENU_CONTROLLER_H_
#define BROWSER_MENUS_VIVALDI_PWA_LINK_MENU_CONTROLLER_H_

#include "ui/menus/simple_menu_model.h"

class Browser;
class Profile;

namespace content {
struct ContextMenuParams;
}

namespace vivaldi {

class VivaldiRenderViewContextMenu;

// PWA: Progressive Web App.
class PWALinkMenuController {
 public:
  PWALinkMenuController(VivaldiRenderViewContextMenu* rv_context_menu,
                        Profile* active_profile);
  ~PWALinkMenuController();

  void Populate(Browser* browser,
                std::u16string label,
                ui::SimpleMenuModel* menu_model);

 private:
  const raw_ptr<VivaldiRenderViewContextMenu> rv_context_menu_;
  const raw_ptr<Profile> profile_;
};

}  // namespace vivaldi

#endif  // BROWSER_MENUS_VIVALDI_LINK_PWA_MENU_CONTROLLER_H_