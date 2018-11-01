// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/drag_drop_client.h"

#include "ui/aura/window.h"
#include "ui/base/class_property.h"

DECLARE_UI_CLASS_PROPERTY_TYPE(aura::client::DragDropClient*)

namespace aura {
namespace client {

DEFINE_LOCAL_UI_CLASS_PROPERTY_KEY(
    DragDropClient*, kRootWindowDragDropClientKey, NULL);

void SetDragDropClient(Window* root_window, DragDropClient* client) {
  DCHECK_EQ(root_window->GetRootWindow(), root_window);
  root_window->SetProperty(kRootWindowDragDropClientKey, client);
}

DragDropClient* GetDragDropClient(Window* root_window) {
  if (root_window)
    DCHECK_EQ(root_window->GetRootWindow(), root_window);
  return root_window ?
      root_window->GetProperty(kRootWindowDragDropClientKey) : NULL;
}

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
