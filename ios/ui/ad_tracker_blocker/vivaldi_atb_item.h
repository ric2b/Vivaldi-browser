// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_ITEM_H_
#define IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_ITEM_H_

#import <Foundation/Foundation.h>

#import "ios/ui/ad_tracker_blocker/vivaldi_atb_setting_type.h"

// The item model for the tracker blocker setting types.
@interface VivaldiATBItem : NSObject

@property(nonatomic, strong) NSString* title;
@property(nonatomic, strong) NSString* subtitle;
@property(nonatomic, assign) ATBSettingType type;

// INITIALIZER
- (instancetype) initWithTitle:(NSString*)title
                      subtitle:(NSString*)subtitle
                          type:(ATBSettingType)type;

@end

#endif  // IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_ITEM_H_
