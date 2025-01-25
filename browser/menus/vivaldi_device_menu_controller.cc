//
// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "browser/menus/vivaldi_device_menu_controller.h"

#include "app/vivaldi_resources.h"
#include "base/strings/utf_string_conversions.h"
#include "browser/menus/vivaldi_menu_enums.h"
#include "browser/menus/vivaldi_render_view_context_menu.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/generated_resources.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/sync_device_info/device_info.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/vivaldi_context_menu.h"

namespace vivaldi {

DeviceMenuController::DeviceMenuController(
    VivaldiRenderViewContextMenu* rv_context_menu, GURL& url,
    std::string url_title)
    : rv_context_menu_(rv_context_menu), url_(url), url_title_(url_title) {}

DeviceMenuController::~DeviceMenuController() {}

void DeviceMenuController::Populate(
    Browser* browser,
    std::u16string label,
    const std::optional<std::vector<std::string>>& icons,
    bool dark_text_color,
    ui::SimpleMenuModel* menu_model,
    ui::SimpleMenuModel::Delegate* delegate) {
  // Format Desktop, mobile, tablet. Must match order set up in JS. Incoming
  // array has two entries per image (for dark and light text colors) so there
  // must be six entries.
  ui::ImageModel images[3];

  browser_ = browser;
  send_tab_to_self::SendTabToSelfSyncService* service =
    SendTabToSelfSyncServiceFactory::GetForProfile(browser->profile());
  int index = 0;
  for (auto device :
       service->GetSendTabToSelfModel()->GetTargetDeviceInfoSortedList()) {

// for debugging
#if 0
    printf("guid: %s\n", device.cache_guid.c_str());
    printf("full: %s\n", device.full_name.c_str());
    printf("short: %s\n", device.short_name.c_str());
    printf("short: %s\n", device.device_name.c_str());
    printf("form: %d\n", device.form_factor);
    int time_in_days =
      (base::Time::Now() - device.last_updated_timestamp).InDays();
    printf("time_in_days: %d\n", time_in_days);
#endif

    int command_id = index + IDC_VIV_SEND_TO_DEVICE_FIRST;
    if (command_id <= IDC_VIV_SEND_TO_DEVICE_LAST) {
#if BUILDFLAG(IS_MAC)
      menu_model->AddItem(command_id, base::UTF8ToUTF16(device.full_name));
#else
      int idx;
      if (device.form_factor == syncer::DeviceInfo::FormFactor::kPhone) {
        idx = 1;
      } else if (
          device.form_factor == syncer::DeviceInfo::FormFactor::kTablet) {
        idx = 2;
      } else {
        idx = 0; // Desktop and unknowns.
      }
      if (images[idx].IsEmpty()) {
        if (icons && icons->size() == 6) {
          const std::string icon =
            icons->at(idx * 2 + (dark_text_color ? 0 : 1));
          images[idx] = GetImageModel(icon);
        }
      }
      menu_model->AddItemWithIcon(
          command_id,
          base::UTF8ToUTF16(device.full_name),
          images[idx]);
#endif
      last_updated_map_[command_id] = device.last_updated_timestamp;
    }
    index ++;
  }
  if (!GetHasInstalledDevices()) {
    // Add a entry that allows people to download another device.
    int command_id = index + IDC_VIV_SEND_TO_DEVICE_FIRST;
    menu_model->AddItem(
      command_id, l10n_util::GetStringUTF16(IDS_VIV_GET_VIVALDI_FOR_MOBILE));
  }
}

bool DeviceMenuController::GetHasInstalledDevices() {
  return last_updated_map_.size() > 0;
}

bool DeviceMenuController::HandleCommand(int command_id, int event_flags) {
  if (command_id >= IDC_VIV_SEND_TO_DEVICE_FIRST &&
      command_id <= IDC_VIV_SEND_TO_DEVICE_LAST) {
    if (GetHasInstalledDevices()) {
      send_tab_to_self::SendTabToSelfSyncService* service =
        SendTabToSelfSyncServiceFactory::GetForProfile(browser_->profile());
      int index = command_id - IDC_VIV_SEND_TO_DEVICE_FIRST;
      std::string guid =
        service->GetSendTabToSelfModel()->
            GetTargetDeviceInfoSortedList()[index].cache_guid;
      service->GetSendTabToSelfModel()->AddEntry(url_, url_title_, guid);
    } else {
      rv_context_menu_->OnGetMobile();
    }
    return true;
  }
  return false;
}

bool DeviceMenuController::IsCommandIdEnabled(
    int command_id,
    const content::ContextMenuParams& params,
    bool* enabled) {
  if (command_id >= IDC_VIV_SEND_TO_DEVICE_FIRST &&
      command_id <= IDC_VIV_SEND_TO_DEVICE_LAST) {
    *enabled = true;
    return true;
  }
  *enabled = false;
  return false;
}

bool DeviceMenuController::GetHighlightText(int command_id, std::string& text) {
  if (command_id >= IDC_VIV_SEND_TO_DEVICE_FIRST &&
      command_id <= IDC_VIV_SEND_TO_DEVICE_LAST) {
    auto it = last_updated_map_.find(command_id);
    if (it != last_updated_map_.end()) {
      int time_in_days = (base::Time::Now() - it->second).InDays();
      if (time_in_days == 0) {
        text = l10n_util::GetStringUTF8(IDS_VIV_DEVICE_ACTIVE_TODAY);
      } else if (time_in_days == 1) {
        text = l10n_util::GetStringUTF8(IDS_VIV_DEVICE_ACTIVE_YESTERDAY);
      } else {
        text = l10n_util::GetStringFUTF8(
            IDS_VIV_DEVICE_ACTIVE_DAYS_AGO,
            base::UTF8ToUTF16(base::NumberToString(time_in_days)));
      }
    }
    return true;
  }
  return false;
}



}  // namespace vivaldi
