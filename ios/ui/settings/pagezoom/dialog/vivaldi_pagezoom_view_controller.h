// Copyright 2024-25 Vivaldi Technologies. All rights reserved.

#import <UIKit/UIKit.h>
#import "ios/ui/settings/pagezoom/dialog/vivaldi_pagezoom_dialog_consumer.h"

@protocol TextZoomCommands;
@class VivaldiPageZoomViewController;

@protocol PageZoomHandler <NSObject>
// Asks the handler to zoom in.
- (void)zoomIn;
// Asks the handler to zoom out.
- (void)zoomOut;
// Asks the handler to reset the zoom level to the default.
- (void)resetZoom;
// Asks the handler to refresh the state
- (void)refreshState;
// update global page zoom status
- (void)updateGlobalZoomSwitch:(UISwitch *)sender;
@end


@interface VivaldiPageZoomViewController :
    UIViewController <VivaldiPageZoomDialogConsumer>
@property(nonatomic, weak) id<TextZoomCommands> commandHandler;
@property(nonatomic, weak) id<PageZoomHandler> zoomHandler;
@end
