// Copyright (c) 2018 Vivaldi Technologies

#include "chrome/browser/ui/cocoa/tab_contents/web_drag_bookmark_handler_mac.h"

blink::WebDragOperationsMask WebDragBookmarkHandlerMac::OnDragEnd(
    int screen_x,
    int screen_y,
    blink::WebDragOperationsMask ops,
    bool cancelled) {
  return ops;
}
