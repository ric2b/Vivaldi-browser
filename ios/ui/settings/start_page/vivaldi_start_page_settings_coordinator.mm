// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/start_page/vivaldi_start_page_settings_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_settings_mediator.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_settings_view_controller.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

@interface VivaldiStartPageSettingsCoordinator () {
  // The browser where the settings are being displayed.
  Browser* _browser;
}
// View controller for the start page setting.
@property(nonatomic, strong)
    VivaldiStartPageSettingsViewController* viewController;
// Start page preference mediator.
@property(nonatomic, strong) VivaldiStartPageSettingsMediator* mediator;

@end

@implementation VivaldiStartPageSettingsCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];

  if (self) {
    _browser = browser;
    _baseNavigationController = navigationController;
  }

  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.viewController =
      [[VivaldiStartPageSettingsViewController alloc] initWithBrowser:_browser];
  self.viewController.title =
      l10n_util::GetNSString(IDS_IOS_PREFS_VIVALDI_START_PAGE);
  self.viewController.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;

  self.mediator = [[VivaldiStartPageSettingsMediator alloc]
      initWithOriginalPrefService:self.browser->GetProfile()
                                      ->GetOriginalProfile()
                                      ->GetPrefs()];
  self.mediator.consumer = self.viewController;

  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
  self.viewController.consumer = self.mediator;
}

- (void)stop {
  [super stop];
  self.viewController = nil;
  [self.mediator disconnect];
  self.mediator = nil;
}

@end
