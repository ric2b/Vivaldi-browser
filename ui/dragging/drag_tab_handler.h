// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DRAGGING_DRAG_TAB_HANDLER_H_
#define UI_DRAGGING_DRAG_TAB_HANDLER_H_

#include <string>
#include "base/macros.h"
#include "content/public/browser/web_contents.h"
#if defined(OS_MACOSX)
#include "chrome/browser/ui/cocoa/tab_contents/web_drag_bookmark_handler_mac.h"
#else
#include "chrome/browser/ui/aura/tab_contents/web_drag_bookmark_handler_aura.h"
#endif  // defined(OS_MACOSX)
#include "extensions/api/tabs/tabs_private_api.h"
#include "third_party/blink/public/platform/web_drag_operation.h"
#include "ui/base/dragdrop/os_exchange_data.h"

namespace content {
class WebContents;
}

namespace extensions {
class VivaldiAppHelper;
}

#if defined(OS_MACOSX)
typedef WebDragBookmarkHandlerMac BaseClass;
#else
typedef WebDragBookmarkHandlerAura BaseClass;
#endif  // OS_MACOSX

namespace vivaldi {
// Vivaldi needs to intercept tab drag events so it can dispatch them to the
// extensions system.
// We override the bookmark drag handler from Chromium and handle our own
// data, if needed, otherwise we call the base class to let it handle
// bookmark drags.
class DragTabHandler :
#if defined(OS_MACOSX)
    public WebDragBookmarkHandlerMac {
#else
    public WebDragBookmarkHandlerAura {
#endif  // OS_MACOSX
 public:
  DragTabHandler();
  ~DragTabHandler() override;

  // Overridden from content::WebDragDestDelegate:
  void DragInitialize(content::WebContents* contents) override;
  void OnDragOver() override;
  void OnDragEnter() override;
  void OnDrop() override;
  void OnDragLeave() override;
  blink::WebDragOperationsMask OnDragEnd(int screen_x,
                                         int screen_y,
                                         blink::WebDragOperationsMask ops,
                                         bool cancelled) override;

#if defined(USE_AURA)
  void OnReceiveDragData(const ui::OSExchangeData& data) override;
#endif  // defined(USE_AURA)

#if defined(OS_MACOSX)
  void SetDragData(const content::DropData* data) override;
#endif  // OS_MACOSX

 private:
  content::WebContents* web_contents_;

  // The data for the active drag.  Empty when there is no active drag.
  TabDragDataCollection tab_drag_data_;

  DISALLOW_COPY_AND_ASSIGN(DragTabHandler);
};

}  // namespace vivaldi

#endif  // UI_DRAGGING_DRAG_TAB_HANDLER_H_
