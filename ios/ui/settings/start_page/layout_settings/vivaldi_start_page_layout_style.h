// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_START_PAGE_LAYOUT_SETTINGS_VIVALDI_START_PAGE_LAYOUT_STYLE_H_
#define IOS_UI_SETTINGS_START_PAGE_LAYOUT_SETTINGS_VIVALDI_START_PAGE_LAYOUT_STYLE_H_

// Enum for the vivaldi start page layout styles.
typedef NS_ENUM(NSUInteger, VivaldiStartPageLayoutStyle) {
  VivaldiStartPageLayoutStyleLarge = 0,
  VivaldiStartPageLayoutStyleMedium = 1,
  VivaldiStartPageLayoutStyleSmall = 2,
  VivaldiStartPageLayoutStyleList = 3,
#if defined(VIVALDI_RELEASE_KIND) && VIVALDI_RELEASE_KIND != 2 // Not final
  VivaldiStartPageLayoutStyleIcon = 4,
#endif
};

#endif  // IOS_UI_SETTINGS_START_PAGE_LAYOUT_SETTINGS_VIVALDI_START_PAGE_LAYOUT_STYLE_H_
