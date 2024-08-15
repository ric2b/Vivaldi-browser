// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved.

#include "extensions/api/window/window_private_api.h"

#import <AppKit/AppKit.h>
#include <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include "extensions/schema/window_private.h"
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

void WindowPrivateSetControlButtonsPaddingFunction::RequestChange(
    gfx::NativeWindow window,
    vivaldi::window_private::ControlButtonsPadding padding) {
  auto* ns_window = window.GetNativeNSWindow();

  const auto *paddingAsString = vivaldi::window_private::ToString(padding);
  NSString* ns_padding =
      [NSString stringWithUTF8String:paddingAsString];

  NSDictionary* userInfo = @{@"padding" : ns_padding};
  [[NSNotificationCenter defaultCenter]
      postNotificationName:@"VivaldiSetControlButtonsPadding"
                    object:ns_window
                  userInfo:userInfo];
}

void WindowPrivatePerformHapticFeedbackFunction::PerformHapticFeedback() {
  if (@available(macos 12.0.1, *)) {
    [[NSHapticFeedbackManager defaultPerformer]
        performFeedbackPattern:NSHapticFeedbackPatternAlignment
              performanceTime:NSHapticFeedbackPerformanceTimeNow
    ];
  }
}

}  // namespace extensions
