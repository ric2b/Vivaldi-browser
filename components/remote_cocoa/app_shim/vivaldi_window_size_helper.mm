// Copyright 2019 Vivaldi Technologies. All rights reserved.

#import "components/remote_cocoa/app_shim/vivaldi_window_size_helper.h"

#import "base/mac/mac_util.h"

namespace vivaldi {

unsigned int getDisplayId(NSScreen* screen) {
  return [[[screen deviceDescription]
      objectForKey:@"NSScreenNumber"] unsignedIntValue];
}

void VerifyWindowSizeInternal(NSWindow* window, const display::Display& old_display) {
  // The screen that the window is on
  unsigned int window_screen_display_id = getDisplayId([window screen]);

  // Window dimensions & position on screen
  CGFloat window_width = [window frame].size.width;
  CGFloat window_height = [window frame].size.height;
  CGFloat x_pos = [window frame].origin.x;
  CGFloat y_pos = [window frame].origin.y;

  // Put the window onto the screen that has keyboard focus, mainScreen.
  NSScreen* main_screen = [NSScreen mainScreen];
  unsigned int selected_screen_display_id = getDisplayId(main_screen);

  // screen.visibleFrame takes the menubar and dock into account
  CGFloat selected_screen_visible_height = main_screen.visibleFrame.size.height;
  CGFloat selected_screen_full_height = main_screen.frame.size.height;
  CGFloat selected_screen_width = main_screen.visibleFrame.size.width;
  CGFloat selected_screen_origin_y = main_screen.frame.origin.y;
  CGFloat selected_screen_origin_x = main_screen.frame.origin.x;

  bool updateWindowFrame = false;
  if (old_display.id() == window_screen_display_id ||
      window_screen_display_id == 0) {
    // The display that the window was on has been removed,
    // window_screen_display_id == 0 observed when using more than two monitors
    if (old_display.id() == selected_screen_display_id) {
      // main_screen was the removed screen, find the next available screen
      for (NSScreen* screen in [NSScreen screens]) {
        unsigned int screen_display_id = getDisplayId(screen);
        if (old_display.id() != screen_display_id) {
          selected_screen_display_id = screen_display_id;
          selected_screen_visible_height = screen.visibleFrame.size.height;
          selected_screen_full_height = screen.frame.size.height;
          selected_screen_width = screen.frame.size.width;
          selected_screen_origin_y = screen.frame.origin.y;
          selected_screen_origin_x = screen.frame.origin.x;
          break;
        }
      }
    }

    // Resize window if needed and make sure it is inside the screen bounds
    if (window_height > selected_screen_visible_height) {
      window_height = selected_screen_visible_height;
      updateWindowFrame = true;
    }
    if (y_pos + window_height > selected_screen_visible_height) {
      y_pos = selected_screen_full_height - window_height -
        [NSApp mainMenu].menuBarHeight + selected_screen_origin_y;
      updateWindowFrame = true;
    }
    if (window_width > selected_screen_width) {
      window_width = selected_screen_width;
      updateWindowFrame = true;
    }
    if (x_pos + window_width > selected_screen_width) {
      x_pos = selected_screen_width - window_width + selected_screen_origin_x;
      updateWindowFrame = true;
    }
    if (updateWindowFrame) {
      [window setFrame:CGRectMake(x_pos, y_pos, window_width, window_height)
              display:YES];
    }
  }
}

void VerifyWindowSize(NSWindow* window, const display::Displays& old_displays) {
  if (base::mac::MacOSVersion() >= 14'00'00) {
    // TODO(tomas@vivaldi.com): This is not needed on macOS 14+.
    // Remove this when minimum supported version is macOS 14.x
    return;
  }

  for (const auto& display : old_displays) {
    VerifyWindowSizeInternal(window, display);
  }
}

} // vivaldi
