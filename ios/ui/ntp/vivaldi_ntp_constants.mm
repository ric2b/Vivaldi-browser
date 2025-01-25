// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/vivaldi_ntp_constants.h"

#pragma mark - NOTIFICATION
NSString* vNTPShowOmniboxPopupOnFocus = @"vNTPShowOmniboxPopupOnFocus";
NSString* vNTPShowOmniboxPopupOnFocusBoolKey =
    @"vNTPShowOmniboxPopupOnFocusBoolKey";

#pragma mark - SIZE AND PADDINGS
// Vivaldi location bar leading padding
const CGFloat vLocationBarLeadingPadding = 16.0;
// Height of the search bar within the top toolbar
const CGFloat vNTPSearchBarHeight = 52.0;
// This is deducted from the actual height of the search bar to keep the height
// consistent with the Chromium omnibox while transitioning from Vivaldi start
// page to chromium omnibox.
const CGFloat vNTPSearchBarHeightOffset = 10.0;
// Search bar corner radius
const CGFloat vNTPSearchBarCornerRadius = 6.0;
// Search bar padding
// In order - Top, Left, Bottom, Right
const UIEdgeInsets vNTPSearchBarPadding =
  UIEdgeInsetsMake(4.0, 16.0, 0.0, 16.0);
// Search bar search icon padding
// In order - Top, Left, Bottom, Right
const UIEdgeInsets vNTPSearchBarSearchIconPadding =
  UIEdgeInsetsMake(0.0, 0.0, 0.0, 8.0);
// Search bar search icon size
// In order - Width, Height
const CGSize vNTPSearchBarSearchIconSize = CGSizeMake(16.0, 16.0);
// Padding for Vivaldi Menu button
const UIEdgeInsets vNTPVivaldiMenuButtonPadding =
  UIEdgeInsetsMake(0.f, 16.f, 0.f, 18.f);
// Padding for Vivaldi Menu button
const CGSize vNTPVivaldiMenuButtonSize = CGSizeMake(28.f, 28.f);

#pragma mark - COLORS
// Color for the regular tab page background
NSString* const vNTPBackgroundColor =
    @"vivaldi_ntp_background_color";
// Color for the private tab page background
NSString* const vPrivateNTPBackgroundColor =
    @"vivaldi_private_ntp_background_color";
// Color for the search bar background
NSString* const vSearchbarBackgroundColor =
    @"vivaldi_ntp_searchbar_background_color";
// Color for regular tab toolbar background
NSString* const vRegularToolbarBackgroundColor =
    @"vivaldi_regular_toolbar_background_color";
// Color for private mode toolbar background
NSString* const vPrivateModeToolbarBackgroundColor =
    @"vivaldi_private_toolbar_background_color";
// Color for private mode selected tab view
NSString* const vPrivateModeTabSelectedBackgroundColor =
    @"vivaldi_private_tabview_selected_background_color";
// Color for the new tab page toolbar selection underline
NSString* const vNTPToolbarSelectionLineColor =
    @"vivaldi_ntp_toolbar_selectionline_color";
// Color for the new tab page toolbar item when not selected or highlighted
NSString* const vNTPToolbarTextColor =
    @"vivaldi_ntp_toolbar_text_color";
// Color for the new tab page toolbar item when selected or highlighted
NSString* const vNTPToolbarTextHighlightedColor =
    @"vivaldi_ntp_toolbar_text_highlighted_color";
// Color for the new tab page toolbar more button light tint
NSString* const vNTPToolbarMoreLightTintColor =
    @"vivaldi_ntp_toolbar_more_light_color";
// Color for the new tab page toolbar more button dark tint
NSString* const vNTPToolbarMoreDarkTintColor =
    @"vivaldi_ntp_toolbar_more_dark_color";

#pragma mark - ICONS
// Image names for the search icon.
NSString* vNTPSearchIcon = @"vivaldi_ntp_search";
// Image name for toolbar more icon
NSString* vNTPToolbarMoreIcon = @"vivaldi_ntp_toolbar_more";
// Image name for private mode page background
NSString* vNTPPrivateTabBG = @"vivaldi_private_tab_bg";
// Image name for private mode page ghost
NSString* vNTPPrivateTabGhost = @"vivaldi_private_tab_ghost";
