// Copyright 2024-25 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_PAGEZOOM_VIVALDI_PAGEZOOM_SETTINGS_MEDIATOR_H_
#define IOS_UI_SETTINGS_PAGEZOOM_VIVALDI_PAGEZOOM_SETTINGS_MEDIATOR_H_

#import "ios/ui/settings/pagezoom/vivaldi_pagezoom_settings_consumer.h"

class PrefService;
class Browser;

@protocol VivaldiPageZoomSettingsConsumer;

// The Mediator for address bar preference setting.
@interface VivaldiPageZoomSettingsMediator
    : NSObject <VivaldiPageZoomSettingsConsumer>

- (instancetype)initWithOriginalPrefService:(PrefService*)originalPrefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The consumer of the page zoom settings mediator.
@property(nonatomic, weak) id<VivaldiPageZoomSettingsConsumer> consumer;
@property(nonatomic, assign) Browser* browser;
// Disconnects the page zoom settings observation.
- (void)disconnect;

@end

#endif  // IOS_UI_SETTINGS_PAGEZOOM_VIVALDI_PAGEZOOML_SETTINGS_MEDIATOR_H_
