// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_GENERAL_VIVALDI_GENERAL_SETTINGS_MEDIATOR_H_
#define IOS_UI_SETTINGS_GENERAL_VIVALDI_GENERAL_SETTINGS_MEDIATOR_H_

#import "ios/ui/settings/general/vivaldi_general_settings_consumer.h"

class PrefService;

@protocol VivaldiGeneralSettingsConsumer;

// The Mediator for address bar preference setting.
@interface VivaldiGeneralSettingsMediator
  : NSObject <VivaldiGeneralSettingsConsumer>

- (instancetype)initWithOriginalPrefService:(PrefService*)originalPrefService
  NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The consumer of the general settings mediator.
@property(nonatomic, weak) id<VivaldiGeneralSettingsConsumer> consumer;

// Disconnects the general settings observation.
- (void)disconnect;

@end

#endif  // IOS_UI_SETTINGS_GENERAL_VIVALDI_GENERAL_SETTINGS_MEDIATOR_H_
