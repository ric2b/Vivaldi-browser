// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/dragging/drag_tab_handler.h"

#include <utility>

#include "app/vivaldi_apptools.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "ui/base/dragdrop/os_exchange_data.h"

#if defined(OS_MACOSX)
#include "ui/dragging/drag_tab_handler_helper_mac.h"
#endif  // OS_MACOSX

using content::WebContents;
using extensions::TabsPrivateAPI;

namespace vivaldi {

DragTabHandler::DragTabHandler() : web_contents_(NULL) {}

DragTabHandler::~DragTabHandler() {}

void DragTabHandler::DragInitialize(WebContents* contents) {
  web_contents_ = contents;

#if defined(OS_MACOSX)
  populateCustomData(tab_drag_data_);
#endif  // OS_MACOSX

  BaseClass::DragInitialize(contents);
}

void DragTabHandler::OnDragOver() {
  // We ignore this one
  BaseClass::OnDragOver();
}

#if defined(USE_AURA)
void DragTabHandler::OnReceiveDragData(const ui::OSExchangeData& data) {
  TabsPrivateAPI* api = TabsPrivateAPI::GetFactoryInstance()->Get(
      ProfileManager::GetActiveUserProfile());
  if (api && api->tab_drag_delegate() && vivaldi::IsTabDragInProgress() &&
      data.HasCustomFormat(ui::Clipboard::GetWebCustomDataFormatType())) {
    base::Pickle pickle;

    if (data.GetPickledData(ui::Clipboard::GetWebCustomDataFormatType(),
                            &pickle)) {
      base::PickleIterator iter(pickle);
      uint32_t size = 0;
      if (!iter.ReadUInt32(&size))
        return;

      tab_drag_data_.clear();
      for (uint32_t i = 0; i < size; ++i) {
        base::string16 deserialized_type;
        if (!iter.ReadString16(&deserialized_type))
          break;
        base::string16 transport_data;
        if (!iter.ReadString16(&transport_data))
          break;
        tab_drag_data_.insert(
            std::make_pair(deserialized_type, transport_data));
      }
    } else {
      BaseClass::OnReceiveDragData(data);
    }
  }
}
#endif  // defined(USE_AURA)

#if defined(OS_MACOSX)
void DragTabHandler::SetDragData(const content::DropData* drop_data) {
  if (!drop_data) {
    return;
  }
  // We only need custom_data atm.
  if (!drop_data->custom_data.empty()) {
    tab_drag_data_.clear();
    for (std::unordered_map<base::string16, base::string16>::const_iterator it =
             drop_data->custom_data.begin();
         it != drop_data->custom_data.end(); ++it) {
      tab_drag_data_.insert(std::make_pair(it->first, it->second));
    }
  }
}
#endif // OS_MACOSX

void DragTabHandler::OnDragEnter() {
  TabsPrivateAPI* api = TabsPrivateAPI::GetFactoryInstance()->Get(
      ProfileManager::GetActiveUserProfile());
  if (api && api->tab_drag_delegate() && vivaldi::IsTabDragInProgress()) {
    api->tab_drag_delegate()->OnDragEnter(tab_drag_data_);
  } else {
    // We didn't handle it
    BaseClass::OnDragEnter();
  }
}

void DragTabHandler::OnDrop() {
  TabsPrivateAPI* api = TabsPrivateAPI::GetFactoryInstance()->Get(
      ProfileManager::GetActiveUserProfile());
  if (api && api->tab_drag_delegate() && vivaldi::IsTabDragInProgress()) {
    api->tab_drag_delegate()->OnDrop(tab_drag_data_);
  } else {
    // We didn't handle it
    BaseClass::OnDrop();
  }
}

void DragTabHandler::OnDragLeave() {
  TabsPrivateAPI* api = TabsPrivateAPI::GetFactoryInstance()->Get(
      ProfileManager::GetActiveUserProfile());
  if (api && api->tab_drag_delegate() && vivaldi::IsTabDragInProgress()) {
    api->tab_drag_delegate()->OnDragLeave(tab_drag_data_);
  } else {
    // We didn't handle it
    BaseClass::OnDragLeave();
  }
}

blink::WebDragOperationsMask DragTabHandler::OnDragEnd(
    int screen_x,
    int screen_y,
    blink::WebDragOperationsMask ops,
    bool cancelled) {
  TabsPrivateAPI* api = TabsPrivateAPI::GetFactoryInstance()->Get(
      ProfileManager::GetActiveUserProfile());
  if (api && api->tab_drag_delegate() && vivaldi::IsTabDragInProgress()) {
    return api->tab_drag_delegate()->OnDragEnd(screen_x, screen_y, ops,
                                               tab_drag_data_, cancelled);
  } else {
    // We didn't handle it
    return BaseClass::OnDragEnd(screen_x, screen_y, ops, cancelled);
  }
}

}  // namespace vivaldi
