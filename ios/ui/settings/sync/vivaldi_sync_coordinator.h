// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_COORDINATOR_H_
#define IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace syncer {
  class SyncSetupInProgressHandle;
}

@class VivaldiSyncCoordinator;

// Delegate for VivaldiSyncCoordinator.
@protocol VivaldiSyncCoordinatorDelegate <NSObject>

// Called when the view controller is removed from its parent.
- (void)vivaldiSyncCoordinatorWasRemoved:
    (VivaldiSyncCoordinator*)coordinator;

@end

@interface VivaldiSyncCoordinator : ChromeCoordinator

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                               showCreateAccount:(BOOL)showCreateAccount;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@property(nonatomic, weak) id<VivaldiSyncCoordinatorDelegate> delegate;

// Determine if we need to setup a cancel button on the navigation's left bar
// button.
@property(nonatomic) BOOL showCancelButton;

@end

#endif  // IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_COORDINATOR_H_
