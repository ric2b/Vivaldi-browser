// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SITE_TRACKER_PREFS_VIVALDI_SITE_TRACKER_PREFS_VIEW_DELEGATE_H_
#define IOS_UI_SITE_TRACKER_PREFS_VIVALDI_SITE_TRACKER_PREFS_VIEW_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/ui/ad_tracker_blocker/vivaldi_atb_setting_type.h"

@protocol VivaldiSiteTrackerPrefsViewDelegate
- (void)setExceptionForDomain:(NSString*)domain
                 blockingType:(ATBSettingType)blockingType;
@end

#endif  // IOS_UI_SITE_TRACKER_PREFS_VIVALDI_SITE_TRACKER_PREFS_VIEW_DELEGATE_H_
