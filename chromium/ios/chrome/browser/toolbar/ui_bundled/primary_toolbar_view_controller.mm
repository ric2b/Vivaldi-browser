// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/primary_toolbar_view_controller.h"

#import "base/check.h"
#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/banner_promo/model/default_browser_banner_promo_app_agent.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_animator.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/omnibox/public/omnibox_ui_features.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/dynamic_type_util.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/toolbar/ui_bundled/adaptive_toolbar_view_controller+subclassing.h"
#import "ios/chrome/browser/toolbar/ui_bundled/banner_promo_view.h"
#import "ios/chrome/browser/toolbar/ui_bundled/buttons/toolbar_button.h"
#import "ios/chrome/browser/toolbar/ui_bundled/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/toolbar/ui_bundled/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/toolbar/ui_bundled/primary_toolbar_view.h"
#import "ios/chrome/browser/toolbar/ui_bundled/primary_toolbar_view_controller_delegate.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_constants.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_height_delegate.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_utils.h"
#import "ios/chrome/browser/toolbar/ui_bundled/tab_groups/ui/tab_group_indicator_view.h"
#import "ios/chrome/common/ui/util/ui_util.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_view_controller.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/ntp/vivaldi_ntp_constants.h"

using vivaldi::IsVivaldiRunning;
// End Vivaldi

namespace {
// Duration for the banner promo appearance/disappearance animation
const base::TimeDelta kBannerPromoAnimationDuration = base::Seconds(0.5);
}  // namespace

// TODO(crbug.com/374808149): Clean up the killswitch.
BASE_FEATURE(kPrimaryToolbarViewDidLoadUpdateViews,
             base::FEATURE_ENABLED_BY_DEFAULT);

@interface PrimaryToolbarViewController () <TabGroupIndicatorViewDelegate>

// Redefined to be a PrimaryToolbarView.
@property(nonatomic, strong) PrimaryToolbarView* view;
@property(nonatomic, assign) BOOL isNTP;
// The last fullscreen progress registered.
@property(nonatomic, assign) CGFloat previousFullscreenProgress;
// Pan Gesture Recognizer for the view revealing pan gesture handler.
@property(nonatomic, weak) UIPanGestureRecognizer* panGestureRecognizer;

@end

@implementation PrimaryToolbarViewController

@dynamic view;

#pragma mark - AdaptiveToolbarViewController

- (void)updateForSideSwipeSnapshot:(BOOL)onNonIncognitoNTP {
  [super updateForSideSwipeSnapshot:onNonIncognitoNTP];
  if (!onNonIncognitoNTP) {
    return;
  }

  // An opaque image is expected during a snapshot. Make sure the view is not
  // hidden and display a blank view by using the NTP background and by hidding
  // the location bar.
  self.view.hidden = NO;

  if (IsVivaldiRunning()) {
    self.view.backgroundColor =
        [self toolbarBackgroundColorForType:ToolbarType::kPrimary];
  } else {
  self.view.backgroundColor =
      self.buttonFactory.toolbarConfiguration.NTPBackgroundColor;
  } // End Vivaldi

  self.view.locationBarContainer.hidden = YES;
}

- (void)resetAfterSideSwipeSnapshot {
  [super resetAfterSideSwipeSnapshot];
  // Note: the view is made visible or not by an `updateToolbar` call when the
  // snapshot animation ends.

  if (IsVivaldiRunning()) {
    self.view.backgroundColor =
        [self toolbarBackgroundColorForType:ToolbarType::kPrimary];
  } else {
  self.view.backgroundColor =
      self.buttonFactory.toolbarConfiguration.backgroundColor;
  } // End Vivaldi

  if (self.hasOmnibox) {
    self.view.locationBarContainer.hidden = NO;
  }
}

#pragma mark - AdaptiveToolbarViewController (Subclassing)

- (void)setLocationBarViewController:
    (UIViewController*)locationBarViewController {
  [super setLocationBarViewController:locationBarViewController];

  self.view.separator.hidden = !self.hasOmnibox;
  [self updateBackgroundColor];

  // Vivaldi
  self.view.leadingStackView.hidden = !self.hasOmnibox;
  self.view.trailingStackView.hidden = !self.hasOmnibox;
  self.view.bottomOmniboxEnabled = self.hasOmnibox;
  [self updateForFullscreenProgress:1];
  // End Vivaldi

}

