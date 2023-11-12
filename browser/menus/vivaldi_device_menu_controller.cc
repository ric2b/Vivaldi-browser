//
// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "browser/menus/vivaldi_device_menu_controller.h"

#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

#include "browser/menus/vivaldi_render_view_context_menu.h"

namespace vivaldi {

DeviceMenuController::DeviceMenuController(
    VivaldiRenderViewContextMenu* rv_context_menu,
    Mode mode)
    : rv_context_menu_(rv_context_menu), mode_(mode) {}

DeviceMenuController::~DeviceMenuController() {}

void DeviceMenuController::Populate(Browser* browser,
                                    std::u16string label,
                                    ui::SimpleMenuModel* menu_model,
                                    ui::SimpleMenuModel::Delegate* delegate) {
  if (mode_ == kPage) {
#if BUILDFLAG(IS_MAC)
    menu_model->AddItem(
        IDC_SEND_TAB_TO_SELF,
        l10n_util::GetStringUTF16(
          IDS_MENU_SEND_TAB_TO_SELF));
#else
    menu_model->AddItemWithIcon(
        IDC_SEND_TAB_TO_SELF,
        l10n_util::GetStringUTF16(
          IDS_MENU_SEND_TAB_TO_SELF),
        ui::ImageModel::FromVectorIcon(vector_icons::kDevicesIcon));
#endif
  } else if (mode_ == kLink) {
    /*
#if BUILDFLAG(IS_MAC)
    menu_model->AddItem(
        IDC_CONTENT_LINK_SEND_TAB_TO_SELF,
        l10n_util::GetStringUTF16(
            IDS_LINK_MENU_SEND_TAB_TO_SELF));
#else
    menu_model->AddItemWithIcon(
        IDC_CONTENT_LINK_SEND_TAB_TO_SELF,
        l10n_util::GetStringFUTF16(
            IDS_LINK_MENU_SEND_TAB_TO_SELF),
        ui::ImageModel::FromVectorIcon(vector_icons::kDevicesIcon));
#endif
   */
  }
}

bool DeviceMenuController::HandleCommand(int command_id, int event_flags) {
  switch (command_id) {
    case IDC_SEND_TAB_TO_SELF:
      if (mode_ == kPage) {
        rv_context_menu_->ExecuteCommand(command_id, event_flags);
        return true;
      }
      break;
      /*
    case IDC_CONTENT_LINK_SEND_TAB_TO_SELF:
      if (mode_ == kLink) {
        rv_context_menu_->ExecuteCommand(command_id, event_flags);
        return true;
      }
      break;
      */
  }
  return false;
}

}  // namespace vivaldi
