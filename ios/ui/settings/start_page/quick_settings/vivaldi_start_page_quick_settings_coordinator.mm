// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/start_page/quick_settings/vivaldi_start_page_quick_settings_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/ui/settings/start_page/quick_settings/quick_settings_swift.h"
#import "ios/ui/settings/start_page/quick_settings/vivaldi_start_page_quick_settings_mediator.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

@interface VivaldiStartPageQuickSettingsCoordinator () {
  // The browser where the settings are being displayed.
  Browser* _browser;
}
// View provider for the start page quick setting.
@property(nonatomic, strong)
    VivaldiStartPageQuickSettingsViewProvider* viewProvider;
// Start page quick settings preference mediator.
@property(nonatomic, strong) VivaldiStartPageQuickSettingsMediator* mediator;

@end

@implementation VivaldiStartPageQuickSettingsCoordinator

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
  VivaldiStartPageQuickSettingsViewProvider* viewProvider =
      [VivaldiStartPageQuickSettingsViewProvider new];
  self.viewProvider = viewProvider;
  UIViewController *controller =
      [VivaldiStartPageQuickSettingsViewProvider makeViewController];
  controller.title = l10n_util::GetNSString(IDS_IOS_START_PAGE_CUSTOMIZE_TITLE);
  controller.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
  controller.modalPresentationStyle = UIModalPresentationPageSheet;

  UINavigationController* navController =
      [[UINavigationController alloc] initWithRootViewController:controller];

  // Add Done button
  UIBarButtonItem* doneItem =
    [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                             target:self
                             action:@selector(handleDoneButtonTap)];
  controller.navigationItem.rightBarButtonItem = doneItem;

  UISheetPresentationController* sheetPc =
      navController.sheetPresentationController;

  sheetPc.detents = @[UISheetPresentationControllerDetent.mediumDetent,
                      UISheetPresentationControllerDetent.largeDetent];
  sheetPc.prefersScrollingExpandsWhenScrolledToEdge = NO;
  sheetPc.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  [self.baseViewController presentViewController:navController
                                        animated:YES
                                      completion:nil];

  self.mediator =
      [[VivaldiStartPageQuickSettingsMediator alloc]
          initWithOriginalPrefService:self.browser->GetProfile()
              ->GetOriginalProfile()->GetPrefs()];
  self.mediator.consumer = self.viewProvider;

  self.viewProvider.settingsStateConsumer = self.mediator;
}

- (void)stop {
  [super stop];
  self.viewProvider.settingsStateConsumer = nil;
  self.viewProvider = nil;
  self.mediator.consumer = nil;
  [self.mediator disconnect];
  self.mediator = nil;
}

- (void)handleDoneButtonTap {
  [self stop];
  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
}

@end
