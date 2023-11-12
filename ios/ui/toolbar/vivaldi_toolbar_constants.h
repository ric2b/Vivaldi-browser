// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_TOOLBAR_VIVALDI_TOOLBAR_CONSTANTS_H_
#define IOS_UI_TOOLBAR_VIVALDI_TOOLBAR_CONSTANTS_H_

#import <Foundation/Foundation.h>

#pragma mark - SIZE AND PADDINGS

// Leading and trailing space for shield and vivaldi menu button.
extern const CGFloat vPrimaryToolbarHorizontalPadding;
// Omnibox textfield leading space when there's no image/icon present.
extern const CGFloat vPrimaryToolbarTextFieldLeadingOffsetNoImage;
// Omnibox textfield leading space when there's image/icon present.
extern const CGFloat vPrimaryToolbarTextFieldLeadingOffsetImage;
// Height for the vivaldi sticky top toolbar view
extern const CGFloat vStickyToolbarViewHeight;

#pragma mark - ANIMATIONS
extern const CGFloat vPrimaryToolbarAnimationDuration;
extern const CGFloat vPrimaryToolbarAnimationDamping;
extern const CGFloat vPrimaryToolbarAnimationSpringVelocity;

extern const CGFloat vStickyToolbarAnimationDuration;
extern const CGFloat vStickyToolbarAnimationDamping;
extern const CGFloat vStickyToolbarAnimationSpringVelocity;
extern const CGFloat vStickyToolbarExpandedScale;
extern const CGFloat vStickyToolbarExpandedAlpha;
extern const CGFloat vStickyToolbarCollapsedScale;
extern const CGFloat vStickyToolbarCollapsedAlpha;

#pragma mark - ICONS

extern NSString* vToolbarPanelButtonIcon;
extern NSString* vToolbarSearchButtonIcon;

#endif  // IOS_UI_TOOLBAR_VIVALDI_TOOLBAR_CONSTANTS_H_
