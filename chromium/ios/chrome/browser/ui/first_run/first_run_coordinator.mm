// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/feature_list.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/time/time.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/first_run/first_run_metrics.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_coordinator.h"
#import "ios/chrome/browser/ui/first_run/default_browser/default_browser_screen_coordinator.h"
#import "ios/chrome/browser/ui/first_run/first_run_screen_delegate.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/first_run/signin/signin_screen_coordinator.h"
#import "ios/chrome/browser/ui/first_run/tangible_sync/tangible_sync_screen_coordinator.h"
#import "ios/chrome/browser/ui/screen/screen_provider.h"
#import "ios/chrome/browser/ui/screen/screen_type.h"
#import "ios/public/provider/chrome/browser/signin/choice_api.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/ui/ad_tracker_blocker/manager/vivaldi_atb_manager.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/modal_page/modal_page_commands.h"
#import "ios/ui/modal_page/modal_page_coordinator.h"
#import "ios/ui/onboarding/vivaldi_onboarding_swift.h"
#import "ios/ui/settings/tabs/vivaldi_tab_setting_prefs.h"
#import "ios/ui/settings/vivaldi_settings_constants.h"
// End Vivaldi

@interface FirstRunCoordinator () <FirstRunScreenDelegate,
                                   HistorySyncCoordinatorDelegate>

@property(nonatomic, strong) ScreenProvider* screenProvider;
@property(nonatomic, strong) ChromeCoordinator* childCoordinator;
@property(nonatomic, strong) UINavigationController* navigationController;

// YES if First Run was completed.
@property(nonatomic, assign) BOOL completed;

// Vivaldi
@property(strong,nonatomic)
    VivaldiOnboardingActionsBridge *onboardingActionsBridge;
@property(nonatomic, weak) id<ModalPageCommands> modalPageHandler;
@property(nonatomic, strong) ModalPageCoordinator* modalPageCoordinator;
@property(nonatomic, strong) UIViewController* onboardingVC;
// End Vivaldi

@end

@implementation FirstRunCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                            screenProvider:(ScreenProvider*)screenProvider {
  DCHECK(!browser->GetBrowserState()->IsOffTheRecord());
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _screenProvider = screenProvider;
    _navigationController =
        [[UINavigationController alloc] initWithNavigationBarClass:nil
                                                      toolbarClass:nil];
    _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;

    // Vivaldi
    [browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(ModalPageCommands)];
    id<ModalPageCommands> modalPageHandler = HandlerForProtocol(
        browser->GetCommandDispatcher(), ModalPageCommands);
    _modalPageHandler = modalPageHandler;
    // End Vivaldi

  }
  return self;
}

- (void)start {
  [self presentScreen:[self.screenProvider nextScreenType]];
  void (^completion)(void) = ^{
    base::UmaHistogramEnumeration("FirstRun.Stage", first_run::kStart);
  };

  if (vivaldi::IsVivaldiRunning()) {
    [self presentOnboarding];
  } else {
  [self.navigationController setNavigationBarHidden:YES animated:NO];
  [self.baseViewController presentViewController:self.navigationController
                                        animated:NO
                                      completion:completion];
  } // End Vivaldi

}

- (void)stop {

  if (vivaldi::IsVivaldiRunning()) {
    [self dismissOnboarding];
  } // End Vivaldi

  void (^completion)(void) = ^{
  };
  if (self.completed) {
    __weak __typeof(self) weakSelf = self;
    completion = ^{
      base::UmaHistogramEnumeration("FirstRun.Stage", first_run::kComplete);
      WriteFirstRunSentinel();
      [weakSelf.delegate didFinishPresentingScreens];
    };
  }

  [self.childCoordinator stop];
  self.childCoordinator = nil;

  [self.baseViewController dismissViewControllerAnimated:YES
                                              completion:completion];
  _navigationController = nil;
  [super stop];
}

#pragma mark - FirstRunScreenDelegate

- (void)screenWillFinishPresenting {
  [self.childCoordinator stop];
  self.childCoordinator = nil;

  // Vivaldi
  _onboardingVC = nil;
  _onboardingActionsBridge = nil;
  // End Vivaldi

  [self presentScreen:[self.screenProvider nextScreenType]];
}

- (void)skipAllScreens {
  [self.childCoordinator stop];
  self.childCoordinator = nil;
  [self willFinishPresentingScreens];
}

#pragma mark - Helper

// Presents the screen of certain `type`.
- (void)presentScreen:(ScreenType)type {
  // If no more screen need to be present, call delegate to stop presenting
  // screens.
  if (type == kStepsCompleted) {
    [self willFinishPresentingScreens];
    return;
  }
  self.childCoordinator = [self createChildCoordinatorWithScreenType:type];
  [self.childCoordinator start];
}

