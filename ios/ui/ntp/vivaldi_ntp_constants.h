// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_VIVALDI_NTP_CONSTANTS_H_
#define IOS_UI_NTP_VIVALDI_NTP_CONSTANTS_H_

#import <UIKit/UIKit.h>

#pragma mark - NOTIFICATION
extern NSString* vNTPShowOmniboxPopupOnFocus;
extern NSString* vNTPShowOmniboxPopupOnFocusBoolKey;

#pragma mark - SIZE AND PADDINGS
// Vivaldi location bar leading padding
extern const CGFloat vLocationBarLeadingPadding;
// Height of the search bar within the top toolbar.
extern const CGFloat vNTPSearchBarHeight;
// This is deducted from the actual height of the search bar to keep the height
// consistent with the Chromium omnibox while transitioning from Vivaldi start
// page to chromium omnibox.
extern const CGFloat vNTPSearchBarHeightOffset;
// Search bar corner radius
extern const CGFloat vNTPSearchBarCornerRadius;
// Search bar padding
extern const UIEdgeInsets vNTPSearchBarPadding;
// Search bar search icon padding
extern const UIEdgeInsets vNTPSearchBarSearchIconPadding;
// Search bar search icon size
extern const CGSize vNTPSearchBarSearchIconSize;
// Padding for Vivaldi Menu button
extern const UIEdgeInsets vNTPVivaldiMenuButtonPadding;
// Padding for Vivaldi Menu button
extern const CGSize vNTPVivaldiMenuButtonSize;

#pragma mark - COLORS
// Color for the regular tab page background
extern NSString* const vNTPBackgroundColor;
// Color for the private tab page background
extern NSString* const vPrivateNTPBackgroundColor;
// Color for the search bar background
extern NSString* const vSearchbarBackgroundColor;
// Color for regular tab toolbar background
extern NSString* const vRegularToolbarBackgroundColor;
// Color for private mode toolbar background
extern NSString* const vPrivateModeToolbarBackgroundColor;
// Color for private mode selected tab view background color
extern NSString* const vPrivateModeTabSelectedBackgroundColor;
// Color for the new tab page toolbar selection underline
extern NSString* const vNTPToolbarSelectionLineColor;
// Color for the new tab page toolbar item when not selected or highlighted
extern NSString* const vNTPToolbarTextColor;
// Color for the new tab page toolbar item when selected or highlighted
extern NSString* const vNTPToolbarTextHighlightedColor;
// Color for the new tab page toolbar more button light tint
extern NSString* const vNTPToolbarMoreLightTintColor;
// Color for the new tab page toolbar more button dark tint
extern NSString* const vNTPToolbarMoreDarkTintColor;

#pragma mark - ICONS
// Image names for the search icon.
extern NSString* vNTPSearchIcon;
// Image name for toolbar more icon
extern NSString* vNTPToolbarMoreIcon;
// Image name for add new speed dial
extern NSString* vNTPAddNewSpeedDialIcon;
// Image name for speed dial folder
extern NSString* vNTPSpeedDialFolderIcon;
// Image name for private mode page background
extern NSString* vNTPPrivateTabBG;
// Image name for private mode page ghost
extern NSString* vNTPPrivateTabGhost;
// Image name for the icon on the speed dial empty view
extern NSString* vNTPSDEmptyViewIcon;

#endif  // IOS_UI_NTP_VIVALDI_NTP_CONSTANTS_H_
