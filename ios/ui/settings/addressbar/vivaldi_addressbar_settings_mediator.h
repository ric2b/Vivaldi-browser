// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_ADDRESSBAR_VIVALDI_ADDRESSBAR_SETTINGS_MEDIATOR_H_
#define IOS_UI_SETTINGS_ADDRESSBAR_VIVALDI_ADDRESSBAR_SETTINGS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/ui/settings/addressbar/vivaldi_addressbar_settings_consumer.h"

class PrefService;

@protocol VivaldiAddressBarSettingsConsumer;

// The mediator for addressbar preference setting.
@interface VivaldiAddressBarSettingsMediator
    : NSObject <VivaldiAddressBarSettingsConsumer>

- (instancetype)initWithOriginalPrefService:(PrefService*)originalPrefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The consumer of the addressbar settings mediator.
@property(nonatomic, weak) id<VivaldiAddressBarSettingsConsumer> consumer;

// Disconnects the addressbar settings observation.
- (void)disconnect;

@end

#endif  // IOS_UI_SETTINGS_ADDRESSBAR_VIVALDI_ADDRESSBAR_SETTINGS_MEDIATOR_H_
