// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_AD_TRACKER_BLOCKER_SETTINGS_VIVALDI_ATB_SOURCE_SETTINGS_VIEW_CONTROLLER_H_
#define IOS_UI_AD_TRACKER_BLOCKER_SETTINGS_VIVALDI_ATB_SOURCE_SETTINGS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_source_type.h"

@interface VivaldiATBSourceSettingsViewController :
  LegacyChromeTableViewController <UIAdaptivePresentationControllerDelegate>

// INITIALIZER
- (instancetype)initWithBrowser:(Browser*)browser
                          title:(NSString*)title
                     sourceType:(ATBSourceType)sourceType;

@end

#endif  // IOS_UI_AD_TRACKER_BLOCKER_SETTINGS_VIVALDI_ATB_SOURCE_SETTINGS_VIEW_CONTROLLER_H_
