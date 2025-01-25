// Copyright 2024-25 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_PAGEZOOM_DIALOG_VIVALDI_PAGEZOOM_DIALOG_MEDIATOR_H_
#define IOS_UI_SETTINGS_PAGEZOOM_DIALOG_VIVALDI_PAGEZOOM_DIALOG_MEDIATOR_H_

#import "components/prefs/pref_service.h"
#import "ios/ui/settings/pagezoom/dialog/vivaldi_pagezoom_dialog_consumer.h"
#import "ios/ui/settings/pagezoom/dialog/vivaldi_pagezoom_view_controller.h"


@protocol TextZoomCommands;
@protocol VivaldiPageZoomDialogConsumer;
class WebStateList;

// The Mediator for address bar preference setting.
@interface VivaldiPageZoomDialogMediator : NSObject <PageZoomHandler>

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                      commandHandler:(id<TextZoomCommands>)commandHandler
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The consumer of the page zoom settings mediator.
@property(nonatomic, weak) id<VivaldiPageZoomDialogConsumer> consumer;

@property(nonatomic, assign) PrefService* prefService;

// Disconnects the page zoom settings observation.
- (void)disconnect;


@end

#endif  // IOS_UI_SETTINGS_PAGEZOOM_DIALOG_VIVALDI_PAGEZOOM_DIALOG_MEDIATOR_H_
