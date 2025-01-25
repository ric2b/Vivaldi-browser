// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/appearance/vivaldi_appearance_settings_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/ui/settings/appearance/vivaldi_appearance_settings_mediator.h"
#import "ios/ui/settings/appearance/vivaldi_appearance_settings_prefs.h"
#import "ios/ui/settings/appearance/vivaldi_appearance_settings_swift.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

@interface VivaldiAppearanceSettingsCoordinator () {
  // The browser where the settings are being displayed.
  Browser* _browser;
}
// The parent view controller where this view is presented. This can be
// different than the baseViewController. This aligns with the active window
// from where currently active root modal view controller is presented.
@property (nonatomic, strong) UIViewController* presentingViewController;
// View provider class for the appearance settings.
@property(nonatomic, strong) VivaldiAppearanceSettingsViewProvider* viewProvider;
// Mediator class for the appearance settings
@property(nonatomic, strong) VivaldiAppearanceSettingsMediator* mediator;

@end

@implementation VivaldiAppearanceSettingsCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                        presentingViewController:
              (UIViewController*)presentingViewController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];

  if (self) {
    _browser = browser;
    _presentingViewController = presentingViewController;
    [VivaldiAppearanceSettingPrefs
        setPrefService:_browser->GetProfile()->GetPrefs()];
    _baseNavigationController = navigationController;
  }

  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.viewProvider = [[VivaldiAppearanceSettingsViewProvider alloc] init];
  UIViewController* controller =
      [VivaldiAppearanceSettingsViewProvider
          makeViewControllerWithPresentingViewControllerTrait:
              self.presentingViewController.traitCollection];
  controller.title =
      l10n_util::GetNSString(IDS_VIVALDI_IOS_APPEARANCE_SETTING_TITLE);
  controller.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;

  self.mediator = [[VivaldiAppearanceSettingsMediator alloc]
      initWithOriginalPrefService:self.browser->GetProfile()
                                      ->GetOriginalProfile()
                                      ->GetPrefs()];
  self.mediator.consumer = self.viewProvider;
  self.viewProvider.settingsStateConsumer = self.mediator;

  [self.baseNavigationController pushViewController:controller
                                           animated:YES];
}

- (void)stop {
  [super stop];
  self.viewProvider = nil;
  self.presentingViewController = nil;
}

@end
