// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/ui_bundled/presentation/infobar_banner_animator.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

using vivaldi::IsVivaldiRunning;
// End Vivaldi

@implementation InfobarBannerAnimator

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  return 1;
}

// "This method can only be a nop if the transition is interactive and not a
// percentDriven interactive transition." (As stated in this method public
// interface). Since this criteria is met it NO-OPs.
- (void)animateTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
}

#pragma mark - UIViewControllerTransitioningDelegate

- (void)startInteractiveTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  // Set up the keys for the "base" view/VC and the "presented" view/VC. These
  // will be used to fetch the associated objects later.
  NSString* baseViewKey = self.presenting ? UITransitionContextFromViewKey
                                          : UITransitionContextToViewKey;
  NSString* presentedViewControllerKey =
      self.presenting ? UITransitionContextToViewControllerKey
                      : UITransitionContextFromViewControllerKey;
  NSString* presentedViewKey = self.presenting ? UITransitionContextToViewKey
                                               : UITransitionContextFromViewKey;

  // Get views and view controllers for this transition.
  UIView* baseView = [transitionContext viewForKey:baseViewKey];
  UIViewController* presentedViewController =
      [transitionContext viewControllerForKey:presentedViewControllerKey];
  UIView* presentedView = [transitionContext viewForKey:presentedViewKey];

  // Always add the destination view to the container.
  UIView* containerView = [transitionContext containerView];
  if (self.presenting) {
    [containerView addSubview:presentedView];
  } else {
    [containerView addSubview:baseView];
  }

  // Set the initial frame and Compute the final frame for the presented view.
  CGRect presentedViewFinalFrame = CGRectZero;

  if (self.presenting) {
    presentedViewFinalFrame =
        [transitionContext finalFrameForViewController:presentedViewController];
    CGRect presentedViewStartFrame = presentedViewFinalFrame;
    if (IsVivaldiRunning() &&
        GetApplicationContext()
            ->GetLocalState()->GetBoolean(prefs::kBottomOmnibox)) {
      // Start from below the screen when animating from bottom
      presentedViewStartFrame.origin.y = CGRectGetHeight(containerView.bounds);
    } else { // Start from above the screen when animating from top (default)

  presentedViewStartFrame.origin.y = -CGRectGetWidth(containerView.bounds);

    } // End Vivaldi

    presentedView.frame = presentedViewStartFrame;
    presentedView.alpha = 0;
  } else {
    presentedViewFinalFrame = presentedView.frame;

    if (IsVivaldiRunning() &&
        GetApplicationContext()
            ->GetLocalState()->GetBoolean(prefs::kBottomOmnibox)) {
      // Exit to below the screen when animating from bottom
      // Adding 200 Points to so that the when user drag the banner to dismiss
      // it goes out of the bounds and disappear
      presentedViewFinalFrame.origin.y = CGRectGetHeight(containerView.bounds)
                                                                        + 200.0;
    } else { // Exit to above the screen when animating from top (default)

  presentedViewFinalFrame.origin.y = -CGRectGetWidth(containerView.bounds);

    } // End Vivaldi

  }

  UIViewPropertyAnimator* animator = [[UIViewPropertyAnimator alloc]
      initWithDuration:[self transitionDuration:transitionContext]
          dampingRatio:0.85
            animations:^{
              presentedView.frame = presentedViewFinalFrame;
              presentedView.alpha = 1;
            }];

  [animator addCompletion:^(UIViewAnimatingPosition finalPosition) {
    BOOL success = ![transitionContext transitionWasCancelled];

    // If presentation failed, remove the view.
    if (self.presenting && !success) {
      [presentedView removeFromSuperview];
    }

    // If dismiss was successful, remove the view.
    if (!self.presenting && success) {
      [presentedView removeFromSuperview];
    }

    // Notify UIKit that the transition has finished
    [transitionContext finishInteractiveTransition];
    [transitionContext completeTransition:success];
  }];

  self.propertyAnimator = animator;
  [self.propertyAnimator startAnimation];
}

@end
