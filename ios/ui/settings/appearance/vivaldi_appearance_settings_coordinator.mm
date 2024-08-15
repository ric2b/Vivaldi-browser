// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/appearance/vivaldi_appearance_settings_coordinator.h"

#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/ui/settings/appearance/vivaldi_appearance_settings_mediator.h"
#import "ios/ui/settings/appearance/vivaldi_appearance_settings_prefs.h"
#import "ios/ui/settings/appearance/vivaldi_appearance_settings_swift.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

@interface VivaldiAppearanceSettingsCoordinator () {
  // The browser where the settings are being displayed.
  Browser* _browser;
}
// View provider class for the appearance settings.
@property(nonatomic, strong) VivaldiAppearanceSettingsViewProvider* viewProvider;
//// View controller for the appearance setting.
//@property(nonatomic, strong) UIViewController* viewController;
// Mediator class for the appearance settings
@property(nonatomic, strong) VivaldiAppearanceSettingsMediator* mediator;

@end

@implementation VivaldiAppearanceSettingsCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];

  if (self) {
    _browser = browser;
    [VivaldiAppearanceSettingPrefs
        setPrefService:_browser->GetBrowserState()->GetPrefs()];
    _baseNavigationController = navigationController;
  }

  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.viewProvider = [[VivaldiAppearanceSettingsViewProvider alloc] init];
  UIViewController* controller =
      [VivaldiAppearanceSettingsViewProvider makeViewController];
  controller.title =
      l10n_util::GetNSString(IDS_VIVALDI_IOS_APPEARANCE_SETTING_TITLE);
  controller.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;

  self.mediator = [[VivaldiAppearanceSettingsMediator alloc]
      initWithOriginalPrefService:self.browser->GetBrowserState()
                                      ->GetOriginalChromeBrowserState()
                                      ->GetPrefs()];
  self.mediator.consumer = self.viewProvider;
  self.viewProvider.settingsStateConsumer = self.mediator;

  [self.baseNavigationController pushViewController:controller
                                           animated:YES];
}

- (void)stop {
  [super stop];
  self.viewProvider = nil;
}

@end
