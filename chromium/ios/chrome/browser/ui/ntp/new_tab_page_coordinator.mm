// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/time/time.h"
#import "components/feed/core/v2/public/common_enums.h"
#import "components/feed/core/v2/public/ios/pref_names.h"
#import "components/feed/feed_feature_list.h"
#import "components/policy/policy_constants.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "components/search/search.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/supervised_user/core/common/features.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/context_menu/ui_bundled/link_preview/link_preview_coordinator.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_observer_bridge.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service_factory.h"
#import "ios/chrome/browser/discover_feed/model/feed_constants.h"
#import "ios/chrome/browser/discover_feed/model/feed_model_configuration.h"
#import "ios/chrome/browser/follow/model/follow_browser_agent.h"
#import "ios/chrome/browser/follow/model/followed_web_site.h"
#import "ios/chrome/browser/follow/model/followed_web_site_state.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_coordinator.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_delegate.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_state.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/authentication_service_observer_bridge.h"
#import "ios/chrome/browser/signin/model/capabilities_types.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_capabilities_observer_bridge.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_coordinator.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_coordinator_delegate.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_coordinator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_constants.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_manage_delegate.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_preview_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_control_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_header_view_controller.h"
#import "ios/chrome/browser/ui/ntp/feed_management/feed_management_coordinator.h"
#import "ios/chrome/browser/ui/ntp/feed_menu_coordinator.h"
#import "ios/chrome/browser/ui/ntp/feed_sign_in_promo_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_top_section/feed_top_section_coordinator.h"
#import "ios/chrome/browser/ui/ntp/feed_wrapper_view_controller.h"
#import "ios/chrome/browser/ui/ntp/home_start_data_source.h"
#import "ios/chrome/browser/ui/ntp/incognito/incognito_view_controller.h"
#import "ios/chrome/browser/ui/ntp/logo_vendor.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_constants.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_recorder.h"
#import "ios/chrome/browser/ui/ntp/metrics/home_metrics.h"
#import "ios/chrome/browser/ui/ntp/metrics/new_tab_page_metrics_recorder.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_component_factory_protocol.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_content_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_controller_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator+Testing.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_follow_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_commands.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_view_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_mediator.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_metrics_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_view_controller.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "ios/chrome/browser/ui/toolbar/public/fakebox_focuser.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/ui_utils/ui_utils_api.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util_mac.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/ntp/vivaldi_speed_dial_home_coordinator.h"

using vivaldi::IsVivaldiRunning;
// End Vivaldi

@interface NewTabPageCoordinator () <AccountMenuCoordinatorDelegate,
                                     AppStateObserver,
                                     AuthenticationServiceObserving,
                                     BooleanObserver,
                                     ContentSuggestionsDelegate,
                                     DiscoverFeedManageDelegate,
                                     DiscoverFeedObserverBridgeDelegate,
                                     DiscoverFeedPreviewDelegate,
                                     FeedControlDelegate,
                                     FeedMenuCoordinatorDelegate,
                                     FeedSignInPromoDelegate,
                                     FeedWrapperViewControllerDelegate,
                                     HomeCustomizationDelegate,
                                     HomeStartDataSource,
                                     IdentityManagerObserverBridgeDelegate,
                                     NewTabPageContentDelegate,
                                     NewTabPageDelegate,
                                     NewTabPageFollowDelegate,
                                     NewTabPageHeaderCommands,
                                     NewTabPageMetricsDelegate,
                                     OverscrollActionsControllerDelegate,
                                     SceneStateObserver,
                                     SupervisedUserCapabilitiesObserving> {
  // Observes changes in the IdentityManager.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityObserverBridge;

  // Observes changes in the DiscoverFeed.
  std::unique_ptr<DiscoverFeedObserverBridge> _discoverFeedObserverBridge;

  // Observer for auth service status changes.
  std::unique_ptr<AuthenticationServiceObserverBridge>
      _authServiceObserverBridge;

  // Observer to track changes to supervision-related capabilities.
  std::unique_ptr<supervised_user::SupervisedUserCapabilitiesObserverBridge>
      _supervisedUserCapabilitiesObserverBridge;
}

// Coordinator for the ContentSuggestions.
@property(nonatomic, strong)
    ContentSuggestionsCoordinator* contentSuggestionsCoordinator;

// View controller for the regular NTP.
@property(nonatomic, strong) NewTabPageViewController* NTPViewController;

// Mediator owned by this coordinator.
@property(nonatomic, strong) NewTabPageMediator* NTPMediator;

// View controller wrapping the feed.
@property(nonatomic, strong)
    FeedWrapperViewController* feedWrapperViewController;

// View controller for the incognito NTP.
@property(nonatomic, strong) IncognitoViewController* incognitoViewController;

// The timetick of the last time the NTP was displayed.
@property(nonatomic, assign) base::TimeTicks didAppearTime;

// Tracks the visibility of the NTP to report NTP usage metrics.
// True if the NTP view is currently displayed to the user.
// Redefined to readwrite.
@property(nonatomic, assign, readwrite) BOOL visible;

// The ViewController displayed by this Coordinator. This is the returned
// ViewController and will contain the `containedViewController` (Which can
// change depending on Feed visibility).
@property(nonatomic, strong) UIViewController* containerViewController;

// The coordinator contained ViewController.
@property(nonatomic, strong) UIViewController* containedViewController;

// PrefService used by this Coordinator.
@property(nonatomic, assign) PrefService* prefService;

// Whether the feed is expanded or collapsed. Collapsed
// means the feed header is shown, but not any of the feed content.
@property(nonatomic, strong) PrefBackedBoolean* feedExpandedPref;

// The view controller representing the selected feed, such as the Discover or
// Following feed.
@property(nonatomic, weak) UIViewController* feedViewController;

// The Coordinator to display previews for Discover feed websites. It also
// handles the actions related to them.
@property(nonatomic, strong) LinkPreviewCoordinator* linkPreviewCoordinator;

// The view controller representing the NTP feed header.
@property(nonatomic, strong) FeedHeaderViewController* feedHeaderViewController;

// Coordinator for handling the feed menu.
@property(nonatomic, strong) FeedMenuCoordinator* feedMenuCoordinator;

// Authentication Service for the user's signed-in state.
@property(nonatomic, assign) AuthenticationService* authService;

// TemplateURL used to get the search engine.
@property(nonatomic, assign) TemplateURLService* templateURLService;

// DiscoverFeed Service to display the Feed.
@property(nonatomic, assign) DiscoverFeedService* discoverFeedService;

// Metrics recorder for actions relating to the feed.
@property(nonatomic, weak) FeedMetricsRecorder* feedMetricsRecorder;

// The header view controller containing the fake omnibox and logo.
@property(nonatomic, strong)
    NewTabPageHeaderViewController* headerViewController;

// The coordinator for handling feed management.
@property(nonatomic, strong)
    FeedManagementCoordinator* feedManagementCoordinator;

// Coordinator for Feed top section.
@property(nonatomic, strong)
    FeedTopSectionCoordinator* feedTopSectionCoordinator;

// Currently selected feed. Redefined to readwrite.
@property(nonatomic, assign, readwrite) FeedType selectedFeed;

// The Webstate associated with this coordinator.
@property(nonatomic, assign) web::WebState* webState;

// Returns `YES` if the coordinator is started.
@property(nonatomic, assign) BOOL started;

// Contains a factory which can generate NTP components which are initialized
// on `start`.
@property(nonatomic, strong) id<NewTabPageComponentFactoryProtocol>
    componentFactory;

// Recorder for new tab page metrics.
@property(nonatomic, strong) NewTabPageMetricsRecorder* NTPMetricsRecorder;

