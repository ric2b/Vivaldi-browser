// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_SUMMERY_DETAILS_VIEW_CONTROLLER_H_
#define IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_SUMMERY_DETAILS_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@interface VivaldiATBSummeryDetailsViewController :
  ChromeTableViewController <UIAdaptivePresentationControllerDelegate>

// INITIALIZER
- (instancetype)initWithTitle:(NSString*)title;

@end

#endif  // IOS_VIVALDI_BROWSER_UI_VIVALDI_ATB_SUMMERY_DETAILS_VIEW_CONTROLLER_H_
