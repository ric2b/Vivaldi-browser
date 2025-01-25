//
// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef BROWSER_MENUS_VIVALDI_DEVICE_MENU_CONTROLLER_H_
#define BROWSER_MENUS_VIVALDI_DEVICE_MENU_CONTROLLER_H_

#include "ui/menus/simple_menu_model.h"
#include "url/gurl.h"

class Browser;


namespace content {
struct ContextMenuParams;
}

namespace vivaldi {

class VivaldiRenderViewContextMenu;

class DeviceMenuController {
 public:
  DeviceMenuController(VivaldiRenderViewContextMenu* rv_context_menu,
                       GURL& url, std::string url_title);
  ~DeviceMenuController();

  void Populate(Browser* browser,
                std::u16string label,
                const std::optional<std::vector<std::string>>& icons,
                bool dark_text_color,
                ui::SimpleMenuModel* menu_model,
                ui::SimpleMenuModel::Delegate* delegate);
  bool HandleCommand(int command_id, int event_flags);
  bool IsCommandIdEnabled(int command_id,
                          const content::ContextMenuParams& params,
                          bool* enabled);
  bool GetHighlightText(int command_id, std::string& text);
 private:
  bool GetHasInstalledDevices();

  const raw_ptr<VivaldiRenderViewContextMenu> rv_context_menu_;
  GURL url_;
  std::string url_title_;
  std::map<int, base::Time> last_updated_map_;
  Browser* browser_ = nullptr;
};

}  // namespace vivaldi

#endif  // BROWSER_MENUS_VIVALDI_DEVICE_MENU_CONTROLLER_H_