- (void)updateBackgroundColor {

  if (IsVivaldiRunning()) {
    [UIView animateWithDuration:0.3 animations:^{
      self.view.backgroundColor =
          [self toolbarBackgroundColorForType:ToolbarType::kPrimary];
      [self updateLocationBarBackgroundColor];
      [self updateToolbarButtonsTintColor];
    }];
  } else {
  UIColor* backgroundColor =
      self.buttonFactory.toolbarConfiguration.backgroundColor;
  if (base::FeatureList::IsEnabled(kThemeColorInTopToolbar) &&
      !self.hasOmnibox) {
    if (self.pageThemeColor) {
      backgroundColor = self.pageThemeColor;
    } else if (self.underPageBackgroundColor) {
      backgroundColor = self.underPageBackgroundColor;
    }
  }
  self.view.backgroundColor = backgroundColor;
  } // End Vivaldi

}

#pragma mark - NewTabPageControllerDelegate

- (void)setScrollProgressForTabletOmnibox:(CGFloat)progress {
  [super setScrollProgressForTabletOmnibox:progress];

  // Always show the omnibox for Vivaldi
  if (IsVivaldiRunning())
    progress = 1; // End Vivaldi

  // Sometimes an NTP may make a delegate call when it's no longer visible.
  if (!self.isNTP) {
    progress = 1;
  }

  if (progress == 1) {
    self.view.locationBarContainer.transform = CGAffineTransformIdentity;
  } else {
    self.view.locationBarContainer.transform = CGAffineTransformMakeTranslation(
        0, [self verticalMarginForLocationBarForFullscreenProgress:1] *
               (progress - 1));
  }
  self.view.locationBarContainer.alpha = progress;
  self.view.separator.alpha = progress;

  // When the locationBarContainer is hidden, show the `fakeOmniboxTarget`.
  if (progress == 0 && !self.view.fakeOmniboxTarget) {
    [self.view addFakeOmniboxTarget];
    UITapGestureRecognizer* tapRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self.omniboxCommandsHandler
                action:@selector(focusOmnibox)];
    [self.view.fakeOmniboxTarget addGestureRecognizer:tapRecognizer];
  } else if (progress > 0 && self.view.fakeOmniboxTarget) {
    [self.view removeFakeOmniboxTarget];
  }
}

#pragma mark - UIViewController

- (void)loadView {
  DCHECK(self.buttonFactory);

  // The first time, the toolbar is fully displayed.
  self.previousFullscreenProgress = 1;

  self.view =
      [[PrimaryToolbarView alloc] initWithButtonFactory:self.buttonFactory];
  [self.layoutGuideCenter referenceView:self.view
                              underName:kPrimaryToolbarGuide];

  // This method cannot be called from the init as the topSafeAnchor can only be
  // set to topLayoutGuide after the view creation on iOS 10.
  [self.view setUp];

  self.view.bannerPromo.delegate = self.bannerPromoDelegate;

  // Reference the location bar container as the top omnibox layout guide.
  // Force the synchronous layout update, as this fixes the screen rotation
  // animation in this case.
  [self.layoutGuideCenter referenceView:self.view.locationBarContainer
                              underName:kTopOmniboxGuide
         forcesSynchronousLayoutUpdates:YES];

  // Note:(prio@vivaldi.com) - Add the guide in ToolbarCoordinator since its
  // used for both top and bottom omnibox.
  if (!IsVivaldiRunning()) {
  [self.layoutGuideCenter referenceView:self.view.locationBarContainer
                              underName:kTopOmniboxGuide];
  } // End Vivaldi

  self.view.locationBarBottomConstraint.constant =
      [self verticalMarginForLocationBarForFullscreenProgress:1];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // We register for specific trait changes (vertical and horizontal size
  // classes) and provide a handler method
  // `updateViews:previousTraitCollection:` to be called when those traits
  // change.
  [self
      registerForTraitChanges:
          @[ UITraitVerticalSizeClass.class, UITraitHorizontalSizeClass.class ]
                   withAction:@selector(updateViews:previousTraitCollection:)];
  // TODO(crbug.com/374808149): Clean up the killswitch.
  if (base::FeatureList::IsEnabled(kPrimaryToolbarViewDidLoadUpdateViews)) {
    [self updateViews:self.view previousTraitCollection:nil];
  }
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray<UIKeyCommand*>*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  base::RecordAction(base::UserMetricsAction(kMobileKeyCommandClose));
  [self.delegate close];
}

