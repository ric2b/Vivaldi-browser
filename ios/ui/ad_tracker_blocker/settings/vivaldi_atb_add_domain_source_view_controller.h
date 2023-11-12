// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_AD_TRACKER_BLOCKER_SETTINGS_VIVALDI_ATB_ADD_DOMAIN_SOURCE_VIEW_CONTROLLER_H_
#define IOS_UI_AD_TRACKER_BLOCKER_SETTINGS_VIVALDI_ATB_ADD_DOMAIN_SOURCE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_domain_source_mode.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_source_type.h"

@interface VivaldiATBAddDomainSourceViewController :
  ChromeTableViewController <UIAdaptivePresentationControllerDelegate>

// INITIALIZER
- (instancetype)initWithTitle:(NSString*)title
                       source:(ATBSourceType)source
                  editingMode:(ATBDomainSourceEditingMode)mode;

@end

#endif  // IOS_UI_AD_TRACKER_BLOCKER_SETTINGS_VIVALDI_ATB_ADD_DOMAIN_SOURCE_VIEW_CONTROLLER_H_
