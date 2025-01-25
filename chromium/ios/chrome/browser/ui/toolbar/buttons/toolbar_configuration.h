// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_style.h"

// Toolbar configuration object giving access to styling elements.
@interface ToolbarConfiguration : NSObject

// Init the toolbar configuration with the desired `style`.
- (instancetype)initWithStyle:(ToolbarStyle)style NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Style of this configuration.
@property(nonatomic, readonly) ToolbarStyle style;

// Background color of the NTP. Used to do as if the toolbar was transparent and
// the NTP is visible behind it.
@property(nonatomic, readonly) UIColor* NTPBackgroundColor;

// Background color of the toolbar.
@property(nonatomic, readonly) UIColor* backgroundColor;

// Focused background color of the toolbar.
// Used only in Updated Popup treatment 2.
@property(nonatomic, readonly) UIColor* focusedBackgroundColor;

#if defined(VIVALDI_BUILD)
// Tint color of the buttons.
@property(nonatomic, readwrite) UIColor* buttonsTintColor;
#else
// Tint color of the buttons.
@property(nonatomic, readonly) UIColor* buttonsTintColor;
#endif  // VIVALDI_BUILD

// Tint color of the buttons in the highlighted state. This is only to be used
// if the button has a custom style.
@property(nonatomic, readonly) UIColor* buttonsTintColorHighlighted;

// Tint color of the buttons when they are highlighted for an IPH;
@property(nonatomic, readonly) UIColor* buttonsTintColorIPHHighlighted;

// Color for the background view when the button is highlighted for an IPH.
@property(nonatomic, readonly) UIColor* buttonsIPHHighlightColor;

// Used as Omnibox background color when focused.
// Used only in Updated Popup treatment 2.
// See locationBarBackgroundColorWithVisibility: below for defocused.
@property(nonatomic, readonly) UIColor* focusedLocationBarBackgroundColor;

// Returns the background color of the location bar, with a `visibilityFactor`.
// The `visibilityFactor` is here to alter the alpha value of the background
// color. Even with a `visibilityFactor` of 1, the final color could is
// translucent.
- (UIColor*)locationBarBackgroundColorWithVisibility:(CGFloat)visibilityFactor;

// Returns the appropriate accessibility label to set on the Open New Tab
// button, based on whether the current tab is grouped or not.
- (NSString*)accessibilityLabelForOpenNewTabButtonInGroup:(BOOL)inGroup;

// Vivaldi
/// Returns the default accent color for the primary toolbar.
@property(nonatomic, readonly) UIColor* primaryToolbarAccentColor;
/// Returns the background color for omnibox which is calculated
/// from the current accent color.
- (UIColor*)locationBarBackgroundColorForAccentColor:(UIColor*)accentColor;
/// Returns the steady view contents tint color from the current accent color.
- (UIColor*)locationBarSteadyViewTintColorForAccentColor:(UIColor*)accentColor;
/// Returns the toolbar buttons tint color from the current accent color.
- (UIColor*)buttonsTintColorForAccentColor:(UIColor*)accentColor;
// End Vivaldi

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_CONFIGURATION_H_
