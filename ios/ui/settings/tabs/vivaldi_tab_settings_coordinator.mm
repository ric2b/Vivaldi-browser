// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/tabs/vivaldi_tab_settings_coordinator.h"

#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/settings/tabs/inactive_tabs/inactive_tabs_settings_coordinator.h"
#import "ios/ui/settings/tabs/vivaldi_tab_settings_mediator.h"
#import "ios/ui/settings/tabs/vivaldi_tab_settings_swift.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

@interface VivaldiTabSettingsCoordinator () {
  // Coordinator for the inactive tabs settings.
  InactiveTabsSettingsCoordinator* _inactiveTabsSettingsCoordinator;
}
@property(nonatomic, strong) VivaldiTabsSettingsViewProvider* viewProvider;
// View controller for the Address bar setting.
@property(nonatomic, strong) UIViewController* viewController;
// Address bar preference mediator.
@property(nonatomic, strong) VivaldiTabSettingsMediator* mediator;

@end

@implementation VivaldiTabSettingsCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];

  if (self) {
    _baseNavigationController = navigationController;
  }

  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.viewProvider = [[VivaldiTabsSettingsViewProvider alloc] init];

  self.viewController =
      [VivaldiTabsSettingsViewProvider
          makeViewControllerWithOnInactiveTabsSelectionTap:^{
      [self showInactiveTabsSettings];
  }];

  self.viewController.title =
      l10n_util::GetNSString(IDS_IOS_PREFS_VIVALDI_TABS);
  self.viewController.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;

  self.mediator = [[VivaldiTabSettingsMediator alloc]
      initWithOriginalPrefService:self.browser->GetProfile()
                                 ->GetOriginalProfile()
                                 ->GetPrefs()
                 localPrefService:GetApplicationContext()->GetLocalState()];
  self.mediator.consumer = self.viewProvider;
  self.viewProvider.settingsStateConsumer = self.mediator;

  // Add Done button
  UIBarButtonItem* doneItem =
    [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                             target:self
                             action:@selector(handleDoneButtonTap)];
  self.viewController.navigationItem.rightBarButtonItem = doneItem;

  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

- (void)stop {
  [super stop];
  self.viewController = nil;
  [self.mediator disconnect];
  self.mediator = nil;
  self.viewProvider = nil;

  [_inactiveTabsSettingsCoordinator stop];
  _inactiveTabsSettingsCoordinator = nil;
}

#pragma mark - Private
- (void)showInactiveTabsSettings {
  if (!IsInactiveTabsAvailable())
    return;
  _inactiveTabsSettingsCoordinator = [[InactiveTabsSettingsCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser];
  [_inactiveTabsSettingsCoordinator start];
}

- (void)handleDoneButtonTap {
  [self stop];
  [self.baseNavigationController dismissViewControllerAnimated:YES
                                                    completion:nil];
}

@end
