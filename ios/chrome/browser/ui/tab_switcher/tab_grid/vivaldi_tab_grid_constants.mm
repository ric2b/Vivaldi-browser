// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/vivaldi_tab_grid_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Accessibility identifiers for automated testing.
NSString* const vTabGridRecentlyClosedTabsPageButtonIdentifier =
    @"TabGridRecentlyClosedTabsPageButtonIdentifier";
NSString* const vTabGridSyncedTabsEmptyStateIdentifier =
    @"TabGridSyncedTabsEmptyStateIdentifier";
NSString* const vTabGridRecentlyClosedTabsEmptyStateIdentifier =
    @"TabGridRecentlyClosedTabsEmptyStateIdentifier";

// The color of the text buttons in the toolbars.
NSString* const vTabGridToolbarTextButtonColor =
    @"grid_toolbar_text_button_color";

// Colors for the empty state and disabled tab view.
NSString* const vTabGridEmptyStateTitleTextColor =
    @"grid_empty_state_title_text_color";
NSString* const vTabGridEmptyStateBodyTextColor =
    @"grid_empty_state_body_text_color";
NSString* const vTabGridEmptyStateLoginButtonBGColor =
    @"grid_empty_state_login_button_bg_color";

// Assets for the empty state
NSString* vTabGridEmptyStateRegularTabsImage = @"tab_grid_regular_tabs_empty";
NSString* vTabGridEmptyStatePrivateTabsImage = @"tab_grid_incognito_tabs_empty";
NSString* vTabGridEmptyStateSyncedTabsImage = @"tab_grid_remote_tabs_empty";
NSString* vTabGridEmptyStateClosedTabsImage = @"tab_grid_closed_tabs_empty";

// Colors for grid cell item
NSString* const vTabGridSelectedColor =
    @"grid_selected_color";
NSString* const vTabGridNotSelectedColor =
    @"grid_not_selected_color";

// Edge padding for tab grid empty state view label
CGFloat const vTabGridEmptyStateViewContainerPadding = 16.0;
// Border width for selected cell
CGFloat const vTabGridSelectedBorderWidth = 3.0;
// Border width for not selected cell
CGFloat const vTabGridNotSelectedBorderWidth = 2.0;
// Tab grid collection top padding
const CGFloat vTabGridCollectionTopPadding = 24.0;
// Tab grid collection bottom padding
const CGFloat vTabGridCollectionBottomPadding = 24.0;
// Tab grid item size multiplier for iPad
const CGFloat vTabGridItemSizeMultiplieriPad = 0.25;
// Tab grid item size multiplier for iPhone portrait
const CGFloat vTabGridItemSizeMultiplieriPhonePortrait = 0.5;
// Tab grid item size multiplier for iPhone landscape
const CGFloat vTabGridItemSizeMultiplieriPhoneLandscape = 0.25;
// Tab grid item padding for iPhone
const CGFloat vTabGridItemPaddingiPhone = 8.0;
// Tab grid item padding for iPad
const CGFloat vTabGridItemPaddingiPad = 12.0;
// Tab grid section padding for iPhone portrait
const CGFloat vTabGridSectionPaddingiPhonePortrait = 8.0;
// Tab grid section padding for iPhone landscape
const CGFloat vTabGridSectionPaddingiPhoneLandscape = 36.0;
// Tab grid section padding for iPad portrait
const CGFloat vTabGridSectionPaddingiPadPortrait = 64.0;
// Tab grid section padding for iPad landscape
const CGFloat vTabGridSectionPaddingiPadLandscape = 64.0;

// Section header height for recent and sync tabs.
const CGFloat vSectionHeaderHeight = 24;

// Button corner radius for login button in empty state
const CGFloat vRecentTabsEmptyStateLoginButtonCornerRadius = 8.0;
// Button height for login button in empty state
const CGFloat vRecentTabsEmptyStateLoginButtonHeight = 52.0;
