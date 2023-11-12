// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/toolbar/vivaldi_toolbar_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - SIZE AND PADDINGS

const CGFloat vPrimaryToolbarHorizontalPadding = 4;
const CGFloat vPrimaryToolbarTextFieldLeadingOffsetNoImage = 0;
const CGFloat vPrimaryToolbarTextFieldLeadingOffsetImage = 2;
const CGFloat vStickyToolbarViewHeight = 20.0;

#pragma mark - ANIMATIONS
const CGFloat vPrimaryToolbarAnimationDuration = 0.5;
const CGFloat vPrimaryToolbarAnimationDamping = 0.9;
const CGFloat vPrimaryToolbarAnimationSpringVelocity = 2.0;

const CGFloat vStickyToolbarAnimationDuration = 0.3;
const CGFloat vStickyToolbarAnimationDamping = 0.8;
const CGFloat vStickyToolbarAnimationSpringVelocity = 15.0;
const CGFloat vStickyToolbarExpandedScale = 1.0;
const CGFloat vStickyToolbarExpandedAlpha = 1.0;
const CGFloat vStickyToolbarCollapsedScale = 0.3;
const CGFloat vStickyToolbarCollapsedAlpha = 0.0;

#pragma mark - ICONS

NSString* vToolbarPanelButtonIcon = @"toolbar_panel";
NSString* vToolbarSearchButtonIcon = @"toolbar_search";
