 // Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/toolbar_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_coordinator.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position_browser_agent.h"
#import "ios/chrome/browser/orchestrator/ui_bundled/omnibox_focus_orchestrator.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presentation_context.h"
#import "ios/chrome/browser/prerender/model/prerender_service.h"
#import "ios/chrome/browser/prerender/model/prerender_service_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/find_in_page_commands.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/text_zoom_commands.h"
#import "ios/chrome/browser/shared/public/commands/toolbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view_controller_delegate.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_omnibox_consumer.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_type.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_coordinatee.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_mediator.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/components/webui/web_ui_url_constants.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_steady_view_consumer.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/tabs/ui_bundled/tab_strip_constants.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_view.h"
#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_view_controller.h"
#import "ios/ui/ad_tracker_blocker/manager/vivaldi_atb_manager.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/toolbar/vivaldi_toolbar_constants.h"

using vivaldi::IsVivaldiRunning;
// End Vivaldi

@interface ToolbarCoordinator () <PrimaryToolbarViewControllerDelegate,
                                  ToolbarCommands,

                                  // Vivaldi
                                  LocationBarSteadyViewConsumer,
                                  VivaldiATBConsumer,
                                  // End Vivaldi

                                  ToolbarMediatorDelegate> {
  raw_ptr<PrerenderService> _prerenderService;
}

/// Whether this coordinator has been started.
@property(nonatomic, assign) BOOL started;
/// Coordinator for the location bar containing the omnibox.
@property(nonatomic, strong) LocationBarCoordinator* locationBarCoordinator;
/// Coordinator for the primary toolbar at the top of the screen.
@property(nonatomic, strong)
    PrimaryToolbarCoordinator* primaryToolbarCoordinator;
/// Coordinator for the secondary toolbar at the bottom of the screen.
@property(nonatomic, strong)
    SecondaryToolbarCoordinator* secondaryToolbarCoordinator;

/// Mediator observing WebStateList for toolbars.
@property(nonatomic, strong) ToolbarMediator* toolbarMediator;
/// Orchestrator for the omnibox focus animation.
@property(nonatomic, strong) OmniboxFocusOrchestrator* orchestrator;
/// Whether the omnibox is currently focused.
@property(nonatomic, assign) BOOL locationBarFocused;

// Vivaldi
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

// Important!!!: (prio@vivaldi.com) - Note to self or someone visiting this part
// Chrome moves only omnibox to the bottom, but for us we have buttons
// (shield, menu, panels) alongside omnibox to move to bottom when bottom
// omnibox is enabled, which are part of primary toolbar.
// Therefore, we create a separate view with this
// coordinator to use that as top primary toolbar. And, use chrome's whole
// primary toolbar as bottom omnibox with buttons, and not just the omnibox part.
@property(nonatomic, strong)
    PrimaryToolbarCoordinator* vivaldiTopToolbarCoordinator;
// Manager to keep track of adblocker shield state for the web state.
@property(nonatomic, strong) VivaldiATBManager* adblockManager;
// End Vivaldi

@end

@implementation ToolbarCoordinator {
  /// Type of toolbar containing the omnibox. Unlike
  /// `_steadyStateOmniboxPosition`, this tracks the omnibox position at all
  /// time.
  ToolbarType _omniboxPosition;
  /// Type of the toolbar that contains the omnibox when it's not focused. The
  /// animation of focusing/defocusing the omnibox changes depending on this
  /// position.
  ToolbarType _steadyStateOmniboxPosition;
  /// Whether the omnibox focusing should happen with animation.
  BOOL _enableAnimationsForOmniboxFocus;
  //// Indicates whether the focus came from a tap on the NTP's fakebox.
  BOOL _focusedFromFakebox;
  /// Indicates whether the fakebox was pinned on last signal to focus from
  /// the fakebox.
  BOOL _fakeboxPinned;
  /// Command handler for showing the IPH.
  id<HelpCommands> _helpHandler;

  // Vivaldi
  // Pref backed boolean to track tab bar style state.
  BOOL _tabBarEnabled;
  // End Vivaldi

}

- (instancetype)initWithBrowser:(Browser*)browser {
  CHECK(browser);
  self = [super initWithBaseViewController:nil browser:browser];
  if (self) {
    // Initialize both coordinators here as they might be referenced before
    // `start`.
    _primaryToolbarCoordinator =
        [[PrimaryToolbarCoordinator alloc] initWithBrowser:browser];
    _secondaryToolbarCoordinator =
        [[SecondaryToolbarCoordinator alloc] initWithBrowser:browser];

    // Vivaldi
    _vivaldiTopToolbarCoordinator =
        [[PrimaryToolbarCoordinator alloc] initWithBrowser:browser];
    // End Vivaldi

    [self.browser->GetCommandDispatcher()
        startDispatchingToTarget:self
                     forProtocol:@protocol(ToolbarCommands)];

    _helpHandler =
        HandlerForProtocol(browser->GetCommandDispatcher(), HelpCommands);
  }
  return self;
}