// Logo vendor to display the doodle on the NTP.
@property(nonatomic, strong) id<LogoVendor> logoVendor;

@end

@implementation NewTabPageCoordinator {
  // Coordinator in charge of handling sharing use cases.
  SharingCoordinator* _sharingCoordinator;
  // Coordinator in charge of fast account menu.
  AccountMenuCoordinator* _accountMenuCoordinator;
  // Coordinator for presenting the Home customization menu.
  HomeCustomizationCoordinator* _customizationCoordinator;

  // Vivaldi
  VivaldiSpeedDialHomeCoordinator* _speedDialHomeCoordinator;
  // End Vivaldi

}

// Synthesize NewTabPageConfiguring properties.
@synthesize shouldScrollIntoFeed = _shouldScrollIntoFeed;
@synthesize baseViewController = _baseViewController;

#pragma mark - ChromeCoordinator

- (instancetype)initWithBrowser:(Browser*)browser
               componentFactory:
                   (id<NewTabPageComponentFactoryProtocol>)componentFactory {
  DCHECK(browser);
  self = [super initWithBaseViewController:nil browser:browser];
  if (self) {
    _componentFactory = componentFactory;
    _containerViewController = [[UIViewController alloc] init];
  }
  return self;
}

- (void)start {
  if (self.started) {
    return;
  }

  DCHECK(self.browser);
  DCHECK(self.toolbarDelegate);
  DCHECK(!self.contentSuggestionsCoordinator);

  self.webState = self.browser->GetWebStateList()->GetActiveWebState();
  DCHECK(self.webState);
  DCHECK(NewTabPageTabHelper::FromWebState(self.webState)->IsActive());

  // Start observing SceneState changes.
  SceneState* sceneState = self.browser->GetSceneState();
  [sceneState addObserver:self];

  // Configures incognito NTP if user is in incognito mode.
  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    DCHECK(!self.incognitoViewController);
    UrlLoadingBrowserAgent* URLLoader =
        UrlLoadingBrowserAgent::FromBrowser(self.browser);
    self.incognitoViewController =
        [[IncognitoViewController alloc] initWithUrlLoader:URLLoader];
    self.started = YES;
    return;
  }

  // NOTE: anything that executes below WILL NOT execute for OffTheRecord
  // browsers!

  if (IsVivaldiRunning()) {
    [self initializeServices];
    _speedDialHomeCoordinator =
        [[VivaldiSpeedDialHomeCoordinator alloc]
            initWithBaseViewController:nil
                               browser:self.browser];
    [_speedDialHomeCoordinator start];
    [self configureVivaldiNTPViewController];
  } else {
  self.selectedFeed = NewTabPageTabHelper::FromWebState(self.webState)
                          ->GetNTPState()
                          .selectedFeed;

  [self initializeServices];
  [self initializeNTPComponents];
  [self startObservers];

  // Do not focus on omnibox for voice over if there are other screens to
  // show.
  AppState* appState = sceneState.appState;
  [appState addObserver:self];
  if (appState.initStage < InitStageFinal) {
    self.NTPViewController.focusAccessibilityOmniboxWhenViewAppears = NO;
  }

  // Update the feed if the account is subject to parental controls.
  if (base::FeatureList::IsEnabled(
          supervised_user::
              kReplaceSupervisionSystemCapabilitiesWithAccountCapabilitiesOnIOS)) {
    signin::IdentityManager* identityManager =
        IdentityManagerFactory::GetForBrowserState(
            self.browser->GetBrowserState());
    signin::Tribool capability =
        supervised_user::IsPrimaryAccountSubjectToParentalControls(
            identityManager);
    [self
        updateFeedWithIsSupervisedUser:(capability == signin::Tribool::kTrue)];
  } else {
    // Update asynchronously using system capabilities.
    [self updateFeedVisibilityForSupervision];
  }

  [self configureNTPMediator];
  if (self.NTPMediator.feedHeaderVisible) {
    [self configureFeedAndHeader];
  }
  [self configureHeaderViewController];
  [self configureContentSuggestionsCoordinator];
  [self configureFeedMetricsRecorder];
  [self configureNTPViewController];
  } // End Vivaldi

  self.started = YES;
}

- (void)stop {
  if (!self.started) {
    return;
  }

  _webState = nullptr;

  SceneState* sceneState = self.browser->GetSceneState();
  [sceneState removeObserver:self];

  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    self.incognitoViewController = nil;
    self.started = NO;
    return;
  }

  // NOTE: anything that executes below WILL NOT execute for OffTheRecord
  // browsers!

  [sceneState.appState removeObserver:self];

  [self.feedManagementCoordinator stop];
  self.feedManagementCoordinator = nil;
  [self.contentSuggestionsCoordinator stop];
  self.contentSuggestionsCoordinator = nil;
  self.headerViewController = nil;
  // Remove before nil to ensure View Hierarchy doesn't hold last strong
  // reference.
  [self.containedViewController willMoveToParentViewController:nil];
  [self.containedViewController.view removeFromSuperview];
  [self.containedViewController removeFromParentViewController];
  self.containedViewController = nil;
  [self.NTPViewController invalidate];
  self.NTPViewController = nil;
  self.feedHeaderViewController.NTPDelegate = nil;
  self.feedHeaderViewController = nil;
  [self.feedTopSectionCoordinator stop];
  self.feedTopSectionCoordinator = nil;

  self.NTPMetricsRecorder = nil;

  [self.linkPreviewCoordinator stop];
  self.linkPreviewCoordinator = nil;

  self.authService = nil;
  self.templateURLService = nil;
  self.prefService = nil;

  [self.NTPMediator shutdown];
  self.NTPMediator = nil;

  if (self.feedViewController) {
    self.discoverFeedService->RemoveFeedViewController(self.feedViewController);
  }
  self.feedWrapperViewController = nil;
  self.feedViewController = nil;
  self.feedMetricsRecorder.followDelegate = nil;
  self.feedMetricsRecorder.NTPMetricsDelegate = nil;
  self.feedMetricsRecorder = nil;

  [self.feedExpandedPref setObserver:nil];
  self.feedExpandedPref = nil;

  [self.feedMenuCoordinator stop];
  self.feedMenuCoordinator = nil;

  _supervisedUserCapabilitiesObserverBridge.reset();
  _discoverFeedObserverBridge.reset();
  _identityObserverBridge.reset();
  _authServiceObserverBridge.reset();

  [_sharingCoordinator stop];
  _sharingCoordinator = nil;

  [_customizationCoordinator stop];
  _customizationCoordinator = nil;

  [self stopAccountMenuCoordinator];

  // Vivaldi
  [_speedDialHomeCoordinator stop];
  _speedDialHomeCoordinator = nil;
  // End Vivaldi

  self.started = NO;
}

#pragma mark - Public

- (void)stopIfNeeded {
  WebStateList* webStateList = self.browser->GetWebStateList();
  for (int i = 0; i < webStateList->count(); i++) {
    NewTabPageTabHelper* iterNtpHelper =
        NewTabPageTabHelper::FromWebState(webStateList->GetWebStateAt(i));
    if (iterNtpHelper->IsActive()) {
      return;
    }
  }
  // No active NTPs were found.
  [self stop];
}

- (BOOL)isNTPActiveForCurrentWebState {
  if (!self.webState) {
    return NO;
  }
  NewTabPageTabHelper* NTPHelper =
      NewTabPageTabHelper::FromWebState(self.webState);
  return NTPHelper && NTPHelper->IsActive();
}

- (BOOL)isScrolledToTop {
  return [self.NTPViewController isNTPScrolledToTop];
}

