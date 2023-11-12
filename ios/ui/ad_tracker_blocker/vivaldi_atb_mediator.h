// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_MEDIATOR_H_
#define IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_MEDIATOR_H_

#import "UIKit/UIKit.h"

#import "ios/ui/ad_tracker_blocker/vivaldi_atb_consumer.h"

/// VivaldiATBMediator manages model interactions for the ad and tracker blocker
/// summer and settings pages.
@interface VivaldiATBMediator : NSObject

@property(nonatomic, weak) id<VivaldiATBConsumer> consumer;

- (instancetype)init;

/// Starts this mediator. Populates the initial values to the UI.
- (void)startMediating;

/// Stops mediating and disconnects.
- (void)disconnect;

/// Rebuilds the ad and tracker blocker setting options.
- (void)computeATBSettingOptions;

@end

#endif  // IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_MEDIATOR_H_
