// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/addressbar/vivaldi_addressbar_settings_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/ui/common/vivaldi_url_constants.h"
#import "ios/ui/settings/addressbar/vivaldi_addressbar_settings_mediator.h"
#import "ios/ui/settings/addressbar/vivaldi_addressbar_settings_swift.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

@interface VivaldiAddressBarSettingsCoordinator ()
@property(nonatomic, strong)
    VivaldiAddressBarSettingsViewProvider* viewProvider;
// View controller for the Address bar setting.
@property(nonatomic, strong) UIViewController* viewController;
// Address bar preference mediator.
@property(nonatomic, strong) VivaldiAddressBarSettingsMediator* mediator;

@end

@implementation VivaldiAddressBarSettingsCoordinator

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
  self.viewProvider = [[VivaldiAddressBarSettingsViewProvider alloc] init];

  self.viewController = [self.viewProvider makeViewController];
  self.viewController.title =
      l10n_util::GetNSString(IDS_IOS_PREFS_VIVALDI_ADDRESSBAR);
  self.viewController.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;

  self.mediator = [[VivaldiAddressBarSettingsMediator alloc]
      initWithOriginalPrefService:self.browser->GetProfile()
                                 ->GetOriginalProfile()
                                 ->GetPrefs()];
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

  [self observeTapEvents];
}

- (void)stop {
  [super stop];
  self.viewController = nil;
  [self.mediator disconnect];
  self.mediator = nil;
  self.viewProvider = nil;
}

#pragma mark - Private Actions
- (void)observeTapEvents {
  __weak __typeof(self) weakSelf = self;
  [self.viewProvider observeDMLearnMoreLinkTap:^{
    [weakSelf showDirectMatchLearnMorePage];
  }];
}

- (void)handleDoneButtonTap {
  [self stop];
  [self.baseNavigationController dismissViewControllerAnimated:YES
                                                    completion:nil];
}

#pragma mark - Private

- (void)showDirectMatchLearnMorePage {
  GURL learnMoreURL([vVivaldiDirectMatchLearnMoreUrl UTF8String]);
  UrlLoadParams params = UrlLoadParams::InNewTab(learnMoreURL);
  params.in_incognito = self.browser->GetProfile()->IsOffTheRecord();
  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
  [self handleDoneButtonTap];
}

@end