- (void)willUpdateSnapshot {
  if (self.contentSuggestionsCoordinator.started) {
    [self.NTPViewController willUpdateSnapshot];
  }
}

- (void)focusFakebox {
  [self.NTPViewController focusOmnibox];
}

- (void)reload {
  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    return;
  }
  [self.contentSuggestionsCoordinator refresh];
  // Call this before RefreshFeed() to ensure some NTP state configs are reset
  // before callbacks in repsonse to a feed refresh are called, ensuring the NTP
  // returns to a state at the top of the surface upon refresh.
  [self.NTPViewController resetStateUponReload];
  self.discoverFeedService->RefreshFeed(
      FeedRefreshTrigger::kForegroundUserTriggered);
}

- (void)locationBarDidBecomeFirstResponder {
  [self.NTPViewController omniboxDidBecomeFirstResponder];
}

- (void)locationBarWillResignFirstResponder {
  [self.NTPViewController omniboxWillResignFirstResponder];
}

- (void)locationBarDidResignFirstResponder {
  [self.NTPViewController omniboxDidResignFirstResponder];
}

- (void)constrainFeedHeaderManagementButtonNamedGuide {
  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    return;
  }
  [LayoutGuideCenterForBrowser(self.browser)
      referenceView:self.feedHeaderViewController.managementButton
          underName:kFeedHeaderManagementButtonGuide];
}

- (void)updateFollowingFeedHasUnseenContent:(BOOL)hasUnseenContent {
  if (![self isFollowingFeedAvailable] ||
      !IsDotEnabledForNewFollowedContent()) {
    return;
  }
  if ([self doesFollowingFeedHaveContent]) {
    [self.feedHeaderViewController
        updateFollowingDotForUnseenContent:hasUnseenContent];
  }
}

- (void)handleFeedModelOfType:(FeedType)feedType
                didEndUpdates:(FeedLayoutUpdateType)updateType {
  DCHECK(self.NTPViewController);
  if (!self.feedViewController) {
    return;
  }
  // When the visible feed has been updated, recalculate the minimum NTP height.
  if (feedType == self.selectedFeed) {
    [self.NTPViewController feedLayoutDidEndUpdatesWithType:updateType];
  }
}

- (void)didNavigateToNTPInWebState:(web::WebState*)webState {
  CHECK(self.started);
  self.webState = webState;
  [self restoreNTPState];
  [self updateNTPIsVisible:YES];
  [self updateStartForVisibilityChange:YES];
  [self.toolbarDelegate didNavigateToNTPOnActiveWebState];
}

- (void)didNavigateAwayFromNTP {
  [self cancelOmniboxEdit];
  [self saveNTPState];
  [self updateNTPIsVisible:NO];
  [self updateStartForVisibilityChange:NO];
  self.webState = nullptr;
}

- (BOOL)isFakeboxPinned {
  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    return YES;
  }
  return self.NTPViewController.isFakeboxPinned;
}

#pragma mark - Setters

- (void)setSelectedFeed:(FeedType)selectedFeed {
  if (_selectedFeed == selectedFeed) {
    return;
  }
  // Updates the NTP state with the newly selected feed.
  [self saveNTPState];

  // Tell Metrics Recorder the feed has changed.
  [self.feedMetricsRecorder recordFeedTypeChangedFromFeed:_selectedFeed];
  _selectedFeed = selectedFeed;
}

#pragma mark - Initializers

// Gets all NTP services from the browser state.
- (void)initializeServices {
  self.authService = AuthenticationServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());
  self.templateURLService = ios::TemplateURLServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());
  self.discoverFeedService = DiscoverFeedServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());
  self.prefService =
      ChromeBrowserState::FromBrowserState(self.browser->GetBrowserState())
          ->GetPrefs();
}

// Starts all NTP observers.
- (void)startObservers {
  DCHECK(self.prefService);
  DCHECK(self.headerViewController);

  self.feedExpandedPref = [[PrefBackedBoolean alloc]
      initWithPrefService:self.prefService
                 prefName:feed::prefs::kArticlesListVisible];
  // Observer is necessary for multiwindow NTPs to remain in sync.
  [self.feedExpandedPref setObserver:self];

  // Start observing IdentityManager.
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  _identityObserverBridge =
      std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                              self);

  // Start observing supervised user capabilities.
  _supervisedUserCapabilitiesObserverBridge = std::make_unique<
      supervised_user::SupervisedUserCapabilitiesObserverBridge>(
      identityManager, self);

  // Start observing DiscoverFeedService.
  _discoverFeedObserverBridge = std::make_unique<DiscoverFeedObserverBridge>(
      self, self.discoverFeedService);

  // Start observing Authentication service.
  _authServiceObserverBridge =
      std::make_unique<AuthenticationServiceObserverBridge>(self.authService,
                                                            self);
}

// Creates all the NTP components.
- (void)initializeNTPComponents {
  Browser* browser = self.browser;
  id<NewTabPageComponentFactoryProtocol> componentFactory =
      self.componentFactory;
  self.logoVendor = ios::provider::CreateLogoVendor(browser, self.webState);
  self.NTPViewController = [componentFactory NTPViewController];
  self.headerViewController =
      [componentFactory headerViewControllerForBrowser:browser];
  self.NTPMediator =
      [componentFactory NTPMediatorForBrowser:browser
                     identityDiscImageUpdater:self.headerViewController];
  self.contentSuggestionsCoordinator =
      [componentFactory contentSuggestionsCoordinatorForBrowser:browser];
  self.feedMetricsRecorder =
      [componentFactory feedMetricsRecorderForBrowser:browser];
  self.NTPMetricsRecorder = [[NewTabPageMetricsRecorder alloc] init];
}

#pragma mark - Configurators

// Creates and configures the feed and feed header based on user prefs.
- (void)configureFeedAndHeader {
  CHECK(self.NTPMediator.feedHeaderVisible);
  CHECK(self.NTPViewController);

  if (!self.feedHeaderViewController) {
    BOOL followingDotVisible = NO;
    if (IsDotEnabledForNewFollowedContent() && IsWebChannelsEnabled()) {
      // Only show the dot if the user follows available publishers.
      followingDotVisible =
          [self doesFollowingFeedHaveContent] &&
          self.discoverFeedService->GetFollowingFeedHasUnseenContent() &&
          self.selectedFeed != FeedTypeFollowing;
    }

    self.feedHeaderViewController = [self.componentFactory
        feedHeaderViewControllerWithFollowingDotVisible:followingDotVisible];
    self.feedMenuCoordinator = [[FeedMenuCoordinator alloc]
        initWithBaseViewController:self.NTPViewController
                           browser:self.browser];
    self.feedMenuCoordinator.delegate = self;
    [self.feedMenuCoordinator start];
    self.feedHeaderViewController.feedMenuHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), FeedMenuCommands);
  }

  self.feedHeaderViewController.feedControlDelegate = self;
  self.feedHeaderViewController.NTPDelegate = self;
  self.feedHeaderViewController.feedMetricsRecorder = self.feedMetricsRecorder;
  if (!IsFollowUIUpdateEnabled()) {
    self.feedHeaderViewController.followingFeedSortType =
        self.followingFeedSortType;
  }
  self.NTPViewController.feedHeaderViewController =
      self.feedHeaderViewController;

  // Requests feeds here if the correct flags and prefs are enabled.
  if ([self shouldFeedBeVisible]) {
    if ([self isFollowingFeedAvailable] &&
        self.selectedFeed == FeedTypeFollowing) {
      self.feedViewController = [self.componentFactory
              followingFeedForBrowser:self.browser
          viewControllerConfiguration:[self feedViewControllerConfiguration]
                             sortType:self.followingFeedSortType];
    } else {
      self.feedViewController = [self.componentFactory
               discoverFeedForBrowser:self.browser
          viewControllerConfiguration:[self feedViewControllerConfiguration]];
    }
  }

  // Feed top section visibility is based on feed visibility, so this should
  // always be below the block that sets `feedViewController`.
  if ([self isFeedVisible]) {
    self.feedTopSectionCoordinator = [self createFeedTopSectionCoordinator];
  }
}