- (void)start {
  if (self.started) {
    return;
  }
  _enableAnimationsForOmniboxFocus = YES;
  // Set a default position, overriden by `setInitialOmniboxPosition` below.
  _omniboxPosition = ToolbarType::kPrimary;

  Browser* browser = self.browser;
  [browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(FakeboxFocuser)];

  segmentation_platform::DeviceSwitcherResultDispatcher* deviceSwitcherResult =
      nullptr;
  if (!browser->GetProfile()->IsOffTheRecord()) {
    deviceSwitcherResult =
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetDispatcherForProfile(browser->GetProfile());
  }
  self.toolbarMediator = [[ToolbarMediator alloc]
      initWithWebStateList:browser->GetWebStateList()
               isIncognito:browser->GetProfile()->IsOffTheRecord()];
  self.toolbarMediator.delegate = self;
  self.toolbarMediator.deviceSwitcherResultDispatcher = deviceSwitcherResult;

  // Vivaldi
  PrefService* originalPrefs =
      browser->GetProfile()->GetOriginalProfile()->GetPrefs();
  self.toolbarMediator.originalPrefService = originalPrefs;
  // End Vivaldi

  self.locationBarCoordinator =
      [[LocationBarCoordinator alloc] initWithBrowser:browser];
  self.locationBarCoordinator.delegate = self.omniboxFocusDelegate;
  self.locationBarCoordinator.bubblePresenter = self.bubblePresenter;
  self.locationBarCoordinator.popupPresenterDelegate =
      self.popupPresenterDelegate;

  // Vivaldi
  self.locationBarCoordinator.steadyViewConsumer = self;
  // End Vivaldi

  [self.locationBarCoordinator start];
  self.toolbarMediator.omniboxConsumer =
      self.locationBarCoordinator.toolbarOmniboxConsumer;

  self.primaryToolbarCoordinator.viewControllerDelegate = self;
  self.primaryToolbarCoordinator.toolbarHeightDelegate =
      self.toolbarHeightDelegate;
  [self.primaryToolbarCoordinator start];
  self.secondaryToolbarCoordinator.toolbarHeightDelegate =
      self.toolbarHeightDelegate;
  [self.secondaryToolbarCoordinator start];

  // Vivaldi
  self.vivaldiTopToolbarCoordinator.viewControllerDelegate = self;
  [self.vivaldiTopToolbarCoordinator start];

  LayoutGuideCenter* layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);
  _layoutGuideCenter = layoutGuideCenter;
  [_layoutGuideCenter referenceView:
      self.secondaryToolbarCoordinator.viewController.view
                          underName:vivaldiBottomOmniboxGuide];
  [self initialiseAdblockManager];
  // End Vivaldi

  self.orchestrator = [[OmniboxFocusOrchestrator alloc] init];

  // Important:(prio@vivaldi.com) - Animatee is set below with config. Setting
  // this incorrectly breaks primary toolbar position.
  if (!IsVivaldiRunning()) {
  self.orchestrator.toolbarAnimatee =
      self.primaryToolbarCoordinator.toolbarAnimatee;
  } // End Vivaldi

  self.orchestrator.locationBarAnimatee =
      [self.locationBarCoordinator locationBarAnimatee];
  self.orchestrator.editViewAnimatee =
      [self.locationBarCoordinator editViewAnimatee];

  if (IsBottomOmniboxAvailable()) {
    [self.toolbarMediator setInitialOmniboxPosition];
  } else {
    [self.primaryToolbarCoordinator
        setLocationBarViewController:self.locationBarCoordinator
                                         .locationBarViewController];
  }

  [self updateToolbarsLayout];
  _prerenderService =
      PrerenderServiceFactory::GetForProfile(self.browser->GetProfile());

  [super start];
  self.started = YES;
}

- (void)stop {
  if (!self.started) {
    return;
  }
  [super stop];
  _prerenderService = nullptr;
  self.orchestrator.editViewAnimatee = nil;
  self.orchestrator.locationBarAnimatee = nil;
  self.orchestrator = nil;

  [self.primaryToolbarCoordinator stop];
  self.primaryToolbarCoordinator.viewControllerDelegate = nil;
  self.primaryToolbarCoordinator = nil;

  [self.secondaryToolbarCoordinator stop];
  self.secondaryToolbarCoordinator = nil;

  [self.locationBarCoordinator stop];
  self.locationBarCoordinator.popupPresenterDelegate = nil;
  self.locationBarCoordinator = nil;

  [self.toolbarMediator disconnect];
  self.toolbarMediator.omniboxConsumer = nil;
  self.toolbarMediator.delegate = nil;
  self.toolbarMediator.deviceSwitcherResultDispatcher = nullptr;
  self.toolbarMediator = nil;

  // Vivaldi
  [self.vivaldiTopToolbarCoordinator stop];
  self.vivaldiTopToolbarCoordinator.viewControllerDelegate = nil;
  self.vivaldiTopToolbarCoordinator = nil;
  self.locationBarCoordinator.steadyViewConsumer = nil;
  if (self.adblockManager) {
    self.adblockManager.consumer = nil;
    [self.adblockManager disconnect];
  }
  // End Vivaldi

  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  _prerenderService = nullptr;
  self.started = NO;
}

