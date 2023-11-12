// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_ITEM_H_
#define IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_ITEM_H_

#import <Foundation/Foundation.h>

#import "ios/ui/ad_tracker_blocker/vivaldi_atb_setting_options.h"

// The item model for the tracker blocker setting option.
@interface VivaldiATBItem : NSObject

@property(nonatomic, strong) NSString* title;
@property(nonatomic, strong) NSString* subtitle;
@property(nonatomic, assign) ATBSettingOption option;

// INITIALIZER
- (instancetype) initWithTitle:(NSString*)title
                      subtitle:(NSString*)subtitle
                        option:(ATBSettingOption)option;

@end

#endif  // IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_ITEM_H_