// Configures `self.headerViewController`.
- (void)configureHeaderViewController {
  DCHECK(self.headerViewController);
  DCHECK(self.NTPMediator);
  DCHECK(self.NTPMetricsRecorder);

  self.headerViewController.isGoogleDefaultSearchEngine =
      [self isGoogleDefaultSearchEngine];
  // TODO(crbug.com/40670043): Use HandlerForProtocol after commands protocol
  // clean up.
  self.headerViewController.dispatcher =
      static_cast<id<ApplicationCommands, BrowserCoordinatorCommands,
                     OmniboxCommands, FakeboxFocuser, LensCommands>>(
          self.browser->GetCommandDispatcher());
  self.headerViewController.commandHandler = self;
  self.headerViewController.delegate = self.NTPViewController;
  self.headerViewController.layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);
  self.headerViewController.toolbarDelegate = self.toolbarDelegate;
  self.headerViewController.baseViewController = self.baseViewController;
  self.headerViewController.NTPMetricsRecorder = self.NTPMetricsRecorder;
  [self.headerViewController setLogoVendor:self.logoVendor];
}

// Configures `self.contentSuggestionsCoordiantor`.
- (void)configureContentSuggestionsCoordinator {
  self.contentSuggestionsCoordinator.webState = self.webState;
  self.contentSuggestionsCoordinator.delegate = self;
  self.contentSuggestionsCoordinator.NTPMetricsDelegate = self;
  self.contentSuggestionsCoordinator.homeStartDataSource = self;
  [self.contentSuggestionsCoordinator start];
}

// Configures `self.NTPMediator`.
- (void)configureNTPMediator {
  NewTabPageMediator* NTPMediator = self.NTPMediator;
  DCHECK(NTPMediator);
  NTPMediator.feedControlDelegate = self;
  NTPMediator.NTPContentDelegate = self;
  NTPMediator.headerConsumer = self.headerViewController;
  NTPMediator.consumer = self.NTPViewController;
  [NTPMediator setUp];
}

// Configures `self.feedMetricsRecorder`.
- (void)configureFeedMetricsRecorder {
  CHECK(self.webState);
  self.feedMetricsRecorder.NTPState =
      NewTabPageTabHelper::FromWebState(self.webState)->GetNTPState();
  self.feedMetricsRecorder.followDelegate = self;
  self.feedMetricsRecorder.NTPMetricsDelegate = self;
}

// Configures `self.NTPViewController` and sets it up as the main ViewController
// managed by this Coordinator.
- (void)configureNTPViewController {
  DCHECK(self.NTPViewController);

  self.NTPViewController.magicStackCollectionView =
      self.contentSuggestionsCoordinator.magicStackCollectionView;
  self.NTPViewController.contentSuggestionsViewController =
      self.contentSuggestionsCoordinator.viewController;

  self.NTPViewController.feedVisible = [self isFeedVisible];

  self.feedWrapperViewController = [self.componentFactory
      feedWrapperViewControllerWithDelegate:self
                         feedViewController:self.feedViewController];

  if ([self isFeedVisible]) {
    self.NTPViewController.feedTopSectionViewController =
        self.feedTopSectionCoordinator.viewController;
  }

  self.NTPViewController.feedWrapperViewController =
      self.feedWrapperViewController;
  self.NTPViewController.overscrollDelegate = self;
  self.NTPViewController.NTPContentDelegate = self;

  self.NTPViewController.headerViewController = self.headerViewController;

  [self configureMainViewControllerUsing:self.NTPViewController];
  self.NTPViewController.feedMetricsRecorder = self.feedMetricsRecorder;
  self.NTPViewController.bubblePresenter = self.bubblePresenter;
  self.NTPViewController.mutator = self.NTPMediator;
}

// Configures the main ViewController managed by this Coordinator.
- (void)configureMainViewControllerUsing:
    (UIViewController*)containedViewController {
  [containedViewController
      willMoveToParentViewController:self.containerViewController];
  [self.containerViewController addChildViewController:containedViewController];
  [self.containerViewController.view addSubview:containedViewController.view];
  [containedViewController
      didMoveToParentViewController:self.containerViewController];

  containedViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(containedViewController.view,
                     self.containerViewController.view);

  self.containedViewController = containedViewController;
}

#pragma mark - Properties

- (UIViewController*)viewController {
  DCHECK(self.started);
  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    return self.incognitoViewController;
  } else {
    return self.containerViewController;
  }
}

#pragma mark - NewTabPageConfiguring

- (void)selectFeedType:(FeedType)feedType {
  if (!self.NTPViewController.viewDidAppear ||
      ![self isFollowingFeedAvailable]) {
    self.selectedFeed = feedType;
    return;
  }
  [self handleFeedSelected:feedType];
}

#pragma mark - NewTabPageHeaderCommands

- (void)updateForHeaderSizeChange {
  [self.NTPViewController updateHeightAboveFeed];
}

- (void)fakeboxTapped {
  [self focusFakebox];
}

- (void)identityDiscWasTapped:(UIView*)identityDisc {
  [self.NTPMetricsRecorder recordIdentityDiscTapped];
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);

  BOOL isSignedIn =
      self.authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (![self isSignInAllowed]) {
    [handler showSettingsFromViewController:self.baseViewController];
  } else if (isSignedIn) {
    if (base::FeatureList::IsEnabled(kIdentityDiscAccountMenu)) {
      _accountMenuCoordinator = [[AccountMenuCoordinator alloc]
          initWithBaseViewController:self.baseViewController
                             browser:self.browser];
      _accountMenuCoordinator.delegate = self;
      _accountMenuCoordinator.anchorView = identityDisc;
      // TODO(crbug.com/336719423): Record signin metrics based on the selected
      // action from the account switcher.
      [_accountMenuCoordinator start];
    } else {
      [handler showSettingsFromViewController:self.baseViewController];
    }
  } else {
    ShowSigninCommand* const showSigninCommand = [[ShowSigninCommand alloc]
        initWithOperation:AuthenticationOperation::kSheetSigninAndHistorySync
              accessPoint:signin_metrics::AccessPoint::
                              ACCESS_POINT_NTP_SIGNED_OUT_ICON];
    [handler showSignin:showSigninCommand
        baseViewController:self.baseViewController];
  }
}

- (void)customizationMenuWasTapped:(UIView*)customizationMenu {
  if (!_customizationCoordinator) {
    _customizationCoordinator = [[HomeCustomizationCoordinator alloc]
        initWithBaseViewController:self.NTPViewController
                           browser:self.browser];
    _customizationCoordinator.delegate = self;
    [_customizationCoordinator start];
  }
  [_customizationCoordinator
      presentCustomizationMenuAtPage:CustomizationMenuPage::kMain];
}

#pragma mark - FeedMenuCoordinatorDelegate

