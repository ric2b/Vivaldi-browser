// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARKS_SORTING_MODE_H_
#define IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARKS_SORTING_MODE_H_

// Page enumerates the modes of the Bookmarks sorting.
typedef NS_ENUM(NSUInteger, VivaldiBookmarksSortingMode) {
  VivaldiBookmarksSortingModeManual = 0,
  VivaldiBookmarksSortingModeByTitle = 1,
  VivaldiBookmarksSortingModeByAddress = 2,
  VivaldiBookmarksSortingModeByNickname = 3,
  VivaldiBookmarksSortingModeByDescription = 4,
  VivaldiBookmarksSortingModeByDate = 5,
  VivaldiBookmarksSortingModeByKind = 6,
};

typedef NS_ENUM(NSUInteger, VivaldiBookmarksSortingOrder) {
  VivaldiBookmarksSortingOrderAscending = 0,
  VivaldiBookmarksSortingOrderDescending = 1
};

#endif  // IOS_UI_BOOKMARKS_EDITOR_VIVALDI_BOOKMARKS_SORTING_MODE_H_
