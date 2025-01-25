// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/search_engine/vivaldi_search_engine_settings_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/ui/settings/search_engine/vivaldi_search_engine_settings_mediator.h"
#import "ios/ui/settings/search_engine/vivaldi_search_engine_settings_view_controller.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

@interface VivaldiSearchEngineSettingsCoordinator ()
// View controller for the setting.
@property(nonatomic, strong)
    VivaldiSearchEngineSettingsViewController* viewController;
// Mediator for search engine settings.
@property(nonatomic, strong) VivaldiSearchEngineSettingsMediator* mediator;

@end

@implementation VivaldiSearchEngineSettingsCoordinator

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
  self.viewController =
      [[VivaldiSearchEngineSettingsViewController alloc]
                         initWithProfile:self.browser->GetProfile()];
  self.viewController.title =
      l10n_util::GetNSString(IDS_IOS_SEARCH_ENGINE_SETTING_TITLE);
  self.viewController.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;

  self.mediator =
      [[VivaldiSearchEngineSettingsMediator alloc]
          initWithProfile:self.browser->GetProfile()];
  self.mediator.consumer = self.viewController;
  self.viewController.delegate = self.mediator;

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
}

#pragma mark - Private
- (void)handleDoneButtonTap {
  [self stop];
  [self.baseNavigationController dismissViewControllerAnimated:YES
                                                    completion:nil];
}

@end
