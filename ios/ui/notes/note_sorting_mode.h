// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NOTES_NOTES_SORTING_MODE_H_
#define IOS_UI_NOTES_NOTES_SORTING_MODE_H_

// Page enumerates the modes of the speed dial sorting.
typedef NS_ENUM(NSUInteger, NotesSortingMode) {
  NotesSortingModeManual = 0,
  NotesSortingModeTitle = 1,
  NotesSortingModeDateCreated = 2,
  NotesSortingModeDateEdited = 3,
  NotesSortingModeByKind = 4,
};

typedef NS_ENUM(NSUInteger, NotesSortingOrder) {
  NotesSortingOrderAscending = 0,
  NotesSortingOrderDescending = 1
};

#endif  // IOS_UI_NOTES_NOTES_SORTING_MODE_H_