#pragma mark - Public

- (UIViewController*)primaryToolbarViewController {

  if (IsVivaldiRunning()) {
    return self.vivaldiTopToolbarCoordinator.viewController;
  } // End Vivaldi

  return self.primaryToolbarCoordinator.viewController;
}

- (UIViewController*)secondaryToolbarViewController {
  return self.secondaryToolbarCoordinator.viewController;
}

- (id<SharingPositioner>)sharingPositioner {

  // Vivaldi: We will return location bar here since share button is within
  // location bar for us.
  if (IsVivaldiRunning())
      return [self.locationBarCoordinator vivaldiPositioner];
  // End Vivaldi

  return self.primaryToolbarCoordinator.SharingPositioner;
}

// Public and in `ToolbarMediatorDelegate`.
- (void)updateToolbar {
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (!webState) {
    return;
  }

  // Please note, this notion of isLoading is slightly different from WebState's
  // IsLoading().
  BOOL isToolbarLoading =
      webState->IsLoading() &&
      !webState->GetLastCommittedURL().SchemeIs(kChromeUIScheme);

  if (self.isLoadingPrerenderer && isToolbarLoading) {
    for (id<ToolbarCoordinatee> coordinator in self.coordinators) {
      [coordinator showPrerenderingAnimation];
    }
  }

  id<FindInPageCommands> findInPageCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), FindInPageCommands);
  [findInPageCommandsHandler showFindUIIfActive];

  id<TextZoomCommands> textZoomCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), TextZoomCommands);
  [textZoomCommandsHandler showTextZoomUIIfActive];

  // There are times when the NTP can be hidden but before the visibleURL
  // changes.  This can leave the BVC in a blank state where only the bottom
  // toolbar is visible. Instead, if possible, use the NewTabPageTabHelper
  // IsActive() value rather than checking -IsVisibleURLNewTabPage.
  NewTabPageTabHelper* NTPHelper = NewTabPageTabHelper::FromWebState(webState);
  BOOL isNTP = NTPHelper && NTPHelper->IsActive();
  BOOL isOffTheRecord = self.browser->GetProfile()->IsOffTheRecord();
  BOOL canShowTabStrip = IsRegularXRegularSizeClass(self.traitEnvironment);

  // Hide the toolbar when displaying content suggestions without the tab
  // strip, without the focused omnibox, and for UI Refresh, only when in
  // split toolbar mode.
  BOOL hideToolbar = isNTP && !isOffTheRecord &&
                     ![self isOmniboxFirstResponder] &&
                     ![self showingOmniboxPopup] && !canShowTabStrip &&
                     IsSplitToolbarMode(self.traitEnvironment);

  if (IsVivaldiRunning()) {
    self.primaryToolbarViewController.view.hidden = NO;
  } else {
  self.primaryToolbarViewController.view.hidden = hideToolbar;
  } // End Vivaldi

}

- (BOOL)isLoadingPrerenderer {
  return _prerenderService && _prerenderService->IsLoadingPrerender();
}

#pragma mark Omnibox and LocationBar

- (void)transitionToLocationBarFocusedState:(BOOL)focused
                                 completion:(ProceduralBlock)completion {
  // Disable infobarBanner overlays when focusing the omnibox as they overlap
  // with primary toolbar.
  OverlayPresentationContext* infobarBannerContext =
      OverlayPresentationContext::FromBrowser(self.browser,
                                              OverlayModality::kInfobarBanner);
  if (infobarBannerContext) {
    infobarBannerContext->SetUIDisabled(focused);
  }

  if (self.traitEnvironment.traitCollection.verticalSizeClass ==
      UIUserInterfaceSizeClassUnspecified) {
    return;
  }
  [self.toolbarMediator locationBarFocusChangedTo:focused];

  // Disable toolbar animations when focusing the omnibox on secondary toolbar.
  // TODO(crbug.com/40275116): Add animation in OmniboxFocusOrchestrator if
  // needed.
  BOOL animateTransition = _enableAnimationsForOmniboxFocus &&
                           _steadyStateOmniboxPosition == ToolbarType::kPrimary;

  if (IsVivaldiRunning())
    animateTransition = _enableAnimationsForOmniboxFocus; // End Vivaldi

  __weak __typeof(self) weakSelf = self;
  BOOL toolbarExpanded =
      focused && !IsRegularXRegularSizeClass(self.traitEnvironment);

  // Vivaldi
  [self updateToolbarBackgroundColorWithOmniboxFocus:focused];
  // End Vivaldi

  [self.orchestrator
      transitionToStateOmniboxFocused:focused
                      toolbarExpanded:toolbarExpanded
                              trigger:[self omniboxFocusTrigger]
                             animated:animateTransition
                           completion:^{
                             [weakSelf focusTransitionDidComplete:focused
                                                       completion:completion];
                           }];

  self.locationBarFocused = focused;
}

