// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_SUMMERY_HEADER_VIEW_H_
#define IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_SUMMERY_HEADER_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/ui/ad_tracker_blocker/vivaldi_atb_setting_type.h"

// A view to hold the summery of the tracker and blocker blocked for the visited
// site.
@interface VivaldiATBSummeryHeaderView : UIView

// INITIALIZERS
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// SETTERS
- (void)setStatusFromSetting:(ATBSettingType)settingType;
- (void)setRulesGroupApplying:(BOOL)isApplying;

@end

#endif  // IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_SUMMERY_HEADER_VIEW_H_
