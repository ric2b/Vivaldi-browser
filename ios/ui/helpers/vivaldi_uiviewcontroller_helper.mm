// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/helpers/vivaldi_uiviewcontroller_helper.h"

@implementation UIViewController(Vivaldi)

#pragma mark:- GETTERS

- (BOOL)hasValidOrientation {
  UIDeviceOrientation orientation= [[UIDevice currentDevice] orientation];
  // Consider the flat orientation as invalid since we will not rotate the
  // interface when the device is flat.
  if (UIDeviceOrientationIsFlat(orientation) ||
      orientation == UIDeviceOrientationUnknown)
    return NO;
  return YES;
}

- (BOOL)isDevicePortrait {
  UIDeviceOrientation orientation= [[UIDevice currentDevice] orientation];

  // If device is flat or orientation is invalid determine the interface from
  // the bounds.
  if (!self.hasValidOrientation) {
    return self.view.bounds.size.height > self.view.bounds.size.width;
  }

  if(UIDeviceOrientationIsLandscape(orientation)) {
    return NO;
  } else {
    return YES;
  }
}

- (BOOL)isDeviceIPad {
    return [UIDevice currentDevice].userInterfaceIdiom
        == UIUserInterfaceIdiomPad;
}

@end
