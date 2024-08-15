// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_EMPTY_STATE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_EMPTY_STATE_VIEW_H_

#include <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_empty_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"

// Vivaldi
#import "ios/chrome/browser/ui/recent_tabs/sessions_sync_user_state.h"

@protocol VivaldiTabGridEmptyStateViewDelegate;
// End Vivaldi

// A view that informs the user that the grid is empty. The displayed
// text is customized for incognito and regular tabs pages. No text is
// displayed for the remote tabs page.
@interface TabGridEmptyStateView : UIView <GridEmptyView>

// Initializes view with `page`, which changes the displayed text.
- (instancetype)initWithPage:(TabGridPage)page NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Vivaldi
// Toggle the visibility of action buttons for different user and sync states
// in synced tabs.
// Also show message and title based on user and sync state.
- (void)updateEmptyViewWithUserState:(SessionsSyncUserState)state;

@property (nonatomic, weak) id<VivaldiTabGridEmptyStateViewDelegate> delegate;

// End Vivaldi

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_EMPTY_STATE_VIEW_H_
