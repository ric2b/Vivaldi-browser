// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_CONSTANTS_VIVALDI_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_CONSTANTS_VIVALDI_H_

#import <Foundation/Foundation.h>

// Notifications
extern NSString* const vTabGridWillCloseAllTabsNotification;

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

// Assets for inactive tabs
extern NSString* vTabGridInactiveTabsEduIcon;

// Colors for grid cell item
extern NSString* const vTabGridSelectedColor;
extern NSString* const vTabGridNotSelectedColor;

// Edge padding for tab grid empty state view label
extern CGFloat const vTabGridEmptyStateViewContainerPadding;
// Border width for selected cell
extern CGFloat const vTabGridSelectedBorderWidth;
// Border width for not selected cell
extern CGFloat const vTabGridNotSelectedBorderWidth;
// Aspect Ratio for portrait mode.
extern CGFloat const vTabGridPortraitAspectRatio;
// Aspect Ratio for landscape mode on iPhone.
extern CGFloat const vTabGridLandscapeAspectRatio;
// Width thresholds for determining the columns count for
// large width screen bounds.
extern CGFloat const vTabGridLargeWidthThreshold;

// Section header height for recent and sync tabs.
extern const CGFloat vSectionHeaderHeight;

// Button corner radius for login button in empty state
extern const CGFloat vRecentTabsEmptyStateLoginButtonCornerRadius;
// Button height for login button in empty state
extern const CGFloat vRecentTabsEmptyStateLoginButtonHeight;

// Pinned tabs view dimension
extern const CGFloat vPinnedViewHorizontalPadding;
extern const CGFloat vPinnedCellHeight;
extern const CGFloat vPinnedCellHorizontalLayoutInsets;

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_CONSTANTS_VIVALDI_H_
