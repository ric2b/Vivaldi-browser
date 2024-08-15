// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_TABS_VIVALDI_TAB_SETTINGS_MEDIATOR_H_
#define IOS_UI_SETTINGS_TABS_VIVALDI_TAB_SETTINGS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/ui/settings/tabs/vivaldi_tab_settings_consumer.h"

class PrefService;

@protocol VivaldiTabsSettingsConsumer;

// The Mediator for address bar preference setting.
@interface VivaldiTabSettingsMediator
    : NSObject <VivaldiTabsSettingsConsumer>

- (instancetype)initWithOriginalPrefService:(PrefService*)originalPrefService
                           localPrefService:(PrefService*)localPrefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The consumer of the tab settings mediator.
@property(nonatomic, weak) id<VivaldiTabsSettingsConsumer> consumer;

// Disconnects the tab settings observation.
- (void)disconnect;

@end

#endif  // IOS_UI_SETTINGS_TABS_VIVALDI_TAB_SETTINGS_MEDIATOR_H_
