// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved.

#include "extensions/api/window/window_private_api.h"

#import <AppKit/AppKit.h>

#include "ui/vivaldi_browser_window.h"

namespace extensions {

bool WindowPrivateIsOnScreenWithNotchFunction::IsWindowOnScreenWithNotch(
    VivaldiBrowserWindow* window) {
  if (@available(macos 12.0.1, *)) {
    id screen =
        [window->GetWidget()->GetNativeWindow().GetNativeNSWindow() screen];
    NSEdgeInsets insets = [screen safeAreaInsets];
    if (insets.top != 0) {
      return true;
    }
  }
  return false;
}

}  // namespace extensions