- (BOOL)isOmniboxFirstResponder {
  return [self.locationBarCoordinator isOmniboxFirstResponder];
}

- (BOOL)showingOmniboxPopup {
  return [self.locationBarCoordinator showingOmniboxPopup];
}

#pragma mark ToolbarHeightProviding

- (CGFloat)collapsedPrimaryToolbarHeight {
  if (_omniboxPosition == ToolbarType::kSecondary) {
    // TODO(crbug.com/40279063): Find out why primary toolbar height cannot be
    // zero. This is a temporary fix for the pdf bug.

    if (IsVivaldiRunning()) {
      return 0.0;
    } // End Vivaldi

    return 1.0;
  }

  return ToolbarCollapsedHeight(
      self.traitEnvironment.traitCollection.preferredContentSizeCategory);
}

- (CGFloat)expandedPrimaryToolbarHeight {
  CGFloat height =
      self.primaryToolbarViewController.view.intrinsicContentSize.height;
  if (!IsSplitToolbarMode(self.traitEnvironment)) {

    // Note: (prio@vivaldi.com) - Add the margin when omnibox is at the top.
    // otherwise, it shows a gap between content view and status bar.
    if (IsVivaldiRunning() &&
        _omniboxPosition == ToolbarType::kPrimary) {
    // When the adaptive toolbar is unsplit, add a margin.
    height += kTopToolbarUnsplitMargin;
    } // End Vivaldi

  }
  return height;
}

- (CGFloat)collapsedSecondaryToolbarHeight {
  if (_omniboxPosition == ToolbarType::kSecondary) {
    return ToolbarCollapsedHeight(
        self.traitEnvironment.traitCollection.preferredContentSizeCategory);
  }
  return 0.0;
}

- (CGFloat)expandedSecondaryToolbarHeight {
  if (!IsSplitToolbarMode(self.traitEnvironment)) {

    // Important(prio@vivaldi.com) - This enables the bottom omnibox area for
    // iPads and iPhone landscape.
    if (_omniboxPosition == ToolbarType::kSecondary) {
      CGFloat height =
          self.secondaryToolbarViewController.view.intrinsicContentSize.height;
      if (_tabBarEnabled || [VivaldiGlobalHelpers isDeviceTablet]) {
        CHECK(IsBottomOmniboxAvailable());
        height += ToolbarExpandedHeight(
            self.traitEnvironment.traitCollection.preferredContentSizeCategory);
      } else {
        height += vBottomAdaptiveLocationBarTopMargin;
      }
      return height;
    }
    // End Vivaldi

    return 0.0;
  }
  CGFloat height =
      self.secondaryToolbarViewController.view.intrinsicContentSize.height;
  if (_omniboxPosition == ToolbarType::kSecondary) {
    height += ToolbarExpandedHeight(
        self.traitEnvironment.traitCollection.preferredContentSizeCategory);
  }
  return height;
}

#pragma mark - FakeboxFocuser

- (void)focusOmniboxNoAnimation {
  _enableAnimationsForOmniboxFocus = NO;
  [self.locationBarCoordinator focusOmniboxFromFakebox];
  _enableAnimationsForOmniboxFocus = YES;
  // If the pasteboard is containing a URL, the omnibox popup suggestions are
  // displayed as soon as the omnibox is focused.
  // If the fake omnibox animation is triggered at the same time, it is possible
  // to see the NTP going up where the real omnibox should be displayed.
  if ([self.locationBarCoordinator omniboxPopupHasAutocompleteResults]) {
    [self onFakeboxAnimationComplete];
  }
}

- (void)focusOmniboxFromFakebox:(BOOL)fromFakebox pinned:(BOOL)pinned {
  _focusedFromFakebox = fromFakebox;
  _fakeboxPinned = pinned;
  [self.locationBarCoordinator focusOmniboxFromFakebox];
}

- (void)onFakeboxBlur {

  if (IsVivaldiRunning()) {
    self.primaryToolbarViewController.view.hidden = NO;
  } else {
  // Hide the toolbar if the NTP is currently displayed.
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (webState && IsVisibleURLNewTabPage(webState)) {
    self.primaryToolbarViewController.view.hidden =
        IsSplitToolbarMode(self.traitEnvironment);
  }
  } // End Vivaldi

}

- (void)onFakeboxAnimationComplete {
  self.primaryToolbarViewController.view.hidden = NO;
}

#pragma mark - NewTabPageControllerDelegate

- (void)setScrollProgressForTabletOmnibox:(CGFloat)progress {
  for (id<NewTabPageControllerDelegate> coordinator in self.coordinators) {
    [coordinator setScrollProgressForTabletOmnibox:progress];
  }
}

