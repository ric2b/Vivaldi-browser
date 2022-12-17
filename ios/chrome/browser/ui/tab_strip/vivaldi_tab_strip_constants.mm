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
// The image for primary toolbar panel button
NSString* const vPrimaryToolbarPanelButton =
    @"vivaldi_primary_toolbar_panel_button";


// Corner radius for vivaldi tab view background
CGFloat const vTabViewBackgroundCornerRadius = 6.f;
// Padding for the vivaldi tab view background.
// In order - (Top, Left, Bottom, Right)
UIEdgeInsets const vTabViewBackgroundPadding =
    UIEdgeInsetsMake(0.f, 16.f, -1.f, 16.f);