#pragma mark - Public

- (void)setTabGroupIndicatorView:(TabGroupIndicatorView*)view {
  view.delegate = self;
  self.view.tabGroupIndicatorView = view;
}

#pragma mark - Property accessors

- (void)setIsNTP:(BOOL)isNTP {
  if (isNTP == _isNTP) {
    return;
  }
  [super setIsNTP:isNTP];
  _isNTP = isNTP;

  // Vivaldi
  [self updateBackgroundColor];
  // End Vivaldi

  // The omnibox is always visible when having two toolbars and no TabStrip.
  BOOL omniboxAlwaysVisible =
      IsSplitToolbarMode(self) && !CanShowTabStrip(self);
  if (omniboxAlwaysVisible || !self.shouldHideOmniboxOnNTP) {
    return;
  }

  // This is hiding/showing and positionning the omnibox. This is only needed
  // if the omnibox should be hidden when there is only one toolbar.
  [self setScrollProgressForTabletOmnibox:(isNTP ? 0 : 1)];
}

- (BOOL)locationBarIsExpanded {
  for (NSLayoutConstraint* constraint in self.view.expandedConstraints) {
    if (!constraint.isActive) {
      return false;
    }
  }
  return true;
}

#pragma mark - SharingPositioner

- (UIView*)sourceView {
  return self.view.shareButton;
}

- (CGRect)sourceRect {
  return self.view.shareButton.bounds;
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  [super updateForFullscreenProgress:progress];

  self.previousFullscreenProgress = progress;

  CGFloat alphaValue = fmax(progress * 2 - 1, 0);

  // Note: (prio@vivaldi.com): We will use the same alpha computation for tab
  // strip and toolbar from BVC so that at the time of scrolling
  // all the related views fade in sync.
  if (IsVivaldiRunning())
    alphaValue = fmax((progress - 0.85) / 0.15, 0);
  // End Vivaldi

  self.view.leadingStackView.alpha = alphaValue;
  self.view.trailingStackView.alpha = alphaValue;
  self.view.locationBarBottomConstraint.constant =
      [self verticalMarginForLocationBarForFullscreenProgress:progress];

  [self.view updateForFullscreenProgress:progress];
}

#pragma mark - ToolbarAnimatee

- (void)expandLocationBar {
  [self deactivateViewLocationBarConstraints];
  [NSLayoutConstraint activateConstraints:self.view.expandedConstraints];
  [self.delegate locationBarExpandedInViewController:self];
  [self.view layoutIfNeeded];
}

- (void)contractLocationBar {
  [self deactivateViewLocationBarConstraints];
  if (IsSplitToolbarMode(self)) {
    [NSLayoutConstraint
        activateConstraints:self.view.contractedNoMarginConstraints];
  } else {
    [NSLayoutConstraint activateConstraints:self.view.contractedConstraints];
  }
  [self.delegate locationBarContractedInViewController:self];
  [self.view layoutIfNeeded];
}

- (void)showCancelButton {
  self.view.cancelButton.hidden = NO;
}

- (void)hideCancelButton {
  self.view.cancelButton.hidden = YES;
}

- (void)showControlButtons {
  for (ToolbarButton* button in self.view.allButtons) {
    button.alpha = 1;
  }

  // Vivaldi
  [self.view handleToolbarButtonVisibility:YES];
  // End Vivaldi

}

- (void)hideControlButtons {
  for (ToolbarButton* button in self.view.allButtons) {
    button.alpha = 0;
  }

  // Vivaldi
  [self.view handleToolbarButtonVisibility:NO];
  // End Vivaldi

}

- (void)setToolbarFaded:(BOOL)faded {
  self.view.alpha = faded ? 0 : 1;
}

