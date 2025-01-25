// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/site_tracker_prefs/vivaldi_site_tracker_prefs_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/page_info_commands.h"
#import "ios/chrome/browser/ui/page_info/page_info_presentation_commands.h"
#import "ios/chrome/browser/ui/page_info/page_info_security_coordinator.h"
#import "ios/chrome/browser/ui/page_info/page_info_site_security_description.h"
#import "ios/chrome/browser/ui/page_info/page_info_site_security_mediator.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/web/model/web_navigation_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/ad_tracker_blocker/settings/vivaldi_atb_settings_view_controller.h"
#import "ios/ui/site_tracker_prefs/site_tracker_prefs_view_swift.h"
#import "ios/ui/site_tracker_prefs/vivaldi_site_tracker_prefs_mediator.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

@interface VivaldiSiteTrackerPrefsCoordinator()
                              <PageInfoCommands,
                              PageInfoPresentationCommands,
                              UIAdaptivePresentationControllerDelegate,
                              VivaldiSiteTrackerPrefsViewPresentationDelegate> {
  // Coordinator for the security screen.
  PageInfoSecurityCoordinator* _securityCoordinator;
  PageInfoSiteSecurityDescription* _siteSecurityDescription;
}
// View provider for the settings page.
@property(nonatomic, strong) VivaldiSiteTrackerPrefsViewProvider* viewProvider;
// View controller for the settings page.
@property(nonatomic, strong) UIViewController* controller;
// Mediator for the setting
@property(nonatomic, strong) VivaldiSiteTrackerPrefsMediator* mediator;
// Navigation controller where settings controller is presented
@property(nonatomic, strong) UINavigationController* navigationController;

@property(nonatomic, strong) CommandDispatcher* dispatcher;

@end

@implementation VivaldiSiteTrackerPrefsCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  VivaldiSiteTrackerPrefsViewProvider* viewProvider =
      [[VivaldiSiteTrackerPrefsViewProvider alloc] init];
  self.viewProvider = viewProvider;
  self.controller = [self.viewProvider makeViewController];

  self.mediator =
      [[VivaldiSiteTrackerPrefsMediator alloc] initWithBrowser:self.browser];
  self.mediator.presentationDelegate = self;
  self.mediator.consumer = self.viewProvider;

  self.viewProvider.consumer = self.mediator;

  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  _siteSecurityDescription =
      [PageInfoSiteSecurityMediator
          configurationForWebState:webState];

  UINavigationController* navigationController =
      [[UINavigationController alloc]
          initWithRootViewController:self.controller];
  navigationController.presentationController.delegate = self;
  self.navigationController = navigationController;

  UISheetPresentationController* sheetPc =
      navigationController.sheetPresentationController;
  sheetPc.detents = @[UISheetPresentationControllerDetent.mediumDetent,
                      UISheetPresentationControllerDetent.largeDetent];
  sheetPc.prefersScrollingExpandsWhenScrolledToEdge = NO;
  sheetPc.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];

  [self observeTapEvents];
}

- (void)stop {
  [super stop];
  [self.mediator disconnect];
  self.mediator = nil;

  self.viewProvider = nil;
  self.controller = nil;
  self.navigationController = nil;

  if (_securityCoordinator) {
    [_securityCoordinator stop];
    _securityCoordinator = nil;
  }
}

#pragma mark - Private
- (void)observeTapEvents {
  __weak __typeof(self) weakSelf = self;
  [self.viewProvider observeSiteInfoTapEvent:^{
    [weakSelf showSecurityPage];
  }];

  [self.viewProvider observeManagePrivacySettingsTapEvent:^{
    [weakSelf handleBlockerSettingsButtonTap];
  }];

  [self.viewProvider observeDoneButtonTapEvent:^{
    [weakSelf handleDoneButtonTap];
  }];
}

- (void)handleBlockerSettingsButtonTap {
  NSString* settingsTitleString =
      l10n_util::GetNSString(IDS_IOS_PREFS_VIVALDI_AD_AND_TRACKER_BLOCKER);
  VivaldiATBSettingsViewController* settingsController =
      [[VivaldiATBSettingsViewController alloc]
          initWithBrowser:self.browser
              title:settingsTitleString];
  [self.navigationController
      pushViewController:settingsController animated:YES];
}

- (void)handleDoneButtonTap {
  [self stop];
  [self.baseViewController dismissViewControllerAnimated:YES
                                              completion:nil];
}

#pragma mark - VivaldiSiteTrackerPrefsViewPresentationDelegate
- (void)viewControllerWantsDismissal {
  [self handleDoneButtonTap];
}

#pragma mark - UIAdaptivePresentationControllerDelegate
- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self handleDoneButtonTap];
}

#pragma mark - PageInfoPresentationCommands

- (void)showSecurityPage {
  _securityCoordinator =
      [[PageInfoSecurityCoordinator alloc]
          initWithBaseNavigationController:self.navigationController
              browser:self.browser
                  siteSecurityDescription:_siteSecurityDescription];
  _securityCoordinator.pageInfoPresentationHandler = self;
  _securityCoordinator.pageInfoCommandsHandler = self;
  _securityCoordinator.openedViaSiteTrackerPrefModal = YES;
  [_securityCoordinator start];
}

- (void)showSecurityHelpPage {
  UrlLoadParams params = UrlLoadParams::InNewTab(GURL(kPageInfoHelpCenterURL));
  params.in_incognito = self.browser->GetProfile()->IsOffTheRecord();
  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
  [self handleDoneButtonTap];
}

- (void)showAboutThisSitePage:(GURL)URL {
  web::NavigationManager::WebLoadParams webParams =
      web::NavigationManager::WebLoadParams(URL);
  bool in_incognito = self.browser->GetProfile()->IsOffTheRecord();

  // Add X-Client-Data header.
  NSMutableDictionary<NSString*, NSString*>* combinedExtraHeaders =
      [web_navigation_util::VariationHeadersForURL(URL, in_incognito)
          mutableCopy];
  [combinedExtraHeaders addEntriesFromDictionary:webParams.extra_headers];
  webParams.extra_headers = [combinedExtraHeaders copy];
  UrlLoadParams params = UrlLoadParams::InNewTab(webParams);
  params.in_incognito = in_incognito;

  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
  [self handleDoneButtonTap];
}

- (PageInfoSiteSecurityDescription*)updatedSiteSecurityDescription {
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  return [PageInfoSiteSecurityMediator configurationForWebState:webState];
}

- (void)showLastVisitedPage {
  // No op.
}

#pragma mark - PageInfoCommands

- (void)showPageInfo {
  // No op.
}

- (void)hidePageInfo {
  _securityCoordinator.pageInfoCommandsHandler = self;
  [_securityCoordinator stop];
  _securityCoordinator = nil;
  [self handleDoneButtonTap];
}

@end
