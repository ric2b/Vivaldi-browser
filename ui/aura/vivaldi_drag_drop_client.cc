// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "ui/aura/client/drag_drop_client.h"

namespace aura {

namespace client {

int DragDropClient::StartDragAndDrop(
    const ui::OSExchangeData& data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& screen_location,
    int operation,
    ui::DragDropTypes::DragEventSource source) {
  bool cancelled;
  return StartDragAndDrop(data, root_window, source_window, screen_location,
                          operation, source, cancelled);
}

int DragDropClient::StartDragAndDrop(const ui::OSExchangeData& data,
                                     aura::Window* root_window,
                                     aura::Window* source_window,
                                     const gfx::Point& screen_location,
                                     int operation,
                                     ui::DragDropTypes::DragEventSource source,
                                     bool& cancelled) {
  cancelled = false;
  return StartDragAndDrop(data, root_window, source_window, screen_location,
                          operation, source);
}

}  // namespace client
}  // namespace aura
