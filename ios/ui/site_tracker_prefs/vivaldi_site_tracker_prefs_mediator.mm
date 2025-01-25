// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/site_tracker_prefs/vivaldi_site_tracker_prefs_mediator.h"

#import "base/apple/foundation_util.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/page_info/page_info_site_security_description.h"
#import "ios/chrome/browser/ui/page_info/page_info_site_security_mediator.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/ad_tracker_blocker/cells/vivaldi_atb_setting_item.h"
#import "ios/ui/ad_tracker_blocker/manager/vivaldi_atb_manager.h"
#import "ios/ui/ad_tracker_blocker/settings/vivaldi_atb_settings_view_controller.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_constants.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_setting_type.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_source_type.h"
#import "ios/ui/helpers/vivaldi_colors_helper.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/ntp/vivaldi_speed_dial_constants.h"
#import "ios/ui/site_tracker_prefs/vivaldi_site_tracker_prefs_consumer.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

@interface VivaldiSiteTrackerPrefsMediator ()<CRWWebStateObserver,
                                              VivaldiATBConsumer> {
  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  web::WebState* _webState;
  // Bridge to observe the web state from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  // Track rule group updates.
  BOOL ruleGroupApplied[2];
  // Track if a rule apply in progress. This is set to 'YES' only when user
  // triggers new settings.
  BOOL ruleApplyInProgressForThisSession;
}
// The Browser in which blocker engine is active.
@property(nonatomic, assign) Browser* browser;
// The manager for the adblock that provides all methods and properties for
// adblocker.
@property(nonatomic, strong) VivaldiATBManager* adblockManager;

@end

@implementation VivaldiSiteTrackerPrefsMediator {
  PageInfoSiteSecurityDescription* _siteSecurityDescription;
}

- (instancetype)initWithBrowser:(Browser*)browser {
  self = [super init];
  if (self) {
    _browser = browser;
    _adblockManager = [[VivaldiATBManager alloc] initWithBrowser:_browser];
    _adblockManager.consumer = self;

    [self.consumer setActiveWebStateDomain:self.activePageDomain];
    [self setUpWebStateObserver];
    [self updateSiteSecurityDescription];
  }
  return self;
}

- (void)disconnect {
  [self removeWebStateObserver];

  if (self.adblockManager) {
    self.adblockManager.consumer = nil;
    [self.adblockManager disconnect];
  }
}

#pragma mark - Private Helpers

- (void)setUpWebStateObserver {
  if ([self activeWebState]) {
    _webState = [self activeWebState];
    _webStateObserverBridge =
    std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());
  }
}

- (void)removeWebStateObserver {
  if (_webState && _webStateObserverBridge) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webStateObserverBridge.reset();
  }
}

- (void)updateSiteSecurityDescription {
  _siteSecurityDescription = [self updatedSiteSecurityDescription];
  [self.consumer
      setPageInfoSecurityDescription:_siteSecurityDescription];
}

- (PageInfoSiteSecurityDescription*)updatedSiteSecurityDescription {
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  return [PageInfoSiteSecurityMediator configurationForWebState:webState];
}

// Returns the favicon for the page
- (UIImage*)favicon {
  favicon::FaviconDriver* faviconDriver =
  favicon::WebFaviconDriver::FromWebState([self activeWebState]);
  UIImage* fallbackFavicon = [UIImage imageNamed:vNTPSDFallbackFavicon];
  if (faviconDriver && faviconDriver->FaviconIsValid()) {
    gfx::Image favicon = faviconDriver->GetFavicon();
    if (!favicon.IsEmpty()) {
      return favicon.ToUIImage();
    } else {
      return fallbackFavicon;
    }
  } else {
    return fallbackFavicon;
  }
}

- (web::WebState*)activeWebState {
  return self.browser->GetWebStateList()->GetActiveWebState();
}

- (NSString*)activePageDomain {
  GURL visibleURL = self.activeWebState->GetVisibleURL();
  NSString* visibleURLString = base::SysUTF8ToNSString(visibleURL.spec());
  NSURL *visibleNSURL = [NSURL URLWithString:visibleURLString];
  if (visibleNSURL && visibleNSURL.host) {
    return visibleNSURL.host;
  }
  return nil;
}

