// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_PANEL_SLIDE_OUT_ANIMATOR_H_
#define IOS_PANEL_SLIDE_OUT_ANIMATOR_H_

#import <UIKit/UIKit.h>

@interface SlideOutAnimator
    : NSObject<UIViewControllerAnimatedTransitioning>


-(NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)context;
-(void)animateTransition:(id<UIViewControllerContextTransitioning>)context;

@end

#endif  // IOS_PANEL_SLIDE_OUT_ANIMATOR_H_
