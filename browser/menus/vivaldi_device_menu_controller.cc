//
// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "browser/menus/vivaldi_device_menu_controller.h"

#include "browser/menus/vivaldi_render_view_context_menu.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_desktop_util.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_sub_menu_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace vivaldi {

DeviceMenuController::DeviceMenuController(
    VivaldiRenderViewContextMenu* rv_context_menu, Mode mode)
  :rv_context_menu_(rv_context_menu),
   mode_(mode) {
}

DeviceMenuController::~DeviceMenuController() {}

void DeviceMenuController::Populate(Browser* browser,
                                    base::string16 label,
                                    ui::SimpleMenuModel* menu_model,
                                    ui::SimpleMenuModel::Delegate* delegate) {
  if (mode_ == kPage) {
    if (send_tab_to_self::GetValidDeviceCount(browser->profile()) == 0) {
#if defined(OS_MAC)
      menu_model->AddItem(IDC_SEND_TAB_TO_SELF_SINGLE_TARGET,
                          l10n_util::GetStringFUTF16(
                              IDS_CONTEXT_MENU_SEND_TAB_TO_SELF_SINGLE_TARGET,
                              send_tab_to_self::GetSingleTargetDeviceName(
                                 browser->profile())));
#else
      menu_model->AddItemWithIcon(
          IDC_SEND_TAB_TO_SELF_SINGLE_TARGET,
          l10n_util::GetStringFUTF16(
              IDS_CONTEXT_MENU_SEND_TAB_TO_SELF_SINGLE_TARGET,
              send_tab_to_self::GetSingleTargetDeviceName(
                  browser->profile())),
          ui::ImageModel::FromVectorIcon(kSendTabToSelfIcon));
#endif
    } else {
      sub_menu_model_ =
          std::make_unique<send_tab_to_self::SendTabToSelfSubMenuModel>(
              browser->tab_strip_model()->GetActiveWebContents(),
              send_tab_to_self::SendTabToSelfMenuType::kContent);
#if defined(OS_MAC)
      menu_model->AddSubMenu(IDC_SEND_TAB_TO_SELF, label,
          sub_menu_model_.get());
#else
      menu_model->AddSubMenu(IDC_SEND_TAB_TO_SELF, label,
          sub_menu_model_.get());
      menu_model->SetIcon(menu_model->GetIndexOfCommandId(IDC_SEND_TAB_TO_SELF),
          ui::ImageModel::FromVectorIcon(kSendTabToSelfIcon));
#endif
    }
  } else if (mode_ == kLink) {
    if (send_tab_to_self::GetValidDeviceCount(browser->profile()) == 1) {
#if defined(OS_MAC)
      menu_model->AddItem(IDC_CONTENT_LINK_SEND_TAB_TO_SELF_SINGLE_TARGET,
                          l10n_util::GetStringFUTF16(
                              IDS_LINK_MENU_SEND_TAB_TO_SELF_SINGLE_TARGET,
                              send_tab_to_self::GetSingleTargetDeviceName(
                                  browser->profile())));
#else
      menu_model->AddItemWithIcon(
          IDC_CONTENT_LINK_SEND_TAB_TO_SELF_SINGLE_TARGET,
          l10n_util::GetStringFUTF16(
              IDS_LINK_MENU_SEND_TAB_TO_SELF_SINGLE_TARGET,
              send_tab_to_self::GetSingleTargetDeviceName(browser->profile())),
          ui::ImageModel::FromVectorIcon(kSendTabToSelfIcon));
#endif
    } else {
      sub_menu_model_ =
          std::make_unique<send_tab_to_self::SendTabToSelfSubMenuModel>(
              browser->tab_strip_model()->GetActiveWebContents(),
              send_tab_to_self::SendTabToSelfMenuType::kLink,
              rv_context_menu_->GetLinkUrl());
#if defined(OS_MAC)
      menu_model->AddSubMenu(IDC_CONTENT_LINK_SEND_TAB_TO_SELF, label,
          sub_menu_model_.get());
#else
      menu_model->AddSubMenu(IDC_CONTENT_LINK_SEND_TAB_TO_SELF, label,
          sub_menu_model_.get());
      menu_model->SetIcon(
          menu_model->GetIndexOfCommandId(IDC_CONTENT_LINK_SEND_TAB_TO_SELF),
          ui::ImageModel::FromVectorIcon(kSendTabToSelfIcon));
#endif
    }
  }
}

bool DeviceMenuController::HandleCommand(int command_id, int event_flags) {
  switch(command_id) {
    case IDC_SEND_TAB_TO_SELF_SINGLE_TARGET:
      if (mode_ == kPage) {
        rv_context_menu_->ExecuteCommand(command_id, event_flags);
        return true;
      }
      break;
    case IDC_CONTENT_LINK_SEND_TAB_TO_SELF_SINGLE_TARGET:
      if (mode_ == kLink) {
        rv_context_menu_->ExecuteCommand(command_id, event_flags);
        return true;
      }
      break;
  }
  return false;
}

}  // namespace vivaldi
