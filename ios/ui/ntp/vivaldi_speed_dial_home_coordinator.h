// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_VIVALDI_SPEED_DIAL_HOME_COORDINATOR_H_
#define IOS_UI_NTP_VIVALDI_SPEED_DIAL_HOME_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/ui/ntp/vivaldi_speed_dial_base_controller.h"
#import "ios/ui/ntp/vivaldi_speed_dial_home_mediator.h"

class Browser;

// This class is the coordinator for the speed dial home/start page.
@interface VivaldiSpeedDialHomeCoordinator : ChromeCoordinator

// Designated initializer.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
NS_DESIGNATED_INITIALIZER;

// View controller for the speed dial home/start page.
@property(nonatomic, strong, readonly)
    VivaldiSpeedDialBaseController* viewController;

// Navigation controller for the base view controller.
@property(nonatomic, strong, readonly)
    UINavigationController* navigationController;

// Mediator class for the speed dial home/start page.
@property(nonatomic, strong, readonly) VivaldiSpeedDialHomeMediator* mediator;

@end

#endif  // IOS_UI_NTP_VIVALDI_SPEED_DIAL_HOME_COORDINATOR_H_
