// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/chrome/browser/ui/ntp/vivaldi_ntp_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
// Speed dial container top padding
const CGFloat vSpeedDialContainerTopPadding = 8.0;
// Speed Dial item corner radius
const CGFloat vSpeedDialItemCornerRadius = 8.0;
// Speed Dial item shadow offset
const CGSize vSpeedDialItemShadowOffset = CGSizeMake(0.0, 1.0);
// Speed Dial item shadow radius
const CGFloat vSpeedDialItemShadowRadius = 3.0;
// Speed Dial item shadow opacity
const CGFloat vSpeedDialItemShadowOpacity = 1.0;
// Speed Dial folder icon
// In order - Width, Height
const CGSize vSpeedDialFolderIconSize = CGSizeMake(56.0, 56.0);
// Speed Dial item favicon size
const CGSize vSpeedDialItemFaviconSize = CGSizeMake(16.0, 16.0);
// Speed Dial favicon corner radius
const CGFloat vSpeedDialFaviconCornerRadius = 4.0;

// Speed dial container top padding
const CGFloat vSDContainerTopPadding = 24.0;
// Speed dial container bottom padding
const CGFloat vSDContainerBottomPadding = 24.0;
// Speed dial item size multiplier for iPad
const CGFloat vSDItemSizeMultiplieriPad = 0.25;
// Speed dial item size multiplier for iPhone portrait
const CGFloat vSDItemSizeMultiplieriPhonePortrait = 0.5;
// Speed dial item size multiplier for iPhone landscape
const CGFloat vSDItemSizeMultiplieriPhoneLandscape = 0.25;
// Speed dial item padding for iPhone
const CGFloat vSDItemPaddingiPhone = 8.0;
// Speed dial item padding for iPad
const CGFloat vSDItemPaddingiPad = 12.0;
// Speed dial section padding for iPhone portrait
const CGFloat vSDSectionPaddingiPhonePortrait = 8.0;
// Speed dial section padding for iPhone landscape
const CGFloat vSDSectionPaddingiPhoneLandscape = 36.0;
// Speed dial section padding for iPad portrait
const CGFloat vSDSectionPaddingiPortrait = 64.0;
// Speed dial section padding for iPad landscape
const CGFloat vSDSectionPaddingiPadLandscape = 250.0;

#pragma mark - FONT SIZE
const CGFloat vHeaderFontSize = 18.0;
const CGFloat vBodyFontSize = 16.0;
const CGFloat vBody1FontSize = 14.0;
const CGFloat vBody2FontSize = 12.0;

#pragma mark - COLORS
// Color for the new tab page background
NSString* const vNTPBackgroundColor =
    @"vivaldi_ntp_background_color";
// Color for the search bar background
NSString* const vSearchbarBackgroundColor =
    @"vivaldi_ntp_searchbar_background_color";
// Color for the search bar text
NSString* const vSearchbarTextColor = @"vivaldi_ntp_searchbar_text_color";
// Color for the new tab page speed dial container
NSString* const vNTPSpeedDialContainerbackgroundColor =
    @"vivaldi_ntp_speed_dial_container_background_color";
// Color for the new tab page speed dial grid cells
NSString* const vNTPSpeedDialCellBackgroundColor =
    @"vivaldi_ntp_speed_dial_cell_background_color";
// Color for the new tab page speed dial domain/website name text
NSString* const vNTPSpeedDialDomainTextColor =
    @"vivaldi_ntp_speed_dial_domain_text_color";
// Color for the new tab page toolbar selection underline
NSString* const vNTPToolbarSelectionLineColor =
    @"vivaldi_ntp_toolbar_selectionline_color";
// Color for the new tab page toolbar item when not selected or highlighted
NSString* const vNTPToolbarTextColor =
    @"vivaldi_ntp_toolbar_text_color";
// Color for the new tab page toolbar item when selected or highlighted
NSString* const vNTPToolbarTextHighlightedColor =
    @"vivaldi_ntp_toolbar_text_highlighted_color";
// Color for the shadow of the speed dial item
NSString* const vSpeedDialItemShadowColor =
    @"vivaldi_ntp_speed_dial_cell_shadow_color";

#pragma mark - ICONS
// Image names for the search icon.
NSString* vNTPSearchIcon = @"vivaldi_ntp_search";
// Image name for toolbar sort icon
NSString* vNTPToolbarSortIcon = @"vivaldi_ntp_toolbar_sort";
// Image name for add new speed dial
NSString* vNTPAddNewSpeedDialIcon = @"vivaldi_ntp_add_new_speed_dial";
// Image name for add new speed dial folder
NSString* vNTPSpeedDialFolderIcon =
    @"vivaldi_ntp_add_new_speed_dial_folder";
// Image name for private mode page background
NSString* vNTPPrivateTabBG = @"vivaldi_private_tab_bg";
// Image name for private mode page ghost
NSString* vNTPPrivateTabGhost = @"vivaldi_private_tab_ghost";