- (UIResponder<UITextInput>*)fakeboxScribbleForwardingTarget {
  return self.locationBarCoordinator.omniboxScribbleForwardingTarget;
}

- (void)didNavigateToNTPOnActiveWebState {
  [self.toolbarMediator didNavigateToNTPOnActiveWebState];
}

#pragma mark - PopupMenuUIUpdating

- (void)updateUIForOverflowMenuIPHDisplayed {
  for (id<ToolbarCoordinatee> coordinator in self.coordinators) {
    [coordinator.popupMenuUIUpdater updateUIForOverflowMenuIPHDisplayed];
  }
}

- (void)updateUIForIPHDismissed {
  for (id<ToolbarCoordinatee> coordinator in self.coordinators) {
    [coordinator.popupMenuUIUpdater updateUIForIPHDismissed];
  }
}

- (void)setOverflowMenuBlueDot:(BOOL)hasBlueDot {
  for (id<ToolbarCoordinatee> coordinator in self.coordinators) {
    [coordinator.popupMenuUIUpdater setOverflowMenuBlueDot:hasBlueDot];
  }
}

#pragma mark - PrimaryToolbarViewControllerDelegate

- (void)viewControllerTraitCollectionDidChange:
    (UITraitCollection*)previousTraitCollection {
  if (!_started) {
    return;
  }
  [self updateToolbarsLayout];
}

- (void)close {
  if (self.locationBarFocused) {
    id<ApplicationCommands> applicationCommandsHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), ApplicationCommands);
    [applicationCommandsHandler dismissModalDialogsWithCompletion:nil];
  }
}

#pragma mark - SideSwipeToolbarInteracting

- (BOOL)isInsideToolbar:(CGPoint)point {
  for (id<ToolbarCoordinatee> coordinator in self.coordinators) {
    // The toolbar frame is inset by -1 because CGRectContainsPoint does
    // include points on the max X and Y edges, which will happen frequently
    // with edge swipes from the right side.
    CGRect toolbarFrame =
        CGRectInset([coordinator viewController].view.bounds, -1, -1);

    // Important(prio@vivaldi.com) - When tab bar is enabled we have to deduct
    // tab bar height and bottom safe area height from the toolbar view.
    // Otherwise, swiping on tab bar conflicts with the omnibox swipe gesture.
    if (_omniboxPosition == ToolbarType::kSecondary &&
        _tabBarEnabled && coordinator == self.secondaryToolbarCoordinator) {
      toolbarFrame.size.height =
          toolbarFrame.size.height - kTabStripHeight -
            VivaldiGlobalHelpers.safeAreaInsets.bottom;
    }
    // End Vivaldi

    CGPoint pointInToolbarCoordinates =
        [[coordinator viewController].view convertPoint:point fromView:nil];
    if (CGRectContainsPoint(toolbarFrame, pointInToolbarCoordinates)) {
      return YES;
    }
  }
  return NO;
}

#pragma mark - SideSwipeToolbarSnapshotProviding

- (UIImage*)toolbarSideSwipeSnapshotForWebState:(web::WebState*)webState
                                withToolbarType:(ToolbarType)toolbarType {
  AdaptiveToolbarCoordinator* adaptiveToolbarCoordinator =
      [self coordinatorWithToolbarType:toolbarType];

  [adaptiveToolbarCoordinator updateToolbarForSideSwipeSnapshot:webState];
  [self updateLocationBarForSideSwipeSnapshot:webState];

  UIImage* toolbarSnapshot = CaptureViewWithOption(
      adaptiveToolbarCoordinator.viewController.view,
      [[UIScreen mainScreen] scale], kClientSideRendering);

  [adaptiveToolbarCoordinator resetToolbarAfterSideSwipeSnapshot];
  [self resetLocationBarAfterSideSwipeSnapshot];

  return toolbarSnapshot;
}

#pragma mark SideSwipeToolbarSnapshotProviding Private

/// Returns the coordinator coresponding to `toolbarType`.
- (AdaptiveToolbarCoordinator*)coordinatorWithToolbarType:
    (ToolbarType)toolbarType {
  switch (toolbarType) {
    case ToolbarType::kPrimary:

      if (IsVivaldiRunning())
        return self.vivaldiTopToolbarCoordinator; // End Vivaldi

      return self.primaryToolbarCoordinator;
    case ToolbarType::kSecondary:
      return self.secondaryToolbarCoordinator;
  }
}

/// Prepares location bar for a side swipe snapshot with`webState`.
- (void)updateLocationBarForSideSwipeSnapshot:(web::WebState*)webState {
  // Hide LocationBarView when taking a snapshot on a web state that is not the
  // active one, as the URL is not updated.
  if (webState != self.browser->GetWebStateList()->GetActiveWebState()) {
    [self.locationBarCoordinator.locationBarViewController.view setHidden:YES];
  }
}

