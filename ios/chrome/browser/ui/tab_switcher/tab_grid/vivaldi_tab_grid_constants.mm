// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/vivaldi_tab_grid_constants.h"

// Notifications
NSString* const vTabGridWillCloseAllTabsNotification =
    @"vTabGridWillCloseAllTabsNotification";

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

// Assets for inactive tabs
NSString* vTabGridInactiveTabsEduIcon = @"vivaldi_inactive_tabs_edu_icon";

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
// Aspect Ratio for portrait mode.
CGFloat const vTabGridPortraitAspectRatio = 6. / 5.;
// Aspect Ratio for landscape mode on iPhone.
CGFloat const vTabGridLandscapeAspectRatio = 6. / 8.;
// Width thresholds for determining the columns count for
// large width screen bounds.
CGFloat const vTabGridLargeWidthThreshold = 800;

// Section header height for recent and sync tabs.
const CGFloat vSectionHeaderHeight = 24;

// Button corner radius for login button in empty state
const CGFloat vRecentTabsEmptyStateLoginButtonCornerRadius = 8.0;
// Button height for login button in empty state
const CGFloat vRecentTabsEmptyStateLoginButtonHeight = 52.0;

// Pinned tabs view dimension
const CGFloat vPinnedViewHorizontalPadding = 16;
const CGFloat vPinnedCellHeight = 40.0f;
const CGFloat vPinnedCellHorizontalLayoutInsets = 12.0f;
