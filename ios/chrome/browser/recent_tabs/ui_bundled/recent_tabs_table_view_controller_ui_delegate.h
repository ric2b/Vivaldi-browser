// Copyright 2025 Vivaldi Technologies. All rights reserved.

#ifndef IOS_CHROME_BROWSER_RECENT_TABS_UI_BUNDLED_RECENT_TABS_TABLE_VIEW_CONTROLLER_UI_DELEGATE_H_
#define IOS_CHROME_BROWSER_RECENT_TABS_UI_BUNDLED_RECENT_TABS_TABLE_VIEW_CONTROLLER_UI_DELEGATE_H_

@class RecentTabsTableViewController;

// Delegate for the RecentTabsTableViewController for all UI-related events.
@protocol RecentTabsTableViewControllerUIDelegate

// Tells the delegate that the scroll view scrolled.
- (void)recentTabsScrollViewDidScroll:
    (RecentTabsTableViewController*)recentTabsTableViewController;

- (void)recentTabsSyncStateDidEnable:(BOOL)syncEnabled;

@end

#endif  // IOS_CHROME_BROWSER_RECENT_TABS_UI_BUNDLED_RECENT_TABS_TABLE_VIEW_CONTROLLER_UI_DELEGATE_H_
