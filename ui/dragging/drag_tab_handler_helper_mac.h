// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef UI_DRAGGING_DRAG_TAB_HANDLER_HELPER_MAC_H_
#define UI_DRAGGING_DRAG_TAB_HANDLER_HELPER_MAC_H_

#include "extensions/api/tabs/tabs_private_api.h"

namespace vivaldi {

// This set of functions lets C++ code interact with the cocoa pasteboard and
// dragging methods.

void populateCustomData(TabDragDataCollection& data);

}  // namespace vivaldi

#endif  // UI_DRAGGING_DRAG_TAB_HANDLER_HELPER_MAC_H_
