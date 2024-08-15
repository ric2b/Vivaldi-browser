// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_START_PAGE_QUICK_SETTINGS_VIVALDI_START_PAGE_QUICK_SETTINGS_MEDIATOR_H_
#define IOS_UI_SETTINGS_START_PAGE_QUICK_SETTINGS_VIVALDI_START_PAGE_QUICK_SETTINGS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/ui/settings/start_page/vivaldi_start_page_settings_consumer.h"

class PrefService;

@protocol VivaldiStartPageSettingsConsumer;

// The Mediator for start page quick setting.
@interface VivaldiStartPageQuickSettingsMediator:
    NSObject<VivaldiStartPageSettingsConsumer>

- (instancetype)initWithOriginalPrefService:(PrefService*)originalPrefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The consumer of the start page settings mediator.
@property(nonatomic, weak) id<VivaldiStartPageSettingsConsumer> consumer;

// Disconnects the start page settings observation.
- (void)disconnect;

@end

#endif  // IOS_UI_SETTINGS_START_PAGE_QUICK_SETTINGS_VIVALDI_START_PAGE_QUICK_SETTINGS_MEDIATOR_H_
