// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_ADDRESSBAR_VIVALDI_ADDRESSBAR_SETTINGS_COORDINATOR_H_
#define IOS_UI_SETTINGS_ADDRESSBAR_VIVALDI_ADDRESSBAR_SETTINGS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class Browser;

// This class is the coordinator for the addressbar settings.
@interface VivaldiAddressBarSettingsCoordinator : ChromeCoordinator

// Designated initializer.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_UI_SETTINGS_ADDRESSBAR_VIVALDI_ADDRESSBAR_SETTINGS_COORDINATOR_H_
