// Copyright (c) 2018 Vivaldi Technologies

#include "chrome/browser/ui/aura/tab_contents/web_drag_bookmark_handler_aura.h"

blink::WebDragOperationsMask WebDragBookmarkHandlerAura::OnDragEnd(
  int screen_x,
  int screen_y,
  blink::WebDragOperationsMask ops,
  bool cancelled) {
  return ops;
}
