// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/time/time.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/first_run/first_run_metrics.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/first_run/default_browser/default_browser_screen_coordinator.h"
#import "ios/chrome/browser/ui/first_run/first_run_screen_delegate.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/first_run/signin/signin_screen_coordinator.h"
#import "ios/chrome/browser/ui/first_run/tangible_sync/tangible_sync_screen_coordinator.h"
#import "ios/chrome/browser/ui/screen/screen_provider.h"
#import "ios/chrome/browser/ui/screen/screen_type.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/ui/ad_tracker_blocker/manager/vivaldi_atb_manager.h"
#import "ios/ui/modal_page/modal_page_commands.h"
#import "ios/ui/modal_page/modal_page_coordinator.h"
#import "ios/ui/onboarding/vivaldi_onboarding_swift.h"
#import "ios/ui/settings/tabs/vivaldi_tab_setting_prefs.h"
#import "ios/ui/settings/vivaldi_settings_constants.h"
// End Vivaldi

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FirstRunCoordinator () <FirstRunScreenDelegate>

@property(nonatomic, strong) ScreenProvider* screenProvider;
@property(nonatomic, strong) ChromeCoordinator* childCoordinator;
@property(nonatomic, strong) UINavigationController* navigationController;
@property(nonatomic, strong) NSDate* firstScreenStartTime;

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
  __weak FirstRunCoordinator* weakSelf = self;
  void (^completion)(void) = ^{
    base::UmaHistogramEnumeration("FirstRun.Stage", first_run::kStart);
    weakSelf.firstScreenStartTime = [NSDate now];
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
  void (^completion)(void) = ^{
  };
  if (self.completed) {
    completion = ^{
      base::UmaHistogramEnumeration("FirstRun.Stage", first_run::kComplete);
      WriteFirstRunSentinel();

      [self.delegate didFinishPresentingScreens];
    };
  }

  [self.childCoordinator stop];
  self.childCoordinator = nil;

  [self.baseViewController dismissViewControllerAnimated:YES
                                              completion:completion];
}

#pragma mark - FirstRunScreenDelegate

- (void)screenWillFinishPresenting {
  [self.childCoordinator stop];
  self.childCoordinator = nil;

  // Vivaldi
  _onboardingVC = nil;
  // End Vivaldi

  // Usually, finishing presenting the first FRE screen signifies that the user
  // has accepted Terms of Services. Therefore, we can use the time it takes the
  // first screen to be visible as the time it takes a user to accept Terms of
  // Services.
  if (self.firstScreenStartTime) {
    base::TimeDelta delta =
        base::Time::Now() - base::Time::FromNSDate(self.firstScreenStartTime);
    base::UmaHistogramTimes("FirstRun.TermsOfServicesPromoDisplayTime", delta);
    self.firstScreenStartTime = nil;
  }
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
                            showFREConsent:YES
                                  delegate:self];
    case kTangibleSync:
      return [[TangibleSyncScreenCoordinator alloc]
          initWithBaseNavigationController:self.navigationController
                                   browser:self.browser
                                  delegate:self];
    case kDefaultBrowserPromo:
      return [[DefaultBrowserScreenCoordinator alloc]
          initWithBaseNavigationController:self.navigationController
                                   browser:self.browser
                                  delegate:self];
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

#pragma mark: - VIVALDI
- (void)presentOnboarding {
  self.onboardingActionsBridge = [[VivaldiOnboardingActionsBridge alloc] init];
  UIViewController *onboardingVC =
      [self.onboardingActionsBridge makeViewController];
  _onboardingVC = onboardingVC;
  onboardingVC.modalPresentationStyle = UIModalPresentationFullScreen;
  onboardingVC.modalTransitionStyle = UIModalTransitionStyleCrossDissolve;
  [self.baseViewController presentViewController:onboardingVC
                                        animated:NO
                                      completion:nil];

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
  }];

  [self.onboardingActionsBridge observeOnboardingFinishedState:^{
    [self willFinishPresentingScreens];
  }];
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
