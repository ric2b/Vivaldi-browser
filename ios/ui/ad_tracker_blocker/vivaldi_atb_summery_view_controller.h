// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_SUMMERY_VIEW_CONTROLLER_H_
#define IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_SUMMERY_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

/// The controller that contains the summery for the tracker blocked alongside
/// the settings for the visited site.
@interface VivaldiATBSummeryViewController :
  ChromeTableViewController <UIAdaptivePresentationControllerDelegate>

// INITIALIZER
- (instancetype)initWithBrowser:(Browser*)browser
                           host:(NSString*)host;

@end

#endif  // IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_SUMMERY_VIEW_CONTROLLER_H_
