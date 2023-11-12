// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_CONSTANTS_VIVALDI_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_CONSTANTS_VIVALDI_H_

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

// Accessibility identifiers for automated testing.
extern NSString* const vTabGridRecentlyClosedTabsPageButtonIdentifier;
extern NSString* const vTabGridSyncedTabsEmptyStateIdentifier;
extern NSString* const vTabGridRecentlyClosedTabsEmptyStateIdentifier;

// The color of the text buttons in the toolbars.
extern NSString* const vTabGridToolbarTextButtonColor;

// Colors for the empty state.
extern NSString* const vTabGridEmptyStateTitleTextColor;
extern NSString* const vTabGridEmptyStateBodyTextColor;
extern NSString* const vTabGridEmptyStateLoginButtonBGColor;

// Assets for the empty state
extern NSString* vTabGridEmptyStateRegularTabsImage;
extern NSString* vTabGridEmptyStatePrivateTabsImage;
extern NSString* vTabGridEmptyStateSyncedTabsImage;
extern NSString* vTabGridEmptyStateClosedTabsImage;

// Colors for grid cell item
extern NSString* const vTabGridSelectedColor;
extern NSString* const vTabGridNotSelectedColor;

// Edge padding for tab grid empty state view label
extern CGFloat const vTabGridEmptyStateViewContainerPadding;
// Border width for selected cell
extern CGFloat const vTabGridSelectedBorderWidth;
// Border width for not selected cell
extern CGFloat const vTabGridNotSelectedBorderWidth;
// Tab grid collection top padding
extern const CGFloat vTabGridCollectionTopPadding;
// Tab grid collection bottom padding
extern const CGFloat vTabGridCollectionBottomPadding;
// Tab grid item size multiplier for iPad
extern const CGFloat vTabGridItemSizeMultiplieriPad;
// Tab grid item size multiplier for iPhone portrait
extern const CGFloat vTabGridItemSizeMultiplieriPhonePortrait;
// Tab grid item size multiplier for iPhone landscape
extern const CGFloat vTabGridItemSizeMultiplieriPhoneLandscape;
// Tab grid item padding for iPhone
extern const CGFloat vTabGridItemPaddingiPhone;
// Tab grid item padding for iPad
extern const CGFloat vTabGridItemPaddingiPad;
// Tab grid section padding for iPhone portrait
extern const CGFloat vTabGridSectionPaddingiPhonePortrait;
// Tab grid section padding for iPhone landscape
extern const CGFloat vTabGridSectionPaddingiPhoneLandscape;
// Tab grid section padding for iPad portrait
extern const CGFloat vTabGridSectionPaddingiPadPortrait;
// Tab grid section padding for iPad landscape
extern const CGFloat vTabGridSectionPaddingiPadLandscape;

// Section header height for recent and sync tabs.
extern const CGFloat vSectionHeaderHeight;

// Button corner radius for login button in empty state
extern const CGFloat vRecentTabsEmptyStateLoginButtonCornerRadius;
// Button height for login button in empty state
extern const CGFloat vRecentTabsEmptyStateLoginButtonHeight;

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_CONSTANTS_VIVALDI_H_
