// Copyright 2025 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/sync/error_dialog/vivaldi_sync_error_dialog_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/ui/settings/sync/error_dialog/vivaldi_sync_error_dialog_delegate.h"
#import "ios/ui/settings/sync/error_dialog/vivaldi_sync_error_dialog_prefs.h"
#import "ios/ui/settings/sync/error_dialog/vivaldi_sync_error_dialog_swift.h"
#import "ios/ui/settings/sync/vivaldi_sync_coordinator.h"
#import "ios/vivaldi_account/vivaldi_account_manager_factory.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi_account/vivaldi_account_manager.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

@interface VivaldiSyncErrorDialogCoordinator () <VivaldiSyncCoordinatorDelegate>
@property(nonatomic, strong) VivaldiSyncErrorDialogViewProvider* viewProvider;
// View controller for the sync error dialog
@property(nonatomic, strong) UIViewController* viewController;
// View controller for the sync error dialog
@property(nonatomic, strong) UIViewController* parentViewController;
// Vivaldi sync coordinator
@property(nonatomic, strong) VivaldiSyncCoordinator* vivaldiSyncCoordinator;
@property(nonatomic, strong) UINavigationController* navigationController;
@end

@implementation VivaldiSyncErrorDialogCoordinator


- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    self.parentViewController = viewController;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.viewProvider = [[VivaldiSyncErrorDialogViewProvider alloc] init];
  self.viewController =
    [VivaldiSyncErrorDialogViewProvider makeViewController];
       self.viewController.title =
         l10n_util::GetNSString(IDS_IOS_SYNC_ERROR_DIALOG_TITLE);
  self.viewController.navigationItem.largeTitleDisplayMode =
    UINavigationItemLargeTitleDisplayModeNever;
  UINavigationController* navigationController =
  [[UINavigationController alloc]
    initWithRootViewController:self.viewController];
  self.navigationController = navigationController;
  UISheetPresentationController* sheetPc =
    navigationController.sheetPresentationController;
  sheetPc.detents = @[UISheetPresentationControllerDetent.mediumDetent,
                      UISheetPresentationControllerDetent.largeDetent];
  sheetPc.prefersScrollingExpandsWhenScrolledToEdge = NO;
  sheetPc.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
  [self observeTapAndNavigationEvents];
  // Set the last sync error dialog shown date to today
  [VivaldiSyncErrorDialogPrefs
    setLastSyncErrorDialogShownDate:[NSDate date]
                    withPrefService:self.browser->GetProfile()->GetPrefs()];
}

- (void)stop {
  [super stop];
  self.viewController = nil;
  self.viewProvider = nil;
}

#pragma mark - Private

- (void)observeTapAndNavigationEvents {
   __weak __typeof(self) weakSelf = self;
  [self.viewProvider observeOpenSettingsButtonTapEvent:^{
    [weakSelf openSettingsButtonTap];
  }];
  [self.viewProvider observeNoThanksButtonTapEvent:^{
    [self noThanksButtonTap];
  }];
}

- (void) openSettingsButtonTap {
  // Call the delegate to show the sync error dialog
  __weak __typeof(self) weakSelf = self;
  [self.viewController dismissViewControllerAnimated:YES
                                          completion:^{
    [weakSelf.delegate showSyncErrorDialog];

  }];
}

- (void)noThanksButtonTap {
  PrefService* prefService = self.browser->GetProfile()->GetPrefs();
  [VivaldiSyncErrorDialogPrefs setShouldAskUserAgain:NO
                                     withPrefService:prefService];
  vivaldi::VivaldiAccountManager* account_manager =
    vivaldi::VivaldiAccountManagerFactory::GetForProfile(
                                self.browser->GetProfile());
  if (account_manager) {
    account_manager->Logout();
  }
  [self.viewController dismissViewControllerAnimated:YES
                                          completion:nil];
  [self stop];
}

#pragma mark - VivaldiSyncCoordinatorDelegate
- (void)vivaldiSyncCoordinatorWasRemoved:
    (VivaldiSyncCoordinator*)coordinator {
  DCHECK_EQ(self.vivaldiSyncCoordinator, coordinator);
  [self.vivaldiSyncCoordinator stop];
  self.vivaldiSyncCoordinator.delegate = nil;
  self.vivaldiSyncCoordinator = nil;
}

@end