- (void)didSelectFeedMenuItem:(FeedMenuItemType)item {
  switch (item) {
    case FeedMenuItemType::kTurnOff:
      [self setFeedVisibleFromHeader:NO];
      break;
    case FeedMenuItemType::kTurnOn:
      [self setFeedVisibleFromHeader:YES];
      break;
    case FeedMenuItemType::kManage:
      [self handleFeedManageTapped];
      break;
    case FeedMenuItemType::kManageActivity:
      [self.NTPMediator handleNavigateToActivity];
      break;
    case FeedMenuItemType::kManageFollowing:
      [self.NTPMediator handleNavigateToFollowing];
      break;
    case FeedMenuItemType::kLearnMore:
      [self.NTPMediator handleFeedLearnMoreTapped];
      break;
  }
}

#pragma mark - DiscoverFeedManageDelegate

- (void)didTapDiscoverFeedManage {
  [self handleFeedManageTapped];
}

#pragma mark - DiscoverFeedPreviewDelegate

- (UIViewController*)discoverFeedPreviewWithURL:(const GURL)URL {
  std::string referrerURL = base::GetFieldTrialParamValueByFeature(
      kOverrideFeedSettings, kFeedSettingDiscoverReferrerParameter);
  if (referrerURL.empty()) {
    referrerURL = kDefaultDiscoverReferrer;
  }

  self.linkPreviewCoordinator =
      [[LinkPreviewCoordinator alloc] initWithBrowser:self.browser URL:URL];
  self.linkPreviewCoordinator.referrer =
      web::Referrer(GURL(referrerURL), web::ReferrerPolicyDefault);
  [self.linkPreviewCoordinator start];
  return [self.linkPreviewCoordinator linkPreviewViewController];
}

- (void)didTapDiscoverFeedPreview {
  DCHECK(self.linkPreviewCoordinator);
  [self.linkPreviewCoordinator handlePreviewAction];
  [self.linkPreviewCoordinator stop];
  self.linkPreviewCoordinator = nil;
}

#pragma mark - FeedControlDelegate

- (FollowingFeedSortType)followingFeedSortType {
  // TODO(crbug.com/40858105): Add a DCHECK to make sure the coordinator isn't
  // stopped when we check this. That would require us to use the NTPHelper to
  // get this information.
  return (FollowingFeedSortType)self.prefService->GetInteger(
      prefs::kNTPFollowingFeedSortType);
}

- (void)handleFeedSelected:(FeedType)feedType {
  DCHECK([self isFollowingFeedAvailable]);

  if (self.selectedFeed == feedType) {
    return;
  }
  self.selectedFeed = feedType;

  // Saves scroll position before changing feed.
  CGFloat scrollPosition = [self.NTPViewController scrollPosition];

  if (feedType == FeedTypeFollowing && IsDotEnabledForNewFollowedContent()) {
    // Clears dot and notifies service that the Following feed content has
    // been seen.
    [self.feedHeaderViewController updateFollowingDotForUnseenContent:NO];
    self.discoverFeedService->SetFollowingFeedContentSeen();
  }

  [self handleChangeInModules];

  // Scroll position resets when changing the feed, so we set it back to what it
  // was.
  [self.NTPViewController setContentOffsetToTopOfFeedOrLess:scrollPosition];
}

- (void)handleSortTypeForFollowingFeed:(FollowingFeedSortType)sortType {
  DCHECK([self isFollowingFeedAvailable]);

  if (self.feedHeaderViewController.followingFeedSortType == sortType) {
    return;
  }

  // Save the scroll position before changing sort type.
  CGFloat scrollPosition = [self.NTPViewController scrollPosition];

  [self.feedMetricsRecorder recordFollowingFeedSortTypeSelected:sortType];
  self.prefService->SetInteger(prefs::kNTPFollowingFeedSortType, sortType);
  self.prefService->SetBoolean(prefs::kDefaultFollowingFeedSortTypeChanged,
                               true);
  self.discoverFeedService->SetFollowingFeedSortType(sortType);
  self.feedHeaderViewController.followingFeedSortType = sortType;

  [self handleChangeInModules];

  // Scroll position resets when changing the feed, so we set it back to what it
  // was.
  [self.NTPViewController setContentOffsetToTopOfFeedOrLess:scrollPosition];

  // Updates the NTP state for the newly selected sort type.
  [self saveNTPState];
}

- (BOOL)shouldFeedBeVisible {
  return self.NTPMediator.feedHeaderVisible && [self.feedExpandedPref value];
}

- (BOOL)isFollowingFeedAvailable {
  return IsWebChannelsEnabled() && self.authService &&
         self.authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
}

- (NSUInteger)lastVisibleFeedCardIndex {
  return [self.feedWrapperViewController lastVisibleFeedCardIndex];
}

- (void)setFeedAndHeaderVisibility:(BOOL)visible {
  if (!self.NTPViewController.viewLoaded) {
    return;
  }
  [self handleChangeInModules];
  [self.NTPViewController setContentOffsetToTop];
}

- (void)updateFeedForDefaultSearchEngineChanged {
  if (!self.NTPViewController.viewLoaded) {
    return;
  }
  [self.feedHeaderViewController updateForDefaultSearchEngineChanged];
  [self updateFeedLayout];
  [self cancelOmniboxEdit];
  [self.NTPViewController setContentOffsetToTop];
}

#pragma mark - ContentSuggestionsDelegate

- (void)contentSuggestionsWasUpdated {
  [self.NTPViewController updateHeightAboveFeed];
}

- (void)shareURL:(const GURL&)URL
           title:(NSString*)title
        fromView:(UIView*)view {
  SharingParams* params =
      [[SharingParams alloc] initWithURL:URL
                                   title:title
                                scenario:SharingScenario::MostVisitedEntry];
  _sharingCoordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:self.NTPViewController
                         browser:self.browser
                          params:params
                      originView:view];
  [_sharingCoordinator start];
}

#pragma mark - FeedSignInPromoDelegate

- (void)showSignInPromoUI {
  // Both possible flows (sign-in only and sign-in + sync) involve sign-in. So
  // they shouldn't be offered if sign-in is disallowed.
  if (![self isSignInAllowed]) {
    [self showSignInDisableMessage];
    [self.feedMetricsRecorder recordShowSignInRelatedUIWithType:
                                  feed::FeedSignInUI::kShowSignInDisableToast];
    return;
  }

  BOOL hasUserIdentities =
      ChromeAccountManagerServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState())
          ->HasIdentities();
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kSigninOnly
            accessPoint:signin_metrics::AccessPoint::
                            ACCESS_POINT_NTP_FEED_CARD_MENU_PROMO];
  [handler showSignin:command baseViewController:self.NTPViewController];
  [self.feedMetricsRecorder recordShowSignInRelatedUIWithType:
                                feed::FeedSignInUI::kShowSignInOnlyFlow];
  [self.feedMetricsRecorder recordShowSignInOnlyUIWithUserId:hasUserIdentities];
  signin_metrics::RecordSigninUserActionForAccessPoint(
      signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_CARD_MENU_PROMO);
}

- (void)showSignInUI {
  // Both possible flows (sign-in only and sign-in + sync) involve sign-in. So
  // they shouldn't be offered if sign-in is disallowed.
  if (![self isSignInAllowed]) {
    [self showSignInDisableMessage];
    [self.feedMetricsRecorder recordShowSyncnRelatedUIWithType:
                                  feed::FeedSyncPromo::kShowDisableToast];
    return;
  }

  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  // If there are 0 identities, kInstantSignin requires less taps.
  auto operation =
      ChromeAccountManagerServiceFactory::GetForBrowserState(browserState)
              ->HasIdentities()
          ? AuthenticationOperation::kSigninOnly
          : AuthenticationOperation::kInstantSignin;
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:operation
            accessPoint:signin_metrics::AccessPoint::
                            ACCESS_POINT_NTP_FEED_BOTTOM_PROMO];
  [handler showSignin:command baseViewController:self.NTPViewController];
  // TODO(crbug.com/40066051): Strictly speaking this should record a bucket
  // other than kShowSyncFlow. But I don't think we care too much about this
  // particular histogram, just rename the bucket after launch.
  [self.feedMetricsRecorder
      recordShowSyncnRelatedUIWithType:feed::FeedSyncPromo::kShowSyncFlow];
  signin_metrics::RecordSigninUserActionForAccessPoint(
      signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_BOTTOM_PROMO);
}

