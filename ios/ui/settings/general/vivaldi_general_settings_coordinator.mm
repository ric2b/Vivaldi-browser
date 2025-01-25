// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/general/vivaldi_general_settings_coordinator.h"

#import "ios/chrome/browser/language/model/language_model_manager_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_table_view_controller.h"
#import "ios/ui/settings/general/vivaldi_general_settings_mediator.h"
#import "ios/ui/settings/general/vivaldi_general_settings_swift.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

@interface VivaldiGeneralSettingsCoordinator () {
  // Profile for the browser
  raw_ptr<ProfileIOS> _profile;  // weak
}
@property(nonatomic, strong) VivaldiGeneralSettingsViewProvider* viewProvider;
// View controller for the General setting.
@property(nonatomic, strong) UIViewController* viewController;
// General preference mediator.
@property(nonatomic, strong) VivaldiGeneralSettingsMediator* mediator;
@end

@implementation VivaldiGeneralSettingsCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
    (UINavigationController*)navigationController browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _profile = browser->GetProfile();
    _baseNavigationController = navigationController;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.viewProvider = [[VivaldiGeneralSettingsViewProvider alloc] init];
  self.viewController =
    [VivaldiGeneralSettingsViewProvider makeViewController];
  self.viewController.title =
      l10n_util::GetNSString(IDS_IOS_GENERAL_SETTING_TITLE);
  self.viewController.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;

  self.mediator = [[VivaldiGeneralSettingsMediator alloc]
                   initWithOriginalPrefService:self.browser->GetProfile()
                   ->GetOriginalProfile()
                   ->GetPrefs()];
  self.mediator.consumer = self.viewProvider;
  self.viewProvider.settingsStateConsumer = self.mediator;

  [self observeTapAndNavigationEvents];

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
}

#pragma mark - Private

- (void)observeTapAndNavigationEvents {
  __weak __typeof(self) weakSelf = self;

  [self.viewProvider observeAccpetLanguagesButtonTapEvent:^{
    [weakSelf handleAcceptLanguagesButtonTap];
  }];
}

#pragma mark - Actions

- (void)handleDoneButtonTap {
  [self stop];
  [self.baseNavigationController dismissViewControllerAnimated:YES
                                                    completion:nil];
}

- (void)handleAcceptLanguagesButtonTap {
  language::LanguageModelManager* languageModelManager =
      LanguageModelManagerFactory::GetForProfile(_profile);
  LanguageSettingsMediator* mediator = [[LanguageSettingsMediator alloc]
      initWithLanguageModelManager:languageModelManager
                       prefService:_profile->GetPrefs()];
  LanguageSettingsTableViewController* languageSettingsTableViewController =
      [[LanguageSettingsTableViewController alloc]
          initWithDataSource:mediator
              commandHandler:mediator];
  mediator.consumer = languageSettingsTableViewController;
  [self.baseNavigationController
      pushViewController:languageSettingsTableViewController
            animated:YES];
}

@end
