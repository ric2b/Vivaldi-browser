// Copyright 2024-25 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/pagezoom/dialog/uiwindow_pagezoom.h"

#import <objc/runtime.h>

#import "ios/chrome/browser/text_zoom/ui_bundled/text_zoom_view_controller.h"
#import "ios/ui/settings/pagezoom/dialog/vivaldi_pagezoom_view_controller.h"


namespace {
// Key for associated object
static char kViewControllerKey;

// Animation constants
const CGFloat kAnimationDuration = 0.3;
const UIViewAnimationOptions kShowAnimationOptions =
  UIViewAnimationOptionCurveEaseOut;
const UIViewAnimationOptions kHideAnimationOptions =
  UIViewAnimationOptionCurveEaseIn;

// Layout constants
const CGFloat kBaseHeight = 185;
const CGFloat kAnimationDelay = 0;
}  // namespace

@implementation UIWindow (PageZoom)

#pragma mark - ViewController Property with Associated Object

- (void)setViewController:(VivaldiPageZoomViewController *)viewController {
  objc_setAssociatedObject(self, &kViewControllerKey,
    viewController,
    OBJC_ASSOCIATION_RETAIN_NONATOMIC
  );
}

- (VivaldiPageZoomViewController *)viewController {
  return objc_getAssociatedObject(self, &kViewControllerKey);
}

- (void)showPageZoomViewController:
    (VivaldiPageZoomViewController *)viewController {
  // Remove any existing view controller first
  if (self.viewController) {
    [self.viewController.view removeFromSuperview];
  }

  self.viewController = viewController;

  // Account for safe area
  UIEdgeInsets safeAreaInsets = self.safeAreaInsets;
  CGFloat totalHeight = kBaseHeight + safeAreaInsets.top;

  // Set initial frame off-screen (start from above the screen)
  self.viewController.view.frame = CGRectMake(
      0,
      -totalHeight,
      self.bounds.size.width,  // Use bounds instead of frame
      totalHeight
  );

  // Add the view to the window
  [self addSubview:self.viewController.view];

  // Force layout
  [self.viewController.view layoutIfNeeded];

  // Animate sliding it down
  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kAnimationDuration
                        delay:kAnimationDelay
                      options:kShowAnimationOptions |
                        UIViewAnimationOptionBeginFromCurrentState
                   animations:^{
    weakSelf.viewController.view.frame = CGRectMake(
        0,
        0,
        weakSelf.bounds.size.width,
        totalHeight
    );
  } completion:nil];
}

- (void)hidePageZoomViewController {
  if (!self.viewController) return;

  CGFloat totalHeight = self.viewController.view.frame.size.height;

  // Capture view controller to avoid potential nil issues
  VivaldiPageZoomViewController *viewController = self.viewController;

  // Forcing layout before animation
  [viewController.view layoutIfNeeded];

  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kAnimationDuration
                        delay:kAnimationDelay
                      options:kHideAnimationOptions |
                        UIViewAnimationOptionBeginFromCurrentState
                   animations:^{
    viewController.view.frame = CGRectMake(
        0,
        -totalHeight,
        weakSelf.bounds.size.width,
        totalHeight
    );
  } completion:^(BOOL finished) {
    [viewController.view removeFromSuperview];
    if (weakSelf.viewController == viewController) {
      weakSelf.viewController = nil;
    }
  }];
}

// Add method to handle orientation changes
- (void)layoutSubviews {
  [super layoutSubviews];

  if (self.viewController) {
    CGFloat totalHeight = kBaseHeight + self.safeAreaInsets.top;
    CGFloat currentY = self.viewController.view.frame.origin.y;

    self.viewController.view.frame = CGRectMake(
        0,
        currentY,
        self.bounds.size.width,
        totalHeight
    );
  }
}
@end
