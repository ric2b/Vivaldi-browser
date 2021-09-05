// Copyright 2019 Vivaldi Technologies. All rights reserved.

#import "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"

namespace remote_cocoa {

void NativeWidgetNSWindowBridge::VerifyWindowSize() {
  CGFloat window_width = [window_ frame].size.width;
  CGFloat window_height = [window_ frame].size.height;
  CGFloat screen_width = [window_ screen].frame.size.width;
  CGFloat screen_height = [window_ screen].frame.size.height;
  CGFloat x_pos = [window_ frame].origin.x;
  CGFloat y_pos = [window_ frame].origin.y;
  CGFloat width = window_width;
  CGFloat height = window_height;
  bool updateWindowFrame = false;

  if (x_pos + window_width > screen_width) {
    width = screen_width - x_pos;
    updateWindowFrame = true;
  }

  if (y_pos + window_height > screen_height) {
    height = screen_height - y_pos - [NSApp mainMenu].menuBarHeight;
    updateWindowFrame = true;
  }

  if (updateWindowFrame) {
    [window_ setFrame:CGRectMake(x_pos, y_pos, width, height) display:YES];
  }
}

} // remote_cocoa