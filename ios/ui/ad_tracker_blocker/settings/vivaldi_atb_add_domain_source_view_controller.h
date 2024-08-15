// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_AD_TRACKER_BLOCKER_SETTINGS_VIVALDI_ATB_ADD_DOMAIN_SOURCE_VIEW_CONTROLLER_H_
#define IOS_UI_AD_TRACKER_BLOCKER_SETTINGS_VIVALDI_ATB_ADD_DOMAIN_SOURCE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_domain_source_mode.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_setting_type.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_source_type.h"

/// VivaldiATBAddDomainSourceViewController is used to
/// add/edit domain and source for exceptions and ad blocker rule
/// respectively.
@interface VivaldiATBAddDomainSourceViewController :
  LegacyChromeTableViewController <UIAdaptivePresentationControllerDelegate>

// INITIALIZER
- (instancetype)initWithBrowser:(Browser*)browser
                          title:(NSString*)title
                         source:(ATBSourceType)source
                    editingMode:(ATBDomainSourceEditingMode)editingMode
                  editingDomain:(NSString*)editingDomain
            siteSpecificSetting:(ATBSettingType)siteSpecificSetting;

@end

#endif  // IOS_UI_AD_TRACKER_BLOCKER_SETTINGS_VIVALDI_ATB_ADD_DOMAIN_SOURCE_VIEW_CONTROLLER_H_
