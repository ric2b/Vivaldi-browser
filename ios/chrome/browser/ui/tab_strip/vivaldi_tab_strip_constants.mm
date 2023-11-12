// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/chrome/browser/ui/tab_strip/vivaldi_tab_strip_constants.h"

#import <Foundation/Foundation.h>
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


// Corner radius for vivaldi tab view background
CGFloat const vTabViewBackgroundCornerRadius = 6.f;
// Padding for the vivaldi tab view background.
// In order - (Top, Left, Bottom, Right)
UIEdgeInsets const vTabViewBackgroundPadding =
    UIEdgeInsetsMake(0.f, 14.f, -1.f, 14.f);
// Padding for the desktop tabs new tab button.
UIEdgeInsets const vNewTabButtonPadding = UIEdgeInsetsMake(0, 8, 0, 4);
