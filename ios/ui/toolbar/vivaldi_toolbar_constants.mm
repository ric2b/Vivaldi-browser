// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/toolbar/vivaldi_toolbar_constants.h"

#pragma mark - SIZE AND PADDINGS

const CGFloat vPrimaryToolbarHorizontalPadding = 4;
const CGFloat vPrimaryToolbarTextFieldLeadingOffsetNoImage = 0;
const CGFloat vPrimaryToolbarTextFieldLeadingOffsetImage = 2;
const CGFloat vPrimaryToolbarLeadingStackViewWidthNoItems = 9;
const CGFloat vPrimaryToolbarLocationContainerTrailingPadding = 8;

// Secondary toolbar
const CGFloat vBottomButtonsTopMargin = 8.0;
const CGFloat vBottomAdaptiveLocationBarTopMargin = 10.0;
const CGFloat vBottomAdaptiveLocationBarBottomMargin = 12.0;
const CGFloat vAdaptiveToolbarMargin = 4.0;
const CGFloat vBottomToolbarSteadyViewTopPadding = 2.0;
const CGFloat vBottomToolbarSteadyViewTopPaddingFullScreen = 1.0;

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
NSString* vToolbarHomeButtonIcon = @"toolbar_home";
NSString* vToolbarForwardButtonIcon = @"toolbar_forward";
NSString* vToolbarBackButtonIcon = @"toolbar_back";
NSString* vToolbarTabSwitcherButtonIcon = @"toolbar_switcher";
NSString* vToolbarMoreButtonIcon = @"toolbar_more";
NSString* vToolbarNTPButtonIcon = @"toolbar_new_tab_page";
NSString* vToolbarTabSwitcherOveflowButtonIcon = @"toolbar_switcher_overflow";

NSString* vToolbarButtonColor = @"toolbar_button_color";
NSString* vToolbarLightButton = @"toolbar_button_color_light";
NSString* vToolbarDarkButton = @"toolbar_button_color_dark";

NSString* vToolbarMoveToTop = @"toolbar_move_to_top";
NSString* vToolbarMoveToBottom = @"toolbar_move_to_bottom";
NSString* vToolbarCopyLink = @"toolbar_copy_link";
NSString* vToolbarPaste = @"toolbar_paste";
