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

#pragma mark - SIZES
// Corner radius for vivaldi tab view background
extern CGFloat const vTabViewBackgroundCornerRadius;
// Padding for the vivaldi tab view background.
// In order - (Top, Left, Bottom, Right)
extern UIEdgeInsets const vTabViewBackgroundPadding;
// Padding for the desktop tabs new tab button.
extern UIEdgeInsets const vNewTabButtonPadding;

#endif  // VIVALDI_BROWSER_UI_TAB_STRIP_CONSTANTS_H_