#pragma mark - FeedWrapperViewControllerDelegate

- (void)updateTheme {
  self.discoverFeedService->UpdateTheme();
}

#pragma mark - NewTabPageContentDelegate

- (BOOL)isContentHeaderSticky {
  return [self isFollowingFeedAvailable] &&
         self.NTPMediator.feedHeaderVisible &&
         !IsStickyHeaderDisabledForFollowingFeed();
}

- (void)signinPromoHasChangedVisibility:(BOOL)visible {
  [self.feedTopSectionCoordinator signinPromoHasChangedVisibility:visible];
}

- (void)cancelOmniboxEdit {
  id<OmniboxCommands> omniboxCommandHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), OmniboxCommands);
  [omniboxCommandHandler cancelOmniboxEdit];
}

- (void)onFakeboxBlur {
  id<FakeboxFocuser> fakeboxFocuserHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), FakeboxFocuser);
  [fakeboxFocuserHandler onFakeboxBlur];
}

- (void)focusOmnibox {
  id<FakeboxFocuser> fakeboxFocuserHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), FakeboxFocuser);
  [fakeboxFocuserHandler focusOmniboxFromFakeboxPinned:[self isFakeboxPinned]];
}

- (void)refreshNTPContent {
  self.discoverFeedService->RefreshFeed(
      FeedRefreshTrigger::kForegroundFeedVisibleOther);
}

- (void)updateForSelectedFeed:(FeedType)selectedFeed {
  [self selectFeedType:selectedFeed];
  if (!IsFollowUIUpdateEnabled()) {
    // Reassign the sort type in case it changed in another tab.
    self.feedHeaderViewController.followingFeedSortType =
        self.followingFeedSortType;
  }
  // Update the header so that it's synced with the currently selected
  // feed, which could have been changed when a new web state was
  // inserted.
  [self.feedHeaderViewController updateForSelectedFeed];
  self.feedMetricsRecorder.followDelegate = self;
}

- (void)updateModuleVisibility {
  [self handleChangeInModules];
  [self cancelOmniboxEdit];
  [self setContentOffsetToTop];
  [self.feedHeaderViewController updateForFeedVisibilityChanged];
}

#pragma mark - NewTabPageDelegate

- (void)updateFeedLayout {
  // If this coordinator has not finished [self start], the below will start
  // viewDidLoad before the UI is ready, failing DCHECKS.
  if (!self.started) {
    return;
  }
  // TODO(crbug.com/40252945): Investigate why this order is correct. Intuition
  // would be that the layout update should happen before telling UIKit to
  // relayout.
  [self.containedViewController.view setNeedsLayout];
  [self.containedViewController.view layoutIfNeeded];
  [self.NTPViewController updateNTPLayout];
}

- (void)setContentOffsetToTop {
  [self.NTPViewController setContentOffsetToTop];
}

- (BOOL)isGoogleDefaultSearchEngine {
  return search::DefaultSearchProviderIsGoogle(self.templateURLService);
}

- (BOOL)isStartSurface {
  // The web state is nil if the NTP is in another tab. In this case, it is
  // never a start surface.
  if (!self.webState) {
    return NO;
  }
  NewTabPageTabHelper* NTPHelper =
      NewTabPageTabHelper::FromWebState(self.webState);
  return NTPHelper && NTPHelper->ShouldShowStartSurface();
}

- (void)handleFeedTopSectionClosed {
  [self.NTPViewController updateScrollPositionForFeedTopSectionClosed];
}

- (BOOL)isSignInAllowed {
  AuthenticationService::ServiceStatus statusService =
      self.authService->GetServiceStatus();
  switch (statusService) {
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy:
    case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
    case AuthenticationService::ServiceStatus::SigninDisabledByUser: {
      return NO;
    }
    case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
    case AuthenticationService::ServiceStatus::SigninAllowed: {
      break;
    }
  }
  return YES;
}

#pragma mark - NewTabPageFollowDelegate

- (NSUInteger)followedPublisherCount {
  return self.followedWebSites.count;
}

- (BOOL)doesFollowingFeedHaveContent {
  for (FollowedWebSite* web_site in self.followedWebSites) {
    if (web_site.state == FollowedWebSiteStateStateActive) {
      return YES;
    }
  }

  return NO;
}

- (NSArray<FollowedWebSite*>*)followedWebSites {
  FollowBrowserAgent* followBrowserAgent =
      FollowBrowserAgent::FromBrowser(self.browser);

  // Return an empty list if the BrowserAgent is null (which can happen
  // if e.g. the Browser is off-the-record).
  if (!followBrowserAgent)
    return @[];

  return followBrowserAgent->GetFollowedWebSites();
}

#pragma mark - NewTabPageMetricsDelegate

- (void)recentTabTileOpenedAtIndex:(NSUInteger)index {
  RecordMagicStackClick(ContentSuggestionsModuleType::kTabResumption,
                        [self isStartSurface]);
  RecordHomeAction(IOSHomeActionType::kReturnToRecentTab,
                   [self isStartSurface]);
  RecordMagicStackTabResumptionClick(true, [self isStartSurface], index);
}

- (void)distantTabResumptionOpenedAtIndex:(NSUInteger)index {
  RecordMagicStackClick(ContentSuggestionsModuleType::kTabResumption,
                        [self isStartSurface]);
  RecordHomeAction(IOSHomeActionType::kOpenDistantTabResumption,
                   [self isStartSurface]);
  RecordMagicStackTabResumptionClick(false, [self isStartSurface], index);
}

- (void)recentTabTileDisplayedAtIndex:(NSUInteger)index {
  LogTabResumptionImpression(true, [self isStartSurface], index);
}

- (void)distantTabResumptionDisplayedAtIndex:(NSUInteger)index {
  LogTabResumptionImpression(false, [self isStartSurface], index);
}

- (void)feedArticleOpened {
  RecordHomeAction(IOSHomeActionType::kFeedCard, [self isStartSurface]);
}

- (void)mostVisitedTileOpened {
  RecordHomeAction(IOSHomeActionType::kMostVisitedTile, [self isStartSurface]);
}

- (void)shortcutTileOpened {
  RecordMagicStackClick(ContentSuggestionsModuleType::kShortcuts,
                        [self isStartSurface]);
  RecordHomeAction(IOSHomeActionType::kShortcuts, [self isStartSurface]);
}

- (void)setUpListItemOpened {
  RecordHomeAction(IOSHomeActionType::kSetUpList, [self isStartSurface]);
}

- (void)safetyCheckOpened {
  RecordMagicStackClick(ContentSuggestionsModuleType::kSafetyCheck,
                        [self isStartSurface]);
  RecordHomeAction(IOSHomeActionType::kSafetyCheck, [self isStartSurface]);
}

- (void)parcelTrackingOpened {
  RecordMagicStackClick(ContentSuggestionsModuleType::kParcelTracking,
                        [self isStartSurface]);
  RecordHomeAction(IOSHomeActionType::kParcelTracking, [self isStartSurface]);
}

#pragma mark - OverscrollActionsControllerDelegate

