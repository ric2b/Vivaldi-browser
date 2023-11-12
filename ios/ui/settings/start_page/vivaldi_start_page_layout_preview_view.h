// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_START_PAGE_VIVALDI_START_PAGE_LAYOUT_PREVIEW_VIEW_H_
#define IOS_UI_SETTINGS_START_PAGE_VIVALDI_START_PAGE_LAYOUT_PREVIEW_VIEW_H_

#import "UIKit/UIKit.h"

#import "ios/ui/settings/start_page/vivaldi_start_page_layout_style.h"

@interface VivaldiStartPageLayoutPreviewView : UIView

// INITIALIZER
- (instancetype)init;

// SETTERS
- (void)reloadLayoutWithStyle:(VivaldiStartPageLayoutStyle)style;
@end

#endif  // IOS_UI_SETTINGS_START_PAGE_VIVALDI_START_PAGE_LAYOUT_PREVIEW_VIEW_H_
