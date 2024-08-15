// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/tabs/vivaldi_tab_settings_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/ui/settings/tabs/vivaldi_tab_settings_mediator.h"
#import "ios/ui/settings/tabs/vivaldi_tab_settings_swift.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

@interface VivaldiTabSettingsCoordinator ()
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
  self.viewController = [VivaldiTabsSettingsViewProvider makeViewController];
  self.viewController.title =
      l10n_util::GetNSString(IDS_IOS_PREFS_VIVALDI_TABS);
  self.viewController.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;

  self.mediator = [[VivaldiTabSettingsMediator alloc]
      initWithOriginalPrefService:self.browser->GetBrowserState()
                                      ->GetOriginalChromeBrowserState()
                                      ->GetPrefs()];
  self.mediator.consumer = self.viewProvider;
  self.viewProvider.settingsStateConsumer = self.mediator;

  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

- (void)stop {
  [super stop];
  self.viewController = nil;
  [self.mediator disconnect];
  self.mediator = nil;
  self.viewProvider = nil;
}

@end
