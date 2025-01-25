// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_NSD_VIVALDI_NSD_COORDINATOR_H_
#define IOS_UI_NTP_NSD_VIVALDI_NSD_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/ui/ntp/vivaldi_speed_dial_item.h"

class Browser;

@protocol VivaldiNSDCoordinatorDelegate <NSObject>
- (void)newSpeedDialCoordinatorDidDismiss;
@end

@interface VivaldiNSDCoordinator : ChromeCoordinator

// Designated initializers.
- (instancetype)initWithBaseNavigationController:
        (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                          parent:(VivaldiSpeedDialItem*)parent;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    parent:(VivaldiSpeedDialItem*)parent
NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Coordinator delegate to observe coorinator dismiss event.
@property (nonatomic, weak) id<VivaldiNSDCoordinatorDelegate> delegate;

// Will provide the necessary UI to create a folder. `YES` by default.
// Should be set before calling `start`.
@property(nonatomic) BOOL allowsNewFolders;

@end

#endif  // IOS_UI_NTP_NSD_VIVALDI_NSD_COORDINATOR_H_
