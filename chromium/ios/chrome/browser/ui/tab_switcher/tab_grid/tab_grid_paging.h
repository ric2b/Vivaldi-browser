// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_PAGING_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_PAGING_H_

#if defined(VIVALDI_BUILD)
// Page enumerates the kinds of grouped tabs.
// Note(prio@vivaldi.com) - Closed tabs should be after remote tab in the enum
// since this also controls the tab switcher slider position, and scrollview
// insets based on the index. Tab groups do not have any UI as of now from
// upstream, so shift closed tabs one stpe up.
typedef NS_ENUM(NSUInteger, TabGridPage) {
  TabGridPageIncognitoTabs = 0,
  TabGridPageRegularTabs = 1,
  TabGridPageRemoteTabs = 2,
  TabGridPageClosedTabs = 3,
  TabGridPageTabGroups = 4
};
#else
// Page enumerates the kinds of grouped tabs.
typedef NS_ENUM(NSUInteger, TabGridPage) {
  TabGridPageIncognitoTabs = 0,
  TabGridPageRegularTabs = 1,
  TabGridPageRemoteTabs = 2,
  TabGridPageTabGroups = 3,
};
#endif // End Vivaldi

// Modes of the tab grid and its elements.
enum class TabGridMode {
  kNormal,
  kSelection,
  kSearch,
};

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_PAGING_H_
