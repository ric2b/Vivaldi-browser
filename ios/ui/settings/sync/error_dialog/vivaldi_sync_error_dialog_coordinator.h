// Copyright 2025 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SYNC_ERROR_DIALOG_VIVALDI_SYNC_ERROR_DIALOG_COORDINATOR_H_
#define IOS_UI_SETTINGS_SYNC_ERROR_DIALOG_VIVALDI_SYNC_ERROR_DIALOG_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/ui/settings/sync/error_dialog/vivaldi_sync_error_dialog_delegate.h"

class Browser;

// This class is the coordinator for the sync tracker error dialog
@interface VivaldiSyncErrorDialogCoordinator : ChromeCoordinator

// Initializer.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser;
// Delegate for VivaldiSyncErrorDialogDelegate related actions
@property(nonatomic, weak) id<VivaldiSyncErrorDialogDelegate> delegate;
@end

#endif  // IOS_UI_SETTINGS_SYNC_ERROR_DIALOG_VIVALDI_SYNC_ERROR_DIALOG_COORDINATOR_H_