- (void)overscrollActionNewTab:(OverscrollActionsController*)controller {
  id<ApplicationCommands> applicationCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [applicationCommandsHandler openURLInNewTab:[OpenNewTabCommand command]];
  [self.NTPMetricsRecorder
      recordOverscrollActionForType:OverscrollActionType::kOpenedNewTab];
}

- (void)overscrollActionCloseTab:(OverscrollActionsController*)controller {
  id<BrowserCoordinatorCommands> browserCoordinatorCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(),
                         BrowserCoordinatorCommands);
  [browserCoordinatorCommandsHandler closeCurrentTab];
  [self.NTPMetricsRecorder
      recordOverscrollActionForType:OverscrollActionType::kCloseTab];
}

- (void)overscrollActionRefresh:(OverscrollActionsController*)controller {
  [self reload];
  [self.NTPMetricsRecorder
      recordOverscrollActionForType:OverscrollActionType::kPullToRefresh];
}

- (BOOL)shouldAllowOverscrollActionsForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return YES;
}

- (UIView*)toolbarSnapshotViewForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return nil;
}

- (UIView*)headerViewForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return self.feedWrapperViewController.view;
}

- (CGFloat)headerInsetForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return [self.NTPViewController heightAboveFeed];
}

- (CGFloat)headerHeightForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  CGFloat height = [self.headerViewController toolBarView].bounds.size.height;
  CGFloat topInset = self.feedWrapperViewController.view.safeAreaInsets.top;
  return height + topInset;
}

- (CGFloat)initialContentOffsetForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return -[self headerInsetForOverscrollActionsController:controller];
}

- (FullscreenController*)fullscreenControllerForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  // Fullscreen isn't supported here.
  return nullptr;
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(InitStage)previousInitStage {
  if (previousInitStage == InitStageFirstRun) {
    self.NTPViewController.focusAccessibilityOmniboxWhenViewAppears = YES;
    [self.headerViewController focusAccessibilityOnOmnibox];

    [appState removeObserver:self];
  }
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  // Observes changes in feed visibility pref.
  [self updateModuleVisibility];
}

#pragma mark - DiscoverFeedObserverBridge

- (void)discoverFeedModelWasCreated {
  if (self.NTPViewController.viewDidAppear) {
    [self handleChangeInModules];

    if (IsWebChannelsEnabled()) {
      [self.feedHeaderViewController updateForFollowingFeedVisibilityChanged];
      [self updateFeedLayout];
    }
    [self.NTPViewController setContentOffsetToTop];
  }
}

#pragma mark - IdentityManagerObserverBridgeDelegate

// TODO(crbug.com/346756363): Remove this method as it is replaced with
// `onIsSubjectToParentalControlsCapabilityChanged`.
- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  // An account change may trigger after the coordinator has been stopped.
  // In this case do not process the event.
  if (!self.started) {
    return;
  }
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
    case signin::PrimaryAccountChangeEvent::Type::kCleared: {
      [self.contentSuggestionsCoordinator refresh];
      [self updateFeedVisibilityForSupervision];
      break;
    }
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

#pragma mark - AuthenticationServiceObserving

- (void)onServiceStatusChanged {
  switch (self.authService->GetServiceStatus()) {
    case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
    case AuthenticationService::ServiceStatus::SigninAllowed:
      break;
    case AuthenticationService::ServiceStatus::SigninDisabledByUser:
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy:
    case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
      // If sign-in becomes disabled, the sign-in promo must be disabled too.
      // TODO(crbug.com/40280872): The sign-in promo should just be hidden
      // instead of resetting the hierarchy.
      [self handleChangeInModules];
      [self setContentOffsetToTop];
  }
}

#pragma mark - SupervisedUserCapabilitiesObserving

- (void)onIsSubjectToParentalControlsCapabilityChanged:
    (supervised_user::CapabilityUpdateState)capabilityUpdateState {
  if (base::FeatureList::IsEnabled(
          supervised_user::
              kReplaceSupervisionSystemCapabilitiesWithAccountCapabilitiesOnIOS)) {
    BOOL isSubjectToParentalControl =
        (capabilityUpdateState ==
         supervised_user::CapabilityUpdateState::kSetToTrue);
    [self updateFeedWithIsSupervisedUser:isSubjectToParentalControl];
  }
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  // `SceneActivationLevelForegroundInactive` is called both when foregrounding
  // and backgrounding, and is thus not used as an indicator to trigger
  // visibility.
  if (self.webState && !self.visible &&
      level == SceneActivationLevelForegroundActive) {
    [self updateNTPIsVisible:YES];
  } else if (self.visible && level < SceneActivationLevelForegroundInactive) {
    [self updateNTPIsVisible:NO];
  }
}

#pragma mark - Private

// Stops the account switcher.
- (void)stopAccountMenuCoordinator {
  [_accountMenuCoordinator stop];
  _accountMenuCoordinator.delegate = nil;
  _accountMenuCoordinator = nil;
}

// Updates the feed visibility or content based on the supervision state
// of the account defined in `value`.
- (void)updateFeedWithIsSupervisedUser:(BOOL)value {
  // This may be called asynchronously after the NTP has
  // been stopped and the object has been stopped. Ignore
  // the invocation.
  PrefService* prefService = self.prefService;
  if (!prefService) {
    return;
  }

  prefService->SetBoolean(prefs::kNTPContentSuggestionsForSupervisedUserEnabled,
                          !value);
}

- (void)updateStartForVisibilityChange:(BOOL)visible {
  if (visible && NewTabPageTabHelper::FromWebState(self.webState)
                     ->ShouldShowStartSurface()) {
    DiscoverFeedServiceFactory::GetForBrowserState(
        self.browser->GetBrowserState())
        ->SetIsShownOnStartSurface(true);
  }
  if (!visible && NewTabPageTabHelper::FromWebState(self.webState)
                      ->ShouldShowStartSurface()) {
    // This means the NTP going away was showing Start. Reset configuration
    // since it should not show Start after disappearing.
    NewTabPageTabHelper::FromWebState(self.webState)
        ->SetShowStartSurface(false);
  }
}

// Updates the NTP to take into account a change in module visibility
- (void)handleChangeInModules {
  DCHECK(self.NTPViewController);

  [self.NTPViewController resetViewHierarchy];

  if (self.feedViewController) {
    self.discoverFeedService->RemoveFeedViewController(self.feedViewController);
  }

  [self.feedTopSectionCoordinator stop];

  self.NTPViewController.feedWrapperViewController = nil;
  self.NTPViewController.feedTopSectionViewController = nil;
  self.feedWrapperViewController = nil;
  self.feedViewController = nil;
  self.feedTopSectionCoordinator = nil;

  // Fetches feed header and conditionally fetches feed. Feed can only be
  // visible if feed header is visible.
  if (self.NTPMediator.feedHeaderVisible) {
    [self configureFeedAndHeader];
  } else {
    self.NTPViewController.feedHeaderViewController = nil;
    self.feedHeaderViewController = nil;
  }

  if ([self isFeedVisible]) {
    self.NTPViewController.feedTopSectionViewController =
        self.feedTopSectionCoordinator.viewController;
  }

  self.NTPViewController.feedVisible = [self isFeedVisible];

  self.feedWrapperViewController = [self.componentFactory
      feedWrapperViewControllerWithDelegate:self
                         feedViewController:self.feedViewController];

  self.NTPViewController.feedWrapperViewController =
      self.feedWrapperViewController;

  [self.NTPViewController layoutContentInParentCollectionView];

  [self updateFeedLayout];
}

// Returns `YES` if the feed is currently visible on the NTP.
- (BOOL)isFeedVisible {
  return [self shouldFeedBeVisible] && self.feedViewController;
}

