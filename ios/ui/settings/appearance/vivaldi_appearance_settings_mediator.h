// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_APPEARANCE_VIVALDI_APPEARANCE_SETTINGS_MEDIATOR_H_
#define IOS_UI_SETTINGS_APPEARANCE_VIVALDI_APPEARANCE_SETTINGS_MEDIATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

#import "ios/ui/settings/appearance/vivaldi_appearance_settings_consumer.h"

class PrefService;

@protocol VivaldiAppearanceSettingsConsumer;

// The Mediator for address bar preference setting.
@interface VivaldiAppearanceSettingsMediator
    : NSObject <VivaldiAppearanceSettingsConsumer>

- (instancetype)initWithOriginalPrefService:(PrefService*)originalPrefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The consumer of the appearance settings mediator.
@property(nonatomic, weak) id<VivaldiAppearanceSettingsConsumer> consumer;

// Disconnects the tab settings observation.
- (void)disconnect;

@end

#endif  // IOS_UI_SETTINGS_APPEARANCE_VIVALDI_APPEARANCE_SETTINGS_MEDIATOR_H_
