// Copyright 2020 Vivaldi Technologies. All rights reserved.

#import "components/remote_cocoa/app_shim/vivaldi_native_widget_mac_frameless_nswindow.h"

#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"

@implementation VivaldiNativeWidgetMacFramelessNSWindow

- (void)mouseDown:(NSEvent*)event {
  if (event.clickCount == 2) {
    NSUserDefaults* ud = [NSUserDefaults standardUserDefaults];
    NSString* appleActionOnDoubleClick =
      [ud stringForKey:@"AppleActionOnDoubleClick"];
    if ([appleActionOnDoubleClick isEqual:@"Minimize"]) {
      [self miniaturize:event];
    } else if ([appleActionOnDoubleClick isEqual:@"Maximize"]) {
      [self zoom:event];
    }
  }
  [super mouseDown:event];
}

@end