// Creates, configures and returns a feed view controller configuration.
- (DiscoverFeedViewControllerConfiguration*)feedViewControllerConfiguration {
  DiscoverFeedViewControllerConfiguration* viewControllerConfig =
      [[DiscoverFeedViewControllerConfiguration alloc] init];
  viewControllerConfig.browser = self.browser;
  viewControllerConfig.scrollDelegate = self.NTPViewController;
  viewControllerConfig.previewDelegate = self;
  viewControllerConfig.manageDelegate = self;
  viewControllerConfig.signInPromoDelegate = self;

  return viewControllerConfig;
}

// Updates the visibility of the content suggestions on the NTP if the account
// is subject to parental controls.
// TODO(crbug.com/346756363): Remove this method as we deprecate getting
// supervision status from SystemIdentityManager.
- (void)updateFeedVisibilityForSupervision {
  if (!base::FeatureList::IsEnabled(
          supervised_user::
              kReplaceSupervisionSystemCapabilitiesWithAccountCapabilitiesOnIOS)) {
    DCHECK(self.prefService);
    DCHECK(self.authService);

    id<SystemIdentity> identity =
        self.authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
    if (!identity) {
      [self updateFeedWithIsSupervisedUser:NO];
      return;
    }

    using CapabilityResult = SystemIdentityCapabilityResult;

    __weak NewTabPageCoordinator* weakSelf = self;
    GetApplicationContext()
        ->GetSystemIdentityManager()
        ->IsSubjectToParentalControls(
            identity, base::BindOnce(^(CapabilityResult result) {
              const bool isSupervisedUser = result == CapabilityResult::kTrue;
              [weakSelf updateFeedWithIsSupervisedUser:isSupervisedUser];
            }));
  }
}

// Toggles feed visibility between hidden or expanded using the feed header
// menu. A hidden feed will continue to show the header, with a modified label.
// TODO(crbug.com/1304382): Modify this comment when Web Channels is launched.
- (void)setFeedVisibleFromHeader:(BOOL)visible {
  [self.feedExpandedPref setValue:visible];
  [self.feedMetricsRecorder recordDiscoverFeedVisibilityChanged:visible];
  [self updateModuleVisibility];
}

// Configures and returns the feed top section coordinator.
- (FeedTopSectionCoordinator*)createFeedTopSectionCoordinator {
  DCHECK(self.NTPViewController);
  FeedTopSectionCoordinator* feedTopSectionCoordinator =
      [[FeedTopSectionCoordinator alloc]
          initWithBaseViewController:self.NTPViewController
                             browser:self.browser];
  feedTopSectionCoordinator.NTPDelegate = self;
  [feedTopSectionCoordinator start];
  return feedTopSectionCoordinator;
}

// Handles the feed management button being tapped.
- (void)handleFeedManageTapped {
  [self.feedMetricsRecorder recordHeaderMenuManageTapped];
  [self.feedManagementCoordinator stop];
  self.feedManagementCoordinator = nil;

  self.feedManagementCoordinator = [[FeedManagementCoordinator alloc]
      initWithBaseViewController:self.NTPViewController
                         browser:self.browser];
  self.feedManagementCoordinator.navigationDelegate = self.NTPMediator;
  self.feedManagementCoordinator.feedMetricsRecorder = self.feedMetricsRecorder;
  [self.feedManagementCoordinator start];
}

// Private setter for the `webState` property.
- (void)setWebState:(web::WebState*)webState {
  if (_webState == webState) {
    return;
  }

  _webState = webState;
  self.contentSuggestionsCoordinator.webState = _webState;
  [self.logoVendor setWebState:_webState];
}

// Called when the NTP changes visibility, either when the user navigates to
// or away from the NTP, or when the active WebState changes.
- (void)updateNTPIsVisible:(BOOL)visible {
  if (visible == self.visible) {
    return;
  }

  CHECK(self.webState);

  self.visible = visible;
  self.NTPViewController.NTPVisible = visible;

  if (!self.browser->GetBrowserState()->IsOffTheRecord()) {
    if (visible) {
      self.didAppearTime = base::TimeTicks::Now();
      if (self.NTPMediator.feedHeaderVisible) {
        if ([self.feedExpandedPref value]) {
          [self.NTPMetricsRecorder
              recordHomeImpression:IOSNTPImpressionType::kFeedVisible
                    isStartSurface:[self isStartSurface]];
        } else {
          [self.NTPMetricsRecorder
              recordHomeImpression:IOSNTPImpressionType::kFeedCollapsed
                    isStartSurface:[self isStartSurface]];
        }
      } else {
        [self.NTPMetricsRecorder
            recordHomeImpression:IOSNTPImpressionType::kFeedDisabled
                  isStartSurface:[self isStartSurface]];
      }
    } else {
      if (!self.didAppearTime.is_null()) {
        [self.NTPMetricsRecorder
            recordTimeSpentInHome:(base::TimeTicks::Now() - self.didAppearTime)
                   isStartSurface:[self isStartSurface]];
        self.didAppearTime = base::TimeTicks();
      }
    }
    // Check if feed is visible before reporting NTP visibility as the feed
    // needs to be visible in order to use for metrics.
    // TODO(crbug.com/40871863) Move isFeedVisible check to the metrics recorder
    if ([self isFeedVisible]) {
      [self.feedMetricsRecorder recordNTPDidChangeVisibility:visible];
    }
  }
}

// Returns whether the user policies allow them to sync.
- (BOOL)isSyncAllowedByPolicy {
  return !SyncServiceFactory::GetForBrowserState(
              self.browser->GetBrowserState())
              ->HasDisableReason(
                  syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
}

// Shows sign-in disabled snackbar message.
- (void)showSignInDisableMessage {
  id<SnackbarCommands> handler =
      static_cast<id<SnackbarCommands>>(self.browser->GetCommandDispatcher());
  MDCSnackbarMessage* message = CreateSnackbarMessage(l10n_util::GetNSString(
      IDS_IOS_NTP_FEED_SIGNIN_PROMO_DISABLE_SNACKBAR_MESSAGE));

  [handler showSnackbarMessage:message];
}

// Saves the state of the NTP associated with `self.webState`.
- (void)saveNTPState {
  [self.NTPMediator saveNTPStateForWebState:self.webState];
}

// Restores the saved state of the NTP associated with `self.webState` if
// necessary.
- (void)restoreNTPState {
  [self.NTPMediator restoreNTPStateForWebState:self.webState];
}

#pragma mark - AccountMenuCoordinatorDelegate

- (void)acountMenuCoordinatorShouldStop:(AccountMenuCoordinator*)coordinator {
  CHECK_EQ(coordinator, _accountMenuCoordinator);
  [self stopAccountMenuCoordinator];
}

#pragma mark - HomeCustomizationDelegate

- (void)handleCustomizationMenuDismissed:
    (HomeCustomizationCoordinator*)coordinator {
  [_customizationCoordinator stop];
  _customizationCoordinator = nil;
}

#pragma mark - VIVALDI
- (void)configureVivaldiNTPViewController {
  UINavigationController *speedDialHomeController =
      _speedDialHomeCoordinator.navigationController;
  [speedDialHomeController
      willMoveToParentViewController:self.containerViewController];
  [self.containerViewController addChildViewController:speedDialHomeController];
  [self.containerViewController.view addSubview:speedDialHomeController.view];
  [speedDialHomeController
      didMoveToParentViewController:self.containerViewController];
  [speedDialHomeController.view fillSuperview];
  self.containedViewController = speedDialHomeController;
}
// End Vivaldi

@end
