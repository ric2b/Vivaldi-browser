// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_START_PAGE_QUICK_SETTINGS_VIVALDI_START_PAGE_QUICK_SETTINGS_MEDIATOR_H_
#define IOS_UI_SETTINGS_START_PAGE_QUICK_SETTINGS_VIVALDI_START_PAGE_QUICK_SETTINGS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/ui/settings/start_page/quick_settings/vivaldi_start_page_quick_settings_consumer.h"

class PrefService;

@protocol VivaldiStartPageQuickSettingsConsumer;

// The Mediator for start page quick setting.
@interface VivaldiStartPageQuickSettingsMediator:
    NSObject<VivaldiStartPageQuickSettingsConsumer>

- (instancetype)initWithOriginalPrefService:(PrefService*)originalPrefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The consumer of the start page settings mediator.
@property(nonatomic, weak) id<VivaldiStartPageQuickSettingsConsumer> consumer;

// Disconnects the start page settings observation.
- (void)disconnect;

@end

#endif  // IOS_UI_SETTINGS_START_PAGE_QUICK_SETTINGS_VIVALDI_START_PAGE_QUICK_SETTINGS_MEDIATOR_H_
