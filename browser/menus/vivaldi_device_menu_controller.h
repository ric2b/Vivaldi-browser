//
// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef BROWSER_MENUS_VIVALDI_DEVICE_MENU_CONTROLLER_H_
#define BROWSER_MENUS_VIVALDI_DEVICE_MENU_CONTROLLER_H_

#include "ui/base/models/simple_menu_model.h"

class Browser;

namespace vivaldi {

class VivaldiRenderViewContextMenu;

class DeviceMenuController {
 public:
  enum Mode {
    kPage,
    kLink,
  };

  DeviceMenuController(VivaldiRenderViewContextMenu* rv_context_menu,
                       Mode mode);
  ~DeviceMenuController();

  void Populate(Browser* browser,
                std::u16string label,
                ui::SimpleMenuModel* menu_model,
                ui::SimpleMenuModel::Delegate* delegate);
  bool HandleCommand(int command_id, int event_flags);

 private:
  const raw_ptr<VivaldiRenderViewContextMenu> rv_context_menu_;
  Mode mode_;
};

}  // namespace vivaldi

#endif  // BROWSER_MENUS_VIVALDI_DEVICE_MENU_CONTROLLER_H_
