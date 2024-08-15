// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/panel/panel_transitioning_delegate.h"

#import "base/apple/foundation_util.h"
#import "ios/panel/sidebar_panel_presentation_controller.h"
#import "ios/panel/slide_in_animator.h"
#import "ios/panel/slide_out_animator.h"

@implementation PanelTransitioningDelegate

- (UIPresentationController*)
presentationControllerForPresentedViewController:(UIViewController*)presented
                        presentingViewController:(UIViewController*)presenting
                            sourceViewController:(UIViewController*)source {
  SidebarPanelPresentationController *presentationController =
      [[SidebarPanelPresentationController alloc]
          initWithPresentedViewController:presented
                 presentingViewController:presenting];
  presentationController.toolbarType = self.toolbarType;
  return presentationController;
}

- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForPresentedController:(UIViewController*)presented
                     presentingController:(UIViewController*)presenting
                         sourceController:(UIViewController*)source {
    return [[SlideInAnimator alloc] init];
}

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForDismissedController:(UIViewController*)dismissed {
    return [[SlideOutAnimator alloc] init];
}

@end
