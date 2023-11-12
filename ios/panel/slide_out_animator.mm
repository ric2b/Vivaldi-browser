// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/panel/slide_out_animator.h"

#import <UIKit/UIKit.h>

#import "ios/panel/panel_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SlideOutAnimator

-(NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)context {
    return 0.3;
}

-(void)animateTransition:(id<UIViewControllerContextTransitioning>)context {
    UIViewController* fromVC = [context
                viewControllerForKey:UITransitionContextFromViewControllerKey];
    UIView* fromView = [context viewForKey:UITransitionContextFromViewKey];
    UIViewController* toVC = [context
            viewControllerForKey:UITransitionContextToViewControllerKey];
    toVC.view.alpha = 1.0;
    CGRect fromViewStartFrame = [context initialFrameForViewController:fromVC];
    CGRect fromViewFinalFrame = [context finalFrameForViewController:fromVC];
    CGRect frame = fromViewFinalFrame;
    frame.origin.x = - (panel_sidebar_width + panel_icon_size);
    fromViewFinalFrame = frame;
    fromView.frame = fromViewStartFrame;
    float duration = [self transitionDuration:context];
    [UIView animateWithDuration:duration
                     delay: 0
                   options: UIViewAnimationCurveEaseOut
                animations:^{
                        [fromView setFrame:fromViewFinalFrame];
                        }
                completion:^(BOOL finished) {
                        [fromView removeFromSuperview];
                        [context completeTransition:YES];
                    }];
}

@end
