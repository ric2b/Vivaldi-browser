// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SEARCH_ENGINE_VIVALDI_SEARCH_ENGINE_SETTINGS_MEDIATOR_H_
#define IOS_UI_SETTINGS_SEARCH_ENGINE_VIVALDI_SEARCH_ENGINE_SETTINGS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/ui/settings/search_engine/vivaldi_search_engine_settings_view_controller.h"

class ProfileIOS;
@protocol VivaldiSearchEngineSettingsConsumer;

// The mediator for search engine settings.
@interface VivaldiSearchEngineSettingsMediator: NSObject
                            <VivaldiSearchEngineSettingsViewControllerDelegate>

- (instancetype)initWithProfile:(ProfileIOS*)profile
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The consumer of the search engine settings mediator.
@property(nonatomic, weak) id<VivaldiSearchEngineSettingsConsumer> consumer;

- (void)disconnect;

@end

#endif  // IOS_UI_SETTINGS_SEARCH_ENGINE_VIVALDI_SEARCH_ENGINE_SETTINGS_MEDIATOR_H_