// Creates a screen coordinator according to `type`.
- (ChromeCoordinator*)createChildCoordinatorWithScreenType:(ScreenType)type {
  switch (type) {
    case kSignIn:
      return [[SigninScreenCoordinator alloc]
          initWithBaseNavigationController:self.navigationController
                                   browser:self.browser
                                  delegate:self
                               accessPoint:signin_metrics::AccessPoint::
                                               ACCESS_POINT_START_PAGE
                               promoAction:signin_metrics::PromoAction::
                                               PROMO_ACTION_NO_SIGNIN_PROMO];
    case kHistorySync:
      return [[HistorySyncCoordinator alloc]
          initWithBaseNavigationController:self.navigationController
                                   browser:self.browser
                                  delegate:self
                                  firstRun:YES
                             showUserEmail:NO
                                isOptional:YES
                               accessPoint:signin_metrics::AccessPoint::
                                               ACCESS_POINT_START_PAGE];
    case kTangibleSync:
      return [[TangibleSyncScreenCoordinator alloc]
          initWithBaseNavigationController:self.navigationController
                                   browser:self.browser
                                  firstRun:YES
                                  delegate:self];
    case kDefaultBrowserPromo:
      return [[DefaultBrowserScreenCoordinator alloc]
          initWithBaseNavigationController:self.navigationController
                                   browser:self.browser
                                  delegate:self];
    case kChoice:
      return ios::provider::
          CreateChoiceCoordinatorForFREWithNavigationController(
              self.navigationController, self.browser, self);
    case kStepsCompleted:
      NOTREACHED() << "Reaches kStepsCompleted unexpectedly.";
      break;
  }
  return nil;
}

- (void)willFinishPresentingScreens {
  self.completed = YES;
  [self.delegate willFinishPresentingScreens];
}

#pragma mark - HistorySyncCoordinatorDelegate

- (void)closeHistorySyncCoordinator:
            (HistorySyncCoordinator*)historySyncCoordinator
                     declinedByUser:(BOOL)declined {
  CHECK_EQ(self.childCoordinator, historySyncCoordinator);
  [self screenWillFinishPresenting];
}

#pragma mark: - VIVALDI
- (void)presentOnboarding {
  self.onboardingActionsBridge = [[VivaldiOnboardingActionsBridge alloc] init];
  UIViewController *onboardingVC =
      [self.onboardingActionsBridge makeViewController];
  _onboardingVC = onboardingVC;

  // Note: (prio@vivaldi.com) On iPads, modal presentation styles are treated
  // as adaptive interfaces, which means they can be presented as floating
  // cards that users can easily dismiss. And this dismisses onboarding views if
  // user goes to Settings page for default browser settings or moves the app to
  // background. This behavior is present in Chrome too.

  // To prevent our onboarding view from being dismissed in
  // such scenarios, we've chosen to add it as a child view controller to the
  // baseViewController. This ensures the onboarding view is treated
  // as part of the main view hierarchy rather than a dismissible modal. This
  // also fixes the issues with start page being visible momentariliy after
  // splash screen and before onboarding pages.
  [onboardingVC
      willMoveToParentViewController:self.baseViewController];
  [self.baseViewController addChildViewController:onboardingVC];
  [self.baseViewController.view addSubview:onboardingVC.view];
  [onboardingVC
      didMoveToParentViewController:self.baseViewController];
  [onboardingVC.view fillSuperview];

  [self.onboardingActionsBridge observeTOSURLTapEvent:^(NSURL *url,
                                                        NSString *title) {
    [self.modalPageHandler showModalPage:url
                                   title:title];
  }];

  [self.onboardingActionsBridge observePrivacyURLTapEvent:^(NSURL *url,
                                                            NSString *title) {
    [self.modalPageHandler showModalPage:url
                                   title:title];
  }];

  [self.onboardingActionsBridge
    observeAdblockerSettingChange:^(ATBSettingType setting) {
    // Create a weak reference and store the settings to pref.
    VivaldiATBManager* adblockManager =
        [[VivaldiATBManager alloc] initWithBrowser:self.browser];
    if (!adblockManager)
      return;
    [adblockManager setExceptionFromBlockingType:setting];
  }];

  [self.onboardingActionsBridge
    observeTabStyleChange:^(BOOL isTabsOn) {
    [VivaldiTabSettingPrefs
      setDesktopTabsMode:isTabsOn
          inPrefServices:self.browser->GetBrowserState()->GetPrefs()];
    [[NSNotificationCenter defaultCenter]
      postNotificationName:vTabSettingsDidChange
                    object:self];
  }];

  [self.onboardingActionsBridge observeOnboardingFinishedState:^{
    [self willFinishPresentingScreens];
  }];
}

- (void)dismissOnboarding {
  if (_onboardingVC) {
    [_onboardingVC willMoveToParentViewController:nil];
    [_onboardingVC.view removeFromSuperview];
    [_onboardingVC removeFromParentViewController];
    [_onboardingVC didMoveToParentViewController:nil];

    WriteFirstRunSentinel();
    [self.delegate didFinishPresentingScreens];
  }
}

#pragma mark - ModalPageCommands
- (void)showModalPage:(NSURL*)url
                title:(NSString*)title {
  if (!self.onboardingVC || !self.browser)
    return;

  self.modalPageCoordinator = [[ModalPageCoordinator alloc]
      initWithBaseViewController:self.onboardingVC
                         browser:self.browser
                         pageURL:url
                           title:title];
  [self.modalPageCoordinator start];
}

- (void)closeModalPage {
  [self.modalPageCoordinator stop];
  self.modalPageCoordinator = nil;
}

@end