- (ATBSettingType)blockingLevelForSite:(NSString*)site {
  if (!self.adblockManager || !site)
    return ATBSettingNoBlocking;
  return [self.adblockManager blockingSettingForDomain:site];
}

- (ATBSettingType)defaultBlockingLevel {
  if (!self.adblockManager)
    return ATBSettingNoBlocking;
  return [self.adblockManager globalBlockingSetting];
}

- (BOOL)isRulesApplying {
  if (!self.adblockManager)
    return NO;
  return [self.adblockManager isApplyingExceptionRules];
}

- (void)notifyCommonConsumers {
  [self.consumer setGlobalBlockingLevel:[self defaultBlockingLevel]];
  [self.consumer
       setBlockingLevelForDomain:
          [self blockingLevelForSite:self.activePageDomain]];
  [self.consumer setRulesGroupApplying:[self isRulesApplying]];
  [self.consumer setActiveWebStateFavicon:[self favicon]];
  [self updateSiteSecurityDescription];
}

#pragma mark - Properties

- (void)setConsumer:(id<VivaldiSiteTrackerPrefsConsumer>)consumer {
  _consumer = consumer;

  // Header
  [self.consumer setActiveWebStateDomain:self.activePageDomain];
  [self.consumer setActiveWebStateFavicon:[self favicon]];

  // Tracker Blocking
  [self.consumer
      setBlockingLevelForDomain:
          [self blockingLevelForSite:self.activePageDomain]];
  [self.consumer setGlobalBlockingLevel:[self defaultBlockingLevel]];
  [self.consumer setRulesGroupApplying:[self isRulesApplying]];

  // Site info
  [self.consumer
      setPageInfoSecurityDescription:_siteSecurityDescription];
}

#pragma mark: - VivaldiATBConsumer

- (void)didRefreshSettingOptions:(NSArray*)options {
  if (options.count == 0)
    return;
  [self notifyCommonConsumers];
}

- (void)rulesListDidEndApplying:(RuleGroup)group {
  // If user explicitely did not trigger settings changes we will avoid
  // listening to the changes. This method can be triggered other ways too.
  [self.consumer setRulesGroupApplying:[self isRulesApplying]];

  if (!ruleApplyInProgressForThisSession)
    return;

  switch (group) {
    case RuleGroup::kTrackingRules:
      ruleGroupApplied[0] = YES;
      break;
    case RuleGroup::kAdBlockingRules:
      ruleGroupApplied[1] = YES;
      break;
  }

  // Check if all groups have been triggered
  if (ruleGroupApplied[0] && ruleGroupApplied[1]) {
    // Dismiss when all rules are applied.
    ruleApplyInProgressForThisSession = NO;
    [self.presentationDelegate viewControllerWantsDismissal];
  }
}

- (void)didRefreshExceptionsList:(NSArray*)exceptions {
  // If we manually trigger the settings from this page, return early.
  // We don't want to listen to this changes.
  if (ruleApplyInProgressForThisSession)
    return;
  // More checks.
  if (!self.activePageDomain ||
      exceptions.count == 0)
    return;
  [self notifyCommonConsumers];
}

#pragma mark - VivaldiSiteTrackerPrefsViewDelegate
- (void)setExceptionForDomain:(NSString*)domain
                 blockingType:(ATBSettingType)blockingType {
  // Set the flag to track if a user triggered rule update in progress or not.
  ruleApplyInProgressForThisSession = YES;
  // Change settings
  if (![VivaldiGlobalHelpers isValidDomain:domain])
    return;
  [self.adblockManager setExceptionForDomain:domain
                                blockingType:blockingType];
}

#pragma mark - CRWWebStateObserver
- (void)webState:(web::WebState*)webState didUpdateFaviconURLCandidates:
    (const std::vector<web::FaviconURL>&)candidates {
  DCHECK_EQ(_webState, webState);
  if (_webState) {
    [self.consumer setActiveWebStateFavicon:[self favicon]];
  }
}

@end
