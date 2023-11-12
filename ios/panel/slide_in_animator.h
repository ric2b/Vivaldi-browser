// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_PANEL_SLIDE_IN_ANIMATOR_H_
#define IOS_PANEL_SLIDE_IN_ANIMATOR_H_

#import <UIKit/UIKit.h>

@interface SlideInAnimator
    : NSObject<UIViewControllerAnimatedTransitioning>


-(NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)context;
-(void)animateTransition:(id<UIViewControllerContextTransitioning>)context;

@end

#endif  // IOS_PANEL_SLIDE_IN_ANIMATOR_H_