- (void)setLocationBarHeightToMatchFakeOmnibox {
  if (!IsSplitToolbarMode(self)) {
    return;
  }
  [self setLocationBarContainerHeight:content_suggestions::
                                          PinnedFakeOmniboxHeight()];
  self.view.matchNTPHeight = YES;
}

- (void)setLocationBarHeightExpanded {
  [self setLocationBarContainerHeight:LocationBarHeight(
                                          self.traitCollection
                                              .preferredContentSizeCategory)];
  self.view.matchNTPHeight = NO;
}

#pragma mark - PrimaryToolbarConsumer

- (void)showBannerPromo {

  if (IsVivaldiRunning())
    // Do not show the banner promo since that breaks our Primary Toolbar UI.
    return; // End Vivaldi

  [self.view prepareToShowBannerPromo];
  [self.view.superview layoutIfNeeded];

  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kBannerPromoAnimationDuration.InSecondsF()
      animations:^{
        [weakSelf showBannerPromoAnimationBlock];
      }
      completion:^(BOOL success) {
        if (success) {
          [weakSelf showBannerPromoCompletionBlock];
        }
      }];
}

// Helper method to actually do the animation to show the banner promo.
- (void)showBannerPromoAnimationBlock {
  [self.view showBannerPromo];
  [self.toolbarHeightDelegate toolbarsHeightChanged];
  [self.view.superview layoutIfNeeded];
}

- (void)showBannerPromoCompletionBlock {
  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                  self.view.bannerPromo);
}

- (void)hideBannerPromo {

  if (IsVivaldiRunning())
    // Avoid triggering any changes to constraints or IntrinsicContentSize to
    // prevent potential size changes.
    return; // End Vivaldi

  [self.view.superview layoutIfNeeded];
  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kBannerPromoAnimationDuration.InSecondsF()
      animations:^{
        [weakSelf hideBannerPromoAnimationBlock];
      }
      completion:^(BOOL completed) {
        [weakSelf hideBannerPromoCompletionBlock];
      }];
}

// Helper method to actually do the animation to hide the banner promo.
- (void)hideBannerPromoAnimationBlock {
  [self.view hideBannerPromo];
  [self.toolbarHeightDelegate toolbarsHeightChanged];
  [self.view.superview layoutIfNeeded];
}

// Helper method for completion.
- (void)hideBannerPromoCompletionBlock {
  [self.view cleanupAfterHideBannerPromo];
  [self.view.superview layoutIfNeeded];
}
#pragma mark - Private

// Adjusts the layout and appearance of views in response to changes in
// available space and trait collections.
- (void)updateViews:(UIView*)updatedView
    previousTraitCollection:(UITraitCollection*)previousTraitCollection {
  self.view.locationBarBottomConstraint.constant =
      [self verticalMarginForLocationBarForFullscreenProgress:
                self.previousFullscreenProgress];
  self.view.topCornersRounded = NO;
  [self.view updateTabGroupIndicatorAvailability];
  [self.delegate
      viewControllerTraitCollectionDidChange:previousTraitCollection];
}

- (CGFloat)clampedFontSizeMultiplier {
  return ToolbarClampedFontSizeMultiplier(
      self.traitCollection.preferredContentSizeCategory);
}

// Returns the vertical margin to the location bar based on fullscreen
// `progress`, aligned to the nearest pixel.
- (CGFloat)verticalMarginForLocationBarForFullscreenProgress:(CGFloat)progress {
  // The vertical bottom margin for the location bar is such that the location
  // bar looks visually centered. However, the constraints are not geometrically
  // centering the location bar. It is moved by 0pt in iPhone landscape and by
  // 3pt in all other configurations.

  if (IsVivaldiRunning() && !self.hasOmnibox)
    return 0; // End Vivaldi

  CGFloat fullscreenVerticalMargin =
      IsCompactHeight(self) ? 0 : kAdaptiveLocationBarVerticalMarginFullscreen;
  return -AlignValueToPixel((kAdaptiveLocationBarVerticalMargin * progress +
                             fullscreenVerticalMargin * (1 - progress)) *
                                [self clampedFontSizeMultiplier] +
                            ([self clampedFontSizeMultiplier] - 1) *
                                kLocationBarVerticalMarginDynamicType);
}

