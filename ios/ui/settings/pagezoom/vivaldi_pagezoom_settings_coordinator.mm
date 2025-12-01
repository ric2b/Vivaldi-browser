// Copyright 2024-25 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/pagezoom/vivaldi_pagezoom_settings_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/ui/settings/pagezoom/vivaldi_pagezoom_settings_mediator.h"
#import "ios/ui/settings/pagezoom/vivaldi_pagezoom_settings_swift.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

@interface VivaldiPageZoomSettingsCoordinator ()
@property(nonatomic, strong) VivaldiPageZoomSettingsViewProvider* viewProvider;
// View controller for the page zoom setting.
@property(nonatomic, strong) UIViewController* viewController;
// Page zoom preference mediator.
@property(nonatomic, strong) VivaldiPageZoomSettingsMediator* mediator;
@end

@implementation VivaldiPageZoomSettingsCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
    (UINavigationController*)navigationController browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  self.viewProvider = [[VivaldiPageZoomSettingsViewProvider alloc] init];
  self.viewController =
    [VivaldiPageZoomSettingsViewProvider makeViewController];
  self.viewController.title =
    l10n_util::GetNSString(IDS_IOS_PAGEZOOM_SETTING_TITLE);
  self.viewController.navigationItem.largeTitleDisplayMode =
    UINavigationItemLargeTitleDisplayModeNever;

  self.mediator = [[VivaldiPageZoomSettingsMediator alloc]
                    initWithOriginalPrefService:self.browser->GetProfile()
                    ->GetPrefs()];
  self.mediator.browser = self.browser;
  self.mediator.consumer = self.viewProvider;
  self.viewProvider.settingsStateConsumer = self.mediator;
  [self observeResetDomainSettingsButtonTapEvent];

  // Add Done button
  UIBarButtonItem* doneItem =
  [[UIBarButtonItem alloc]
    initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                         target:self
                         action:@selector(handleDoneButtonTap)];
  self.viewController.navigationItem.rightBarButtonItem = doneItem;

  if (self.isFromDialog) {
    // Create a new navigation controller because the page zoom
    // dialoge is presenting from a different view hireacrchy
    UINavigationController* navigationController =
      [[UINavigationController alloc]
        initWithRootViewController:self.viewController];
    navigationController.modalPresentationStyle = UIModalPresentationPageSheet;

    // Configure sheet presentation
    UISheetPresentationController* sheetPc =
      navigationController.sheetPresentationController;
    if (sheetPc) {
      // When iPad full screen or 2/3 SplitView support only large detent
      // because medium detent cuts the contents makes
      // the dialog small and off centered.
      if (IsSplitToolbarMode(self.baseViewController)) {
        sheetPc.detents = @[UISheetPresentationControllerDetent.mediumDetent,
                            UISheetPresentationControllerDetent.largeDetent];
      } else {
        sheetPc.detents = @[UISheetPresentationControllerDetent.largeDetent];
      }
      sheetPc.prefersScrollingExpandsWhenScrolledToEdge = NO;
      sheetPc.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
    }
    [self.baseViewController presentViewController:navigationController
                                          animated:YES
                                        completion:nil];
  } else {
    [self.baseNavigationController pushViewController:self.viewController
                                             animated:YES];
  };
}

- (void)stop {
  [super stop];
  self.viewController = nil;
  [self.mediator disconnect];
  self.mediator = nil;
  self.viewProvider = nil;
}

#pragma mark - Private

- (void)handleDoneButtonTap {
  if (self.isFromDialog) {
    [self.baseViewController dismissViewControllerAnimated:YES
                                                completion:nil];
  } else {
    [self.baseNavigationController dismissViewControllerAnimated:YES
                                                      completion:nil];
  }
  [self stop];
}

- (void)observeResetDomainSettingsButtonTapEvent {
  __weak __typeof(self) weakSelf = self;
  [self.viewProvider observeResetDomainSettingsButtonTapEvent:^{
    [weakSelf handleResetDomainSettingsButtonTap];
  }];
}

- (void)handleResetDomainSettingsButtonTap {
  [self.mediator resetUsersDomainZoomPref];
}

@end
