// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/chrome/browser/ui/tab_strip/vivaldi_tab_strip_constants.h"

#import <UIKit/UIKit.h>

#pragma mark - COLORS

// The default color of the tab strip background
NSString* const vTabStripDefaultBackgroundColor =
    @"vivaldi_tab_strip_background_color";
// The color for tab view background when a tab is selected
NSString* const vTabViewSelectedBackgroundColor =
    @"vivaldi_tab_view_selected_color";
// The color for tab view background for tabs not selected
NSString* const vTabViewNotSelectedBackgroundColor =
    @"vivaldi_tab_view_not_selected_color";
// The tint color for the tab view items when the tab is selected
NSString* const vTabViewSelectedTintColor =
    @"vivaldi_tab_view_selected_tint_color";
// The tint color for the tab view items when the tabs are not selected
NSString* const vTabViewNotSelectedTintColor =
    @"vivaldi_tab_view_not_selected_tint_color";
// Opacity for background tab view for dark tint color
CGFloat const vTabViewDarkTintOpacity = 0.2;
// Opacity for background tab view for light tint color
CGFloat const vTabViewLightTintOpacity = 0.2;


// Corner radius for vivaldi tab view background
CGFloat const vTabViewBackgroundCornerRadius = 6.f;
// Tab overlap padding for unstacked state
CGFloat const vTabOverlapUnstacked = 27;
// Padding for the vivaldi tab view background when address bar is on top.
// In order - (Top, Left, Bottom, Right)
UIEdgeInsets const vTabViewBackgroundPaddingTopAddressBar =
    UIEdgeInsetsMake(0.f, 14.f, -1.f, 14.f);
// Padding for the vivaldi tab view background when address bar is on bottom.
UIEdgeInsets const vTabViewBackgroundPaddingBottomAddressBar =
    UIEdgeInsetsMake(-1.f, 14.f, 0.f, 14.f);
// Padding for the desktop tabs new tab button.
UIEdgeInsets const vNewTabButtonPadding = UIEdgeInsetsMake(0, 8, 0, 4);
