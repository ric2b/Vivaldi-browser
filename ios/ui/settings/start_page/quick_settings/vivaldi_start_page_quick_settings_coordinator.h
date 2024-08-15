// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_START_PAGE_QUICK_SETTINGS_VIVALDI_START_PAGE_QUICK_SETTINGS_COORDINATOR_H_
#define IOS_UI_SETTINGS_START_PAGE_QUICK_SETTINGS_VIVALDI_START_PAGE_QUICK_SETTINGS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class Browser;

// This class is the coordinator for the start page quick settings that is
// presented when customize start page menu button is tapped on start page.
@interface VivaldiStartPageQuickSettingsCoordinator : ChromeCoordinator

// Designated initializer.
- (instancetype)initWithBaseNavigationController:
        (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                         NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_UI_SETTINGS_START_PAGE_QUICK_SETTINGS_VIVALDI_START_PAGE_QUICK_SETTINGS_COORDINATOR_H_
