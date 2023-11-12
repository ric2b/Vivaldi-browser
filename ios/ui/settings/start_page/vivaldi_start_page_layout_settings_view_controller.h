// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_START_PAGE_VIVALDI_START_PAGE_LAYOUT_SETTINGS_VIEW_CONTROLLER_H_
#define IOS_UI_SETTINGS_START_PAGE_VIVALDI_START_PAGE_LAYOUT_SETTINGS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@interface VivaldiStartPageLayoutSettingsViewController :
  ChromeTableViewController <UIAdaptivePresentationControllerDelegate>

// INITIALIZER
- (instancetype)initWithTitle:(NSString*)title
                      browser:(Browser*)browser;

@end

#endif  // IOS_UI_SETTINGS_START_PAGE_VIVALDI_START_PAGE_LAYOUT_SETTINGS_VIEW_CONTROLLER_H_
