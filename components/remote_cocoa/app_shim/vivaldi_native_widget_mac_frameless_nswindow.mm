// Copyright 2020 Vivaldi Technologies. All rights reserved.

#import "components/remote_cocoa/app_shim/vivaldi_native_widget_mac_frameless_nswindow.h"

#include "base/apple/foundation_util.h"
#include "base/mac/mac_util.h"

@implementation VivaldiNativeWidgetMacFramelessNSWindow

- (void)mouseDown:(NSEvent*)event {
  if (event.clickCount == 2) {
    NSUserDefaults* ud = [NSUserDefaults standardUserDefaults];
    NSString* appleActionOnDoubleClick =
      [ud stringForKey:@"AppleActionOnDoubleClick"];
    if (appleActionOnDoubleClick == nil ||
       [appleActionOnDoubleClick isEqual:@"Maximize"]) {
      // Note(tomas@vivaldi.com): VB-71676 There is a bug in some macOS versions
      // where this value is nil after a fresh install of macOS.
      // The default action is to zoom
      [self zoom:event];
    } else if ([appleActionOnDoubleClick isEqual:@"Minimize"]) {
      [self miniaturize:event];
    }
  }
  [super mouseDown:event];
}

@end
