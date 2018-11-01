// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "ui/dragging/custom_drag_source_win.h"
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"

namespace vivaldi {

CustomDragSourceWin::CustomDragSourceWin(bool dragging_in_progress)
    : custom_tab_dragging_in_progress_(dragging_in_progress) {}

HRESULT CustomDragSourceWin::GiveFeedback(DWORD effect) {
  if (custom_tab_dragging_in_progress_) {
    return S_OK;
  }
  return DRAGDROP_S_USEDEFAULTCURSORS;
}

}  // namespace vivaldi
