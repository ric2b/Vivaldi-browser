// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef VIVALDI_BROWSER_UI_TAB_STRIP_CONSTANTS_H_
#define VIVALDI_BROWSER_UI_TAB_STRIP_CONSTANTS_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#pragma mark - COLORS
// The default color of the tab strip background
extern NSString* const vTabStripDefaultBackgroundColor;
// The color for tab view background when a tab is selected
extern NSString* const vTabViewSelectedBackgroundColor;
// The color for tab view background for tabs not selected
extern NSString* const vTabViewNotSelectedBackgroundColor;
// The tint color for the tab view items when the tab is selected
extern NSString* const vTabViewSelectedTintColor;
// The tint color for the tab view items when the tabs are not selected
extern NSString* const vTabViewNotSelectedTintColor;
// Opacity for background tab view for dark tint color
extern CGFloat const vTabViewDarkTintOpacity;
// Opacity for background tab view for light tint color
extern CGFloat const vTabViewLightTintOpacity;

#pragma mark - SIZES
// Corner radius for vivaldi tab view background
extern CGFloat const vTabViewBackgroundCornerRadius;
// Tab overlap padding for unstacked state
extern CGFloat const vTabOverlapUnstacked;
// Padding for the vivaldi tab view background when address bar is on top.
// In order - (Top, Left, Bottom, Right)
extern UIEdgeInsets const vTabViewBackgroundPaddingTopAddressBar;
// Padding for the vivaldi tab view background when address bar is on bottom.
extern UIEdgeInsets const vTabViewBackgroundPaddingBottomAddressBar;
// Padding for the desktop tabs new tab button.
extern UIEdgeInsets const vNewTabButtonPadding;

#endif  // VIVALDI_BROWSER_UI_TAB_STRIP_CONSTANTS_H_
