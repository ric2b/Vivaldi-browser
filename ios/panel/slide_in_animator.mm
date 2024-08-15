// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/panel/slide_in_animator.h"

#import <UIKit/UIKit.h>

#import "ios/panel/panel_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SlideInAnimator

-(NSTimeInterval)transitionDuration:
(id<UIViewControllerContextTransitioning>)context {
  return panel_transition_duration;
}

-(void)animateTransition:(id<UIViewControllerContextTransitioning>)context {
  UIViewController* toVC =
      [context viewControllerForKey:UITransitionContextToViewControllerKey];
  UIView* toView = [context viewForKey:UITransitionContextToViewKey];
  CGRect toViewStartFrame = [context initialFrameForViewController:toVC];
  toViewStartFrame.origin =
  CGPointMake(- (panel_sidebar_width + panel_icon_size), 0);
  [context.containerView addSubview:toVC.view ];
  toView.frame = toViewStartFrame;

  [UIView animateWithDuration:[self transitionDuration:context]
                        delay:0
       usingSpringWithDamping:panel_transition_damping
        initialSpringVelocity:0
                      options:UIViewAnimationCurveEaseOut
                   animations:^{
    CGRect rect = toView.frame;
    rect.origin = CGPointZero;
    toView.frame = rect;
  }
  completion:^(BOOL finished){
    BOOL success = ![context transitionWasCancelled];
    if (!success) {
      [toVC.view removeFromSuperview];
    }
    [context completeTransition:success];
  }];
}

@end
