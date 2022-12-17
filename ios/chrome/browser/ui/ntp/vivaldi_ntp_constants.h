// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_CHROME_BROWSER_UI_VIVALDI_NEW_TAB_PAGE_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_VIVALDI_NEW_TAB_PAGE_CONSTANTS_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

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
// Speed dial container top padding
extern const CGFloat vSpeedDialContainerTopPadding;
// Speed Dial item corner radius
extern const CGFloat vSpeedDialItemCornerRadius;
// Speed Dial item shadow offset
extern const CGSize vSpeedDialItemShadowOffset;
// Speed Dial item shadow radius
extern const CGFloat vSpeedDialItemShadowRadius;
// Speed Dial item shadow opacity
extern const CGFloat vSpeedDialItemShadowOpacity;
// Speed Dial folder icon
extern const CGSize vSpeedDialFolderIconSize;
// Speed Dial item favicon size
extern const CGSize vSpeedDialItemFaviconSize;
// Speed Dial favicon corner radius
extern const CGFloat vSpeedDialFaviconCornerRadius;
// Speed dial container top padding
extern const CGFloat vSDContainerTopPadding;
// Speed dial container bottom padding
extern const CGFloat vSDContainerBottomPadding;
// Speed dial item size multiplier for iPad
extern const CGFloat vSDItemSizeMultiplieriPad;
// Speed dial item size multiplier for iPhone portrait
extern const CGFloat vSDItemSizeMultiplieriPhonePortrait;
// Speed dial item size multiplier for iPhone landscape
extern const CGFloat vSDItemSizeMultiplieriPhoneLandscape;
// Speed dial item padding for iPhone
extern const CGFloat vSDItemPaddingiPhone;
// Speed dial item padding for iPad
extern const CGFloat vSDItemPaddingiPad;
// Speed dial section padding for iPhone portrait
extern const CGFloat vSDSectionPaddingiPhonePortrait;
// Speed dial section padding for iPhone landscape
extern const CGFloat vSDSectionPaddingiPhoneLandscape;
// Speed dial section padding for iPad portrait
extern const CGFloat vSDSectionPaddingiPortrait;
// Speed dial section padding for iPad landscape
extern const CGFloat vSDSectionPaddingiPadLandscape;

#pragma mark - FONT SIZE
extern const CGFloat vHeaderFontSize;
extern const CGFloat vBodyFontSize;
extern const CGFloat vBody1FontSize;
extern const CGFloat vBody2FontSize;

#pragma mark - COLORS
// Color for the new tab page background
extern NSString* const vNTPBackgroundColor;
// Color for the search bar background
extern NSString* const vSearchbarBackgroundColor;
// Color for the search bar text
extern NSString* const vSearchbarTextColor;
// Color for the new tab page speed dial container
extern NSString* const vNTPSpeedDialContainerbackgroundColor;
// Color for the new tab page speed dial grid cells
extern NSString* const vNTPSpeedDialCellBackgroundColor;
// Color for the new tab page speed dial domain/website name text
extern NSString* const vNTPSpeedDialDomainTextColor;
// Color for the new tab page toolbar selection underline
extern NSString* const vNTPToolbarSelectionLineColor;
// Color for the new tab page toolbar item when not selected or highlighted
extern NSString* const vNTPToolbarTextColor;
// Color for the new tab page toolbar item when selected or highlighted
extern NSString* const vNTPToolbarTextHighlightedColor;
// Color for the shadow of the speed dial item
extern NSString* const vSpeedDialItemShadowColor;

#pragma mark - ICONS
// Image names for the search icon.
extern NSString* vNTPSearchIcon;
// Image name for toolbar sort icon
extern NSString* vNTPToolbarSortIcon;
// Image name for add new speed dial
extern NSString* vNTPAddNewSpeedDialIcon;
// Image name for speed dial folder
extern NSString* vNTPSpeedDialFolderIcon;
// Image name for private mode page background
extern NSString* vNTPPrivateTabBG;
// Image name for private mode page ghost
extern NSString* vNTPPrivateTabGhost;
#endif  // IOS_CHROME_BROWSER_UI_VIVALDI_NEW_TAB_PAGE_CONSTANTS_H_