/// Resets location bar after a side swipe snapshot.
- (void)resetLocationBarAfterSideSwipeSnapshot {
  [self.locationBarCoordinator.locationBarViewController.view setHidden:NO];
}

#pragma mark - ToolbarCommands

- (void)triggerToolbarSlideInAnimation {
  for (id<ToolbarCommands> coordinator in self.coordinators) {
    [coordinator triggerToolbarSlideInAnimation];
  }
}

#pragma mark - ToolbarMediatorDelegate

- (void)transitionOmniboxToToolbarType:(ToolbarType)toolbarType {
  _omniboxPosition = toolbarType;
  OmniboxPositionBrowserAgent* positionBrowserAgent =
      OmniboxPositionBrowserAgent::FromBrowser(self.browser);
  switch (toolbarType) {
    case ToolbarType::kPrimary:
      [self.primaryToolbarCoordinator
          setLocationBarViewController:self.locationBarCoordinator
                                           .locationBarViewController];
      [self.secondaryToolbarCoordinator setLocationBarViewController:nil];
      positionBrowserAgent->SetIsCurrentLayoutBottomOmnibox(false);
      break;
    case ToolbarType::kSecondary:
      [self.secondaryToolbarCoordinator
          setLocationBarViewController:self.locationBarCoordinator
                                           .locationBarViewController];
      [self.primaryToolbarCoordinator setLocationBarViewController:nil];
      positionBrowserAgent->SetIsCurrentLayoutBottomOmnibox(true);
      break;
  }
  [self.toolbarHeightDelegate toolbarsHeightChanged];
}

- (void)transitionSteadyStateOmniboxToToolbarType:(ToolbarType)toolbarType {
  _steadyStateOmniboxPosition = toolbarType;
}

#pragma mark - Private

/// Returns primary and secondary coordinator in a array. Helper to call method
/// on both coordinators.
- (NSArray<id<ToolbarCoordinatee>>*)coordinators {

  if (IsVivaldiRunning()) {
    return @[
      self.vivaldiTopToolbarCoordinator,
      self.secondaryToolbarCoordinator
    ];
  } // End Vivaldi

  return @[ self.primaryToolbarCoordinator, self.secondaryToolbarCoordinator ];
}

/// Returns the trait environment of the toolbars.
- (id<UITraitEnvironment>)traitEnvironment {
  return self.primaryToolbarViewController;
}

/// Updates toolbars layout whith current omnibox focus state and trait
/// collection.
- (void)updateToolbarsLayout {
  [self.toolbarMediator
      toolbarTraitCollectionChangedTo:self.traitEnvironment.traitCollection];
  BOOL omniboxFocused =
      self.isOmniboxFirstResponder || self.showingOmniboxPopup;
  [self.orchestrator
      transitionToStateOmniboxFocused:omniboxFocused
                      toolbarExpanded:omniboxFocused &&
                                      !IsRegularXRegularSizeClass(
                                          self.traitEnvironment)
                              trigger:[self omniboxFocusTrigger]
                             animated:NO
                           completion:nil];
}

/// Returns the appropriate `OmniboxFocusTrigger` depending on whether this is
/// an incognito browser, the NTP is displayed, and whether the fakebox was
/// pinned if it was selected.
- (OmniboxFocusTrigger)omniboxFocusTrigger {
  if (self.browser->GetProfile()->IsOffTheRecord() ||
      !IsSplitToolbarMode(self.traitEnvironment)) {
    return _focusedFromFakebox ? OmniboxFocusTrigger::kUnpinnedFakebox
                               : OmniboxFocusTrigger::kOther;
  }
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (!webState) {
    return OmniboxFocusTrigger::kOther;
  }
  NewTabPageTabHelper* NTPHelper = NewTabPageTabHelper::FromWebState(webState);
  if (!NTPHelper || !NTPHelper->IsActive()) {
    return OmniboxFocusTrigger::kOther;
  }
  if (IsIOSLargeFakeboxEnabled()) {
    return _fakeboxPinned ? OmniboxFocusTrigger::kPinnedLargeFakebox
                          : OmniboxFocusTrigger::kUnpinnedLargeFakebox;
  }
  return _fakeboxPinned ? OmniboxFocusTrigger::kPinnedFakebox
                        : OmniboxFocusTrigger::kUnpinnedFakebox;
}

- (void)focusTransitionDidComplete:(BOOL)focused
                        completion:(ProceduralBlock)completion {
  if (completion) {
    completion();
    completion = nil;
  }
}

#pragma mark - VIVALDI

- (void)resetToolbarAfterSideSwipeSnapshot {
  [self.locationBarCoordinator.locationBarViewController.view setHidden:NO];
}

