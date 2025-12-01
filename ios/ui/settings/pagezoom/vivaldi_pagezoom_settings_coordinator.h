// Copyright 2024-25 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_PAGEZOOM_VIVALDI_PAGEZOOM_SETTINGS_COORDINATOR_H_
#define IOS_UI_SETTINGS_PAGEZOOM_VIVALDI_PAGEZOOM_SETTINGS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class Browser;

// This class is the coordinator for the pagezoom setting.
@interface VivaldiPageZoomSettingsCoordinator : ChromeCoordinator
// Flag to determine if the coordinator is initiated from a dialog.
@property(nonatomic, assign) BOOL isFromDialog;
// Designated initializer.
- (instancetype)initWithBaseNavigationController:
    (UINavigationController*)navigationController browser:(Browser*)browser;
@end

#endif  // IOS_UI_SETTINGS_PAGEZOOM_VIVALDI_PAGEZOOM_SETTINGS_COORDINATOR_H_
