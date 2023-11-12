// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/primary_toolbar_coordinator.h"

#import <CoreLocation/CoreLocation.h>

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view_controller.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "ios/ui/ad_tracker_blocker/manager/vivaldi_atb_manager.h"

using vivaldi::IsVivaldiRunning;
// End Vivaldi

@interface PrimaryToolbarCoordinator ()

// Vivaldi
<VivaldiATBConsumer> {}
// End Vivaldi

// Whether the coordinator is started.
@property(nonatomic, assign, getter=isStarted) BOOL started;
// Redefined as PrimaryToolbarViewController.
@property(nonatomic, strong) PrimaryToolbarViewController* viewController;

// Vivaldi
@property(nonatomic, strong) VivaldiATBManager* adblockManager;
// End Vivaldi

@end

@implementation PrimaryToolbarCoordinator

// Vivaldi
@synthesize adblockManager = _adblockManager;
// End Vivaldi

@dynamic viewController;

#pragma mark - ChromeCoordinator

- (void)start {
  DCHECK(self.browser);
  if (self.started)
    return;

  self.viewController = [[PrimaryToolbarViewController alloc] init];

  if (IsVivaldiRunning()) {
    self.viewController.shouldHideOmniboxOnNTP = NO;
  } else {
  self.viewController.shouldHideOmniboxOnNTP =
      !self.browser->GetBrowserState()->IsOffTheRecord();
  } // End Vivaldi

  self.viewController.omniboxCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), OmniboxCommands);
  self.viewController.popupMenuCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), PopupMenuCommands);
  CHECK(self.viewControllerDelegate);
  self.viewController.delegate = self.viewControllerDelegate;
  self.viewController.layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);

  // Button factory requires that the omnibox commands are set up, which is
  // done by the location bar.
  self.viewController.buttonFactory =
      [self buttonFactoryWithType:ToolbarType::kPrimary];

  // Vivaldi
  [self initialiseAdblockManager];
  // End Vivaldi

  [super start];
  self.started = YES;
}

- (void)stop {
  if (!self.started)
    return;
  [super stop];
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];

  // Vivaldi
  if (!self.adblockManager)
    return;
  self.adblockManager.consumer = nil;
  [self.adblockManager disconnect];
  // End Vivaldi

  self.started = NO;
}

#pragma mark - Public

- (id<SharingPositioner>)SharingPositioner {
  return self.viewController;
}

- (id<ToolbarAnimatee>)toolbarAnimatee {
  CHECK(self.viewController);
  return self.viewController;
}

#pragma mark - ToolbarCommands

- (void)triggerToolbarSlideInAnimation {
  [self.viewController triggerToolbarSlideInAnimationFromBelow:NO];
}

#pragma mark - VIVALDI

- (void)initialiseAdblockManager {
  if (!self.browser)
    return;
  self.adblockManager =
      [[VivaldiATBManager alloc] initWithBrowser:self.browser];
  self.adblockManager.consumer = self;
  [self updateVivaldiShieldState];
}

- (void)updateVivaldiShieldState {
  [self.viewController
      updateVivaldiShieldState:[self.adblockManager globalBlockingSetting]];
}

#pragma mark: - VivaldiATBConsumer
- (void)didRefreshSettingOptions:(NSArray*)options {
  if (options.count > 0)
    [self updateVivaldiShieldState];
}

- (void)ruleServiceStateDidLoad {
  [self updateVivaldiShieldState];
}

@end