- (PrimaryToolbarView*)primaryToolbarView {
  if (_omniboxPosition == ToolbarType::kPrimary) {
    PrimaryToolbarView* primaryView =
      (PrimaryToolbarView*)[self.primaryToolbarViewController view];
    return primaryView;
  } else {
    PrimaryToolbarView* primaryView =
      (PrimaryToolbarView*)[self.primaryToolbarCoordinator.viewController view];
    return primaryView;
  }
}

- (SecondaryToolbarView*)secondaryToolbarView {
  SecondaryToolbarView* secondaryView =
    (SecondaryToolbarView*)
      [self.secondaryToolbarCoordinator.viewController view];
  return secondaryView;
}

// Returns the toolbar button stack view for Secondary Toolbar. This is hidden
// when bottom omnibox and tab bar both enabled.
- (UIStackView*)secondaryToolbarButtonStackView {
  SecondaryToolbarViewController* viewController =
      (SecondaryToolbarViewController*)
          self.secondaryToolbarCoordinator.viewController;
  return viewController.toolbarButtonStackView;
}

// Sets the correct toolbar after settings is changed.
- (void)vivaldiTransitionOmniboxToToolbarType:(ToolbarType)toolbarType {
  [self updateProgressBarVisibilityWithToolbarType:toolbarType];
  switch (toolbarType) {
    case ToolbarType::kPrimary:
      [self.vivaldiTopToolbarCoordinator
          setLocationBarViewController:
              self.locationBarCoordinator.locationBarViewController];
      [self.secondaryToolbarCoordinator setLocationBarViewController:nil];
      [self.primaryToolbarCoordinator
          setLocationBarViewController:nil];

      if (self.vivaldiTopToolbarCoordinator.viewController) {
        self.orchestrator.toolbarAnimatee =
            self.vivaldiTopToolbarCoordinator.toolbarAnimatee;
        [self updateToolsMenuButtonGuideForController:
            self.vivaldiTopToolbarCoordinator.viewController];
      }
      break;
    case ToolbarType::kSecondary:
      [self.primaryToolbarCoordinator
          setLocationBarViewController:
              self.locationBarCoordinator.locationBarViewController];
      [self.secondaryToolbarCoordinator
          setLocationBarViewController
              :self.primaryToolbarCoordinator.viewController];
      [self.vivaldiTopToolbarCoordinator setLocationBarViewController:nil];

      if (self.primaryToolbarCoordinator.viewController) {
        self.orchestrator.toolbarAnimatee =
            self.primaryToolbarCoordinator.toolbarAnimatee;
        [self updateToolsMenuButtonGuideForController:
            self.primaryToolbarCoordinator.viewController];
      }
      break;
  }
}

// Maintain the visibility of progress bar for the toolbar. Each toolbar has
// a progress bar. Make sure only one progress bar is visible, and based on the
// ToolbarType.
- (void)updateProgressBarVisibilityWithToolbarType:(ToolbarType)toolbarType {
  BOOL bottomOmniboxEnabled = toolbarType == ToolbarType::kSecondary;
  [self.primaryToolbarCoordinator
      setLocationBarShouldShowProgressBar:!bottomOmniboxEnabled];
  [self.vivaldiTopToolbarCoordinator
      setLocationBarShouldShowProgressBar:!bottomOmniboxEnabled];
  [self.secondaryToolbarCoordinator
      setLocationBarShouldShowProgressBar:bottomOmniboxEnabled];
}

- (void)updateToolsMenuButtonGuideForController:
    (AdaptiveToolbarViewController*)controller {
  controller.toolsMenuButton.guideName = vToolsMenuGuide;
  [controller refreshToolbarButtonsGuide];
}

#pragma mark - ToolbarMediatorDelegate (Vivaldi)

- (void)transitionOmniboxToToolbarType:(ToolbarType)toolbarType
                         tabBarEnabled:(BOOL)tabBarEnabled {
  _omniboxPosition = toolbarType;
  _tabBarEnabled = tabBarEnabled;
  [self vivaldiTransitionOmniboxToToolbarType:toolbarType];
  [self updateToolbarWithBottomOmniboxEnabled:
      toolbarType == ToolbarType::kSecondary
                                tabBarEnabled:tabBarEnabled];
  self.secondaryToolbarButtonStackView.hidden =
      (tabBarEnabled && toolbarType == ToolbarType::kSecondary) ||
          !IsSplitToolbarMode(self.traitEnvironment);
  [self updateToolbarsLayout];

  AdaptiveToolbarCoordinator* adaptiveToolbarCoordinator =
      [self coordinatorWithToolbarType:toolbarType];
  if (![self activeWebState])
    return;
  [adaptiveToolbarCoordinator updateConsumerForWebState:[self activeWebState]];
}

- (void)transitionSteadyStateOmniboxToToolbarType:(ToolbarType)toolbarType
                                    tabBarEnabled:(BOOL)tabBarEnabled {
  _steadyStateOmniboxPosition = toolbarType;
}

