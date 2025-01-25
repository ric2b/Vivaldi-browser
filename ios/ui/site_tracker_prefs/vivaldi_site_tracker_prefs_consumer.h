// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SITE_TRACKER_PREFS_VIVALDI_SITE_TRACKER_PREFS_CONSUMER_H_
#define IOS_UI_SITE_TRACKER_PREFS_VIVALDI_SITE_TRACKER_PREFS_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/page_info/page_info_site_security_description.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_setting_type.h"

// A protocol implemented by consumers to handle changes in Site and Tracker
// Prefs page.
@protocol VivaldiSiteTrackerPrefsConsumer
- (void)setActiveWebStateDomain:(NSString*)domain;
- (void)setActiveWebStateFavicon:(UIImage*)favicon;
- (void)setBlockingLevelForDomain:(ATBSettingType)setting;
- (void)setGlobalBlockingLevel:(ATBSettingType)setting;
- (void)setRulesGroupApplying:(BOOL)applying;
- (void)setPageInfoSecurityDescription:
      (PageInfoSiteSecurityDescription*)description;
@end

#endif  // IOS_UI_SITE_TRACKER_PREFS_VIVALDI_SITE_TRACKER_PREFS_CONSUMER_H_
