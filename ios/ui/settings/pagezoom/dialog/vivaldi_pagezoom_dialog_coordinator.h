// Copyright 2024-25 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_PAGEZOOM_DIALOG_VIVALDI_PAGEZOOM_DIALOG_COORDINATOR_H_
#define IOS_UI_SETTINGS_PAGEZOOM_DIALOG_VIVALDI_PAGEZOOM_DIALOG_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/ui/settings/pagezoom/dialog/vivaldi_pagezoom_view_controller.h"

class Browser;
@class VivaldiPageZoomViewController;
@protocol ToolbarAccessoryCoordinatorDelegate;

// This class is the coordinator for the pagezoom setting.
@interface VivaldiPageZoomDialogCoordinator :
    ChromeCoordinator <VivaldiPageZoomSettingsDelegate>
// Delegate to inform when this coordinator's UI is dismissed.
@property(nonatomic, weak) id<ToolbarAccessoryCoordinatorDelegate> delegate;
@end

#endif  // IOS_UI_SETTINGS_PAGEZOOM_DIALOG_VIVALDI_PAGEZOOM_DIALOG_COORDINATOR_H_