- (void)updateToolbarWithBottomOmniboxEnabled:(BOOL)bottomOmniboxEnabled
                                tabBarEnabled:(BOOL)tabBarEnabled {
  PrimaryToolbarView* primaryView = [self primaryToolbarView];
  if (primaryView) {
    primaryView.bottomOmniboxEnabled = bottomOmniboxEnabled;
    primaryView.tabBarEnabled = tabBarEnabled;
    [primaryView redrawToolbarButtons];
    primaryView.toolsMenuButton.guideName = kToolsMenuGuide;

    // Add guide to omnibox view to track omnibox position in the subviews.
    [self.layoutGuideCenter referenceView:primaryView.locationBarContainer
                                underName:kTopOmniboxGuide];
  }

  SecondaryToolbarView* secondaryView = [self secondaryToolbarView];
  if (secondaryView) {
    secondaryView.bottomOmniboxEnabled = bottomOmniboxEnabled;
    secondaryView.tabBarEnabled = tabBarEnabled;
  }
}

/// Triggers updating the toolbar accent color with omnibox focus state
/// when tab bar is disabled. When omnibox is focused custom or dynamic accent
/// color is replaced by default background color to match the color of omnibox
/// search results  view color.
- (void)updateToolbarBackgroundColorWithOmniboxFocus:(BOOL)focused {
  if (_tabBarEnabled)
    return;

  AdaptiveToolbarCoordinator* adaptiveToolbarCoordinator =
      [self coordinatorWithToolbarType:_omniboxPosition];
  [adaptiveToolbarCoordinator.viewController setIsOmniboxFocused:focused];

  AdaptiveToolbarViewController* primaryVC;
  if (_omniboxPosition == ToolbarType::kSecondary) {
    primaryVC = (PrimaryToolbarViewController*)
        [self.primaryToolbarCoordinator viewController];
  }
  if (primaryVC) {
    [primaryVC setIsOmniboxFocused:focused];
  }
}

#pragma mark - Adblocker manager

- (void)initialiseAdblockManager {
  if (!self.browser)
    return;
  self.adblockManager =
      [[VivaldiATBManager alloc] initWithBrowser:self.browser];
  self.adblockManager.consumer = self;
  [self updateVivaldiShieldState];
}

- (void)updateVivaldiShieldState {
  [self.locationBarCoordinator
      setTrackerBlockerSettingForActiveWebState:
          [self atbSettingsForActiveWebState]];
}

- (ATBSettingType)atbSettingsForActiveWebState {
  web::WebState* webState = [self activeWebState];
  if (!webState)
    return [self globalATBSetting];

  NewTabPageTabHelper* NTPHelper = NewTabPageTabHelper::FromWebState(webState);
  BOOL isNTP = NTPHelper && NTPHelper->IsActive();
  // Return Global Adblocker Settings for New Tab Page.
  if (isNTP)
    return [self globalATBSetting];

  // Find the settings for last committed URL of the active WebState.
  NSString* lastCommittedURLString =
      base::SysUTF8ToNSString(webState->GetLastCommittedURL().spec());
  NSURL* lastCommittedURL = [NSURL URLWithString:lastCommittedURLString];
  NSString* host = [lastCommittedURL host];
  if (!host)
    return [self globalATBSetting];
  return [self.adblockManager blockingSettingForDomain:host];
}

- (ATBSettingType)globalATBSetting {
  return [self.adblockManager globalBlockingSetting];
}

- (web::WebState*)activeWebState {
  return self.browser->GetWebStateList()->GetActiveWebState();
}

#pragma mark: - VivaldiATBConsumer
- (void)didRefreshSettingOptions:(NSArray*)options {
  if (options.count > 0)
    [self updateVivaldiShieldState];
}

- (void)didRefreshExceptionsList:(NSArray*)exceptions {
  if (exceptions.count > 0)
    [self updateVivaldiShieldState];
}

- (void)ruleServiceStateDidLoad {
  [self updateVivaldiShieldState];
}

#pragma mark - LocationBarSteadyViewConsumer

- (void)updateLocationText:(NSString*)text clipTail:(BOOL)clipTail {
  // No op.
}

- (void)updateLocationText:(NSString*)text
                    domain:(NSString*)domain
                  showFull:(BOOL)showFull {
  [self.steadyViewConsumer updateLocationText:text
                                       domain:domain
                                     showFull:showFull];
  [self updateVivaldiShieldState];
}

- (void)updateLocationIcon:(UIImage*)icon
        securityStatusText:(NSString*)statusText {
  [self.steadyViewConsumer updateLocationIcon:icon
                           securityStatusText:statusText];
}

- (void)updateAfterNavigatingToNTP {
  [self.steadyViewConsumer updateAfterNavigatingToNTP];
}

- (void)updateLocationShareable:(BOOL)shareable {
  [self.steadyViewConsumer updateLocationShareable:shareable];
}

- (void)attemptShowingLensOverlayIPH {
  // No op.
}

- (void)recordLensOverlayAvailability {
  // No op.
}
// End Vivaldi

@end
