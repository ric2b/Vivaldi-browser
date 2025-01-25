// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/start_page/reopen_with/vivaldi_start_page_reopen_with_coordinator.h"

#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/ui/settings/start_page/reopen_with/reopen_with_swift.h"
#import "ios/ui/settings/start_page/vivaldi_start_page_prefs.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

@interface VivaldiStartPageReopenWithCoordinator () {
  // The browser where the settings are being displayed.
  Browser* _browser;
  // Pref from application context.
  PrefService* _localPrefs;
}
// View provider for the setting.
@property(nonatomic, strong)
    VivaldiStartPageReopenWithViewProvider* viewProvider;

// View controller for the setting.
@property(nonatomic, strong) UIViewController* viewController;

@end

@implementation VivaldiStartPageReopenWithCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];

  if (self) {
    _browser = browser;
    _baseNavigationController = navigationController;
    _localPrefs = GetApplicationContext()->GetLocalState();
    [VivaldiStartPagePrefs setLocalPrefService:_localPrefs];
  }

  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.viewProvider = [[VivaldiStartPageReopenWithViewProvider alloc] init];

  // Set up controller
  self.viewController =
      [self.viewProvider makeViewControllerWith:[self reopenStartPageWith]];

  self.viewController.title =
      l10n_util::GetNSString(IDS_IOS_START_PAGE_START_PAGE_OPEN_WITH_TITLE);
  self.viewController.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;

  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];

  // Observe tap action
  [self.viewProvider
      observeOpenWithDidChangeEvent:^(VivaldiStartPageStartItemType item) {
    [VivaldiStartPagePrefs setReopenStartPageWithItem:item];

    // If Top Sites is selected, enable Top Sites if not already enabled.
    if (item == VivaldiStartPageStartItemTypeTopSites &&
        ![self topSitesEnabled]) {
      [VivaldiStartPagePrefs setShowFrequentlyVisitedPages:YES];
    }
  }];
}

- (void)stop {
  [super stop];
  self.viewProvider = nil;
  self.viewController = nil;
}

#pragma mark Private

- (VivaldiStartPageStartItemType)reopenStartPageWith {
  return [VivaldiStartPagePrefs getReopenStartPageWithItem];
}

- (BOOL)topSitesEnabled {
  return [VivaldiStartPagePrefs showFrequentlyVisitedPages];
}

@end
