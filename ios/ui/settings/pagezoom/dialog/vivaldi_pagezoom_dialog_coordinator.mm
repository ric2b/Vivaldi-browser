// Copyright 2024-25 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/pagezoom/dialog/vivaldi_pagezoom_dialog_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/text_zoom_commands.h"
#import "ios/ui/settings/pagezoom/dialog/uiwindow_pagezoom.h"
#import "ios/ui/settings/pagezoom/dialog/vivaldi_pagezoom_dialog_mediator.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

@interface VivaldiPageZoomDialogCoordinator ()
// View controller for the page zoom setting.
@property(nonatomic, strong, readwrite)
    VivaldiPageZoomViewController* viewController;
// Page zoom preference mediator.
@property(nonatomic, strong) VivaldiPageZoomDialogMediator* mediator;
// Allows simplified access to the TextZoomCommands handler.
@property(nonatomic) id<TextZoomCommands> textZoomCommandHandler;
@end

@implementation VivaldiPageZoomDialogCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  DCHECK(self.browser);

  self.textZoomCommandHandler =
    HandlerForProtocol(self.browser->GetCommandDispatcher(), TextZoomCommands);
  self.mediator = [[VivaldiPageZoomDialogMediator alloc]
                   initWithWebStateList:self.browser->GetWebStateList()
                   commandHandler:self.textZoomCommandHandler];
  self.mediator.prefService = self.browser->GetProfile()->GetPrefs();
  self.viewController = [[VivaldiPageZoomViewController alloc] init];
  self.viewController.commandHandler = self.textZoomCommandHandler;
  self.viewController.zoomHandler = self.mediator;
  self.mediator.consumer = self.viewController;

  UIWindow *keyWindow = nil;
  for (UIScene *scene in [UIApplication sharedApplication].connectedScenes) {
    if (scene.activationState == UISceneActivationStateForegroundActive &&
        [scene isKindOfClass:[UIWindowScene class]]) {
      keyWindow = ((UIWindowScene *)scene).windows.firstObject;
      break;
    }
  }

  if (keyWindow) {
    [keyWindow showPageZoomViewController:self.viewController];
  } else {
    NSLog(@"Warning: Could not find key window to present zoom controller");
    [self stop];
  }
}

- (void)stop {
  [self.mediator disconnect];
  self.mediator.consumer = nil;
  self.mediator = nil;
  self.viewController = nil;
}

@end
