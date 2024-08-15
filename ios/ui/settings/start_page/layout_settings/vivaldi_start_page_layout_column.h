// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_START_PAGE_LAYOUT_SETTINGS_VIVALDI_START_PAGE_LAYOUT_COLUMN_H_
#define IOS_UI_SETTINGS_START_PAGE_LAYOUT_SETTINGS_VIVALDI_START_PAGE_LAYOUT_COLUMN_H_

// Enum for the vivaldi start page layout column.
typedef NS_ENUM(NSUInteger, VivaldiStartPageLayoutColumn) {
  VivaldiStartPageLayoutColumnOne = 1,
  VivaldiStartPageLayoutColumnTwo = 2,
  VivaldiStartPageLayoutColumnThree = 3,
  VivaldiStartPageLayoutColumnFour = 4,
  VivaldiStartPageLayoutColumnFive = 5,
  VivaldiStartPageLayoutColumnSix = 6,
  VivaldiStartPageLayoutColumnSeven = 7,
  VivaldiStartPageLayoutColumnEight = 8,
  VivaldiStartPageLayoutColumnTen = 10,
  VivaldiStartPageLayoutColumnTwelve = 12,
  // Give an unrealistic number which can never be satisfied.
  // Layout will be created based on available space.
  VivaldiStartPageLayoutColumnUnlimited = 5000,
};

#endif  // IOS_UI_SETTINGS_START_PAGE_LAYOUT_SETTINGS_VIVALDI_START_PAGE_LAYOUT_COLUMN_H_