// Deactivates the constraints on the location bar positioning.
- (void)deactivateViewLocationBarConstraints {
  [NSLayoutConstraint deactivateConstraints:self.view.contractedConstraints];
  [NSLayoutConstraint
      deactivateConstraints:self.view.contractedNoMarginConstraints];
  [NSLayoutConstraint deactivateConstraints:self.view.expandedConstraints];
}

// Sets the height of the location bar container.
- (void)setLocationBarContainerHeight:(CGFloat)height {
  PrimaryToolbarView* view = self.view;
  view.locationBarContainerHeight.constant = height;

  if (IsVivaldiRunning()) {
    view.locationBarContainer.layer.cornerRadius =
        vNTPSearchBarCornerRadius;
  } else {
  view.locationBarContainer.layer.cornerRadius = height / 2;
  } // End Vivaldi

}

#pragma mark - TabGroupIndicatorViewDelegate

- (void)tabGroupIndicatorViewVisibilityUpdated:(BOOL)visible {
  [self.view tabGroupIndicatorViewVisibilityUpdated:visible];
  [self.delegate viewController:self
      tabGroupIndicatorVisibilityUpdated:visible];
}

#pragma mark: - Vivaldi

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  __weak PrimaryToolbarViewController* weakSelf = self;

  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext>) {
        // No op.
      }
      completion:^(id<UIViewControllerTransitionCoordinatorContext>) {
        [weakSelf refreshToolbarButtons];
      }];
}

- (void)refreshToolbarButtons {
  [self.view redrawToolbarButtons];
}

- (void)updateLocationBarBackgroundColor {
  // Update omnibox background color. When tab bar is enabled its not modified
  // with accent color and rather follows the prefixed color. However, when tab
  // bar is disabled the color is calculated from the accent color so that its
  // visible regardless of the accent color.
  if (self.isTabBarEnabled || self.isOmniboxFocused) {
    self.view.locationBarContainer.backgroundColor =
        [self.buttonFactory.toolbarConfiguration
         locationBarBackgroundColorWithVisibility:1.0];
  } else {
    self.view.locationBarContainer.backgroundColor =
        [self.buttonFactory.toolbarConfiguration
            locationBarBackgroundColorForAccentColor:[self finalAccentColor]];
  }
}

- (void)updateToolbarButtonsTintColor {
  // Update toolbar buttons tint color
  // When tab is enabled we don't need a dynamic tint color for toolbar.
  UIColor* buttonsTintColor = self.isTabBarEnabled ?
      [UIColor colorNamed:kToolbarButtonColor] :
          [self.buttonFactory.toolbarConfiguration
              buttonsTintColorForAccentColor:[self finalAccentColor]];
  self.buttonFactory.toolbarConfiguration.buttonsTintColor = buttonsTintColor;


  for (ToolbarButton *button in self.view.leadingStackView.arrangedSubviews) {
    [button updateTintColor];
  }

  for (ToolbarButton *button in self.view.trailingStackView.arrangedSubviews) {
    [button updateTintColor];
  }

  // Update location bar steady view tint color
  LocationBarViewController* viewController =
      (LocationBarViewController*)
          self.locationBarViewController;
  if (!viewController)
    return;
  // When tab is enabled we don't need a dynamic tint color for steady view.
  UIColor* locationContentsTintColor = self.isTabBarEnabled ?
      [UIColor colorNamed:kToolbarButtonColor] :
      [self.buttonFactory.toolbarConfiguration
          locationBarSteadyViewTintColorForAccentColor:[self finalAccentColor]];
  [viewController
      updateSteadyViewColorSchemeWithColor:locationContentsTintColor];
  // Update the container color which is used address bar context menu preview.
  viewController.locationBarContainerColor =
      self.view.locationBarContainer.backgroundColor;
}

// Returns the accent color to used for primary toolbar type which depends on
// omnibox position and tab bar style.
- (UIColor*)finalAccentColor {
  ToolbarType toolbarType =
      self.isBottomOmniboxEnabled && !self.isTabBarEnabled ?
          ToolbarType::kSecondary : ToolbarType::kPrimary;
  UIColor* accentColor =
      [self toolbarBackgroundColorForType:toolbarType];
  return accentColor;
}
// End Vivaldi

@end
