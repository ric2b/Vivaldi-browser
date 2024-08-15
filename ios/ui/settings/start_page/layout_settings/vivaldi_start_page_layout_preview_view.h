// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_START_PAGE_LAYOUT_SETTINGS_VIVALDI_START_PAGE_LAYOUT_PREVIEW_VIEW_H_
#define IOS_UI_SETTINGS_START_PAGE_LAYOUT_SETTINGS_VIVALDI_START_PAGE_LAYOUT_PREVIEW_VIEW_H_

#import "UIKit/UIKit.h"

#import "ios/ui/settings/start_page/layout_settings/vivaldi_start_page_layout_style.h"

// struct for layout preview
typedef struct {
  int numberOfItemsLarge;
  int numberOfItemsLargeLandscape;
  int numberOfItemsLargeiPad;
  int numberOfItemsMedium;
  int numberOfItemsMediumLandscape;
  int numberOfItemsMediumiPad;
  int numberOfItemsSmall;
  int numberOfItemsSmallLandscape;
  int numberOfItemsSmalliPad;
  int numberOfItemsList;
  int numberOfItemsListLandscape;
  int numberOfItemsListiPad;
} PreviewItemConfig;

@interface VivaldiStartPageLayoutPreviewView : UIView {
// Instance variable for storing ItemConfig
  PreviewItemConfig _itemConfig;
}

// Custom initializer that takes ItemConfig as a parameter
- (instancetype)initWithItemConfig:(PreviewItemConfig)itemConfig;

// SETTERS
- (void)reloadLayoutWithStyle:(VivaldiStartPageLayoutStyle)style;
@end

#endif  // IOS_UI_SETTINGS_START_PAGE_LAYOUT_SETTINGS_VIVALDI_START_PAGE_LAYOUT_PREVIEW_VIEW_H_
