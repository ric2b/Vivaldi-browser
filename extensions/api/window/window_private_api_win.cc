// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved.

#include "extensions/api/window/window_private_api.h"

#include "ui/vivaldi_browser_window.h"

namespace extensions {

bool WindowPrivateIsOnScreenWithNotchFunction::IsWindowOnScreenWithNotch(
    VivaldiBrowserWindow* window) {
  return false;
}

}  // namespace extensions
