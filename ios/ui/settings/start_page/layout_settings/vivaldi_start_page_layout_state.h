// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_START_PAGE_LAYOUT_SETTINGS_VIVALDI_START_PAGE_LAYOUT_STATE_H_
#define IOS_UI_SETTINGS_START_PAGE_LAYOUT_SETTINGS_VIVALDI_START_PAGE_LAYOUT_STATE_H_

/// Enum for the layout state.
/// VivaldiStartPageLayoutStatePreviewModalSmall is the state
/// when layout preview is visible within main settings.
/// VivaldiStartPageLayoutStatePreviewModalFull is the state
/// when layout preview is visible within wallpaper select screen.
/// VivaldiStartPageLayoutStateNormal is the state that is used to
/// render the actual speed dial on the start page
/// VivaldiStartPageLayoutStateNormal is the state that is used to
/// render the actual speed dial on bookmarks editor top sites
typedef NS_ENUM(NSUInteger, VivaldiStartPageLayoutState) {
  VivaldiStartPageLayoutStatePreviewModalSmall = 1,
  VivaldiStartPageLayoutStatePreviewModalFull = 2,
  VivaldiStartPageLayoutStateNormal = 3,
  VivaldiStartPageLayoutStateNormalTopSites = 4,
};

#endif  // IOS_UI_SETTINGS_START_PAGE_LAYOUT_SETTINGS_VIVALDI_START_PAGE_LAYOUT_STATE_H_
