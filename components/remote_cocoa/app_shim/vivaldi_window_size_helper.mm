// Copyright 2019 Vivaldi Technologies. All rights reserved.

#import "components/remote_cocoa/app_shim/vivaldi_window_size_helper.h"

namespace vivaldi {

void VerifyWindowSize(NSWindow* window) {
  CGFloat width = [window frame].size.width;
  CGFloat height = [window frame].size.height;
  CGFloat screen_height = [window screen].frame.size.height;
  CGFloat x_pos = [window frame].origin.x;
  CGFloat y_pos = [window frame].origin.y;
  bool updateWindowFrame = false;

    // make sure window fits vertically on the screen
    if (height > screen_height) {
      height = screen_height - [NSApp mainMenu].menuBarHeight;
      updateWindowFrame = true;
    }

    // make sure window titlebar is on the screen
    if (y_pos + height > screen_height) {
      y_pos = screen_height - height - [NSApp mainMenu].menuBarHeight;
      updateWindowFrame = true;
    }


  if (updateWindowFrame) {
    [window setFrame:CGRectMake(x_pos, y_pos, width, height) display:YES];
  }
}

} // vivaldi