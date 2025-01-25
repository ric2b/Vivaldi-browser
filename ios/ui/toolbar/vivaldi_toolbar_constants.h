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
// Width for leading stack view when no buttons present i.e. iPhone portrait
// mode.
extern const CGFloat vPrimaryToolbarLeadingStackViewWidthNoItems;
// Location container trailing space for contracted state.
extern const CGFloat vPrimaryToolbarLocationContainerTrailingPadding;

// Seconary toolbar
extern const CGFloat vBottomButtonsTopMargin;
extern const CGFloat vBottomAdaptiveLocationBarTopMargin;
extern const CGFloat vBottomAdaptiveLocationBarBottomMargin;
extern const CGFloat vAdaptiveToolbarMargin;
extern const CGFloat vBottomToolbarSteadyViewTopPadding;
extern const CGFloat vBottomToolbarSteadyViewTopPaddingFullScreen;

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
extern NSString* vToolbarHomeButtonIcon;
extern NSString* vToolbarForwardButtonIcon;
extern NSString* vToolbarBackButtonIcon;
extern NSString* vToolbarTabSwitcherButtonIcon;
extern NSString* vToolbarMoreButtonIcon;
extern NSString* vToolbarNTPButtonIcon;
extern NSString* vToolbarTabSwitcherOveflowButtonIcon;

extern NSString* vToolbarButtonColor;
// Used over dark background
extern NSString* vToolbarLightButton;
// Used over light background
extern NSString* vToolbarDarkButton;

extern NSString* vToolbarMoveToTop;
extern NSString* vToolbarMoveToBottom;
extern NSString* vToolbarCopyLink;
extern NSString* vToolbarPaste;

#endif  // IOS_UI_TOOLBAR_VIVALDI_TOOLBAR_CONSTANTS_H_
