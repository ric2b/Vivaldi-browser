// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_presenter.h"

#import "base/time/time.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/omnibox/popup/content_providing.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "ui/base/device_form_factor.h"
#import "ui/gfx/ios/uikit_util.h"

// Vivaldi
#import "app/vivaldi_apptools.h"

using vivaldi::IsVivaldiRunning;
// End Vivaldi

namespace {
const CGFloat kVerticalOffset = 6;
const CGFloat kPopupBottomPaddingTablet = 80;

/// Duration of the fade in animation.
constexpr NSTimeInterval kFadeInAnimationDuration =
    base::Milliseconds(300).InSecondsF();
/// Vertical offset of the suggestions when fading in.
const CGFloat kFadeAnimationVerticalOffset = 12;

// Vivaldi
// Bottom corner radius for the suggerstion container view.
const CGFloat vPopupContainerCornerRadius = 8;
// End Vivaldi

}  // namespace

@interface OmniboxPopupPresenter ()
/// Constraint for the bottom anchor of the popup when form factor is phone.
@property(nonatomic, strong) NSLayoutConstraint* bottomConstraintPhone;
/// Constraint for the height anchor of the popup when form factor is tablet.
@property(nonatomic, strong) NSLayoutConstraint* heightConstraintTablet;

@property(nonatomic, weak) id<OmniboxPopupPresenterDelegate> delegate;
@property(nonatomic, weak) UIViewController<ContentProviding>* viewController;
@property(nonatomic, strong) UIView* popupContainerView;
/// Separator for the bottom edge of the popup on iPad.
@property(nonatomic, strong) UIView* bottomSeparator;
/// Top constraint between the popup and it's container. This is used to animate
/// suggestions when focusing the omnibox.
@property(nonatomic, strong) NSLayoutConstraint* popupTopConstraint;

// The layout guide center to use to refer to the omnibox.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;
@property(nonatomic, strong) UILayoutGuide* topOmniboxGuide;

// Vivaldi
@property(nonatomic, strong) UILayoutGuide* vivaldiOmniboxGuide;
// Constraint for popup top anchor when omnibox is at the top.
@property(nonatomic, strong) NSLayoutConstraint* popupTopConstraintTopOmnibox;
// Constraint for popup bottom anchor when omnibox is at the top.
@property(nonatomic, strong) NSLayoutConstraint*
    popupBottomConstraintTopOmnibox;
// Constraint for popup top anchor when omnibox is at the bottom.
@property(nonatomic, strong) NSLayoutConstraint*
    popupTopConstraintBottomOmnibox;
// Constraint for popup bottom anchor when omnibox is at the bottom.
@property(nonatomic, strong) NSLayoutConstraint*
    popupBottomConstraintBottomOmnibox;
// End Vivaldi

@end

@implementation OmniboxPopupPresenter {
  /// Type of the toolbar that contains the omnibox when it's not focused. The
  /// animation of focusing/defocusing the omnibox changes depending on this
  /// position.
  ToolbarType _unfocusedOmniboxToolbarType;
}

- (instancetype)
    initWithPopupPresenterDelegate:(id<OmniboxPopupPresenterDelegate>)delegate
               popupViewController:
                   (UIViewController<ContentProviding>*)viewController
                 layoutGuideCenter:(LayoutGuideCenter*)layoutGuideCenter
                         incognito:(BOOL)incognito {
  self = [super init];
  if (self) {
    _delegate = delegate;
    _viewController = viewController;
    _layoutGuideCenter = layoutGuideCenter;

    UIView* containerView = [[UIView alloc] init];
    [containerView addSubview:viewController.view];
    _popupContainerView = containerView;

    UIUserInterfaceStyle userInterfaceStyle =
        incognito ? UIUserInterfaceStyleDark : UIUserInterfaceStyleUnspecified;
    // Both the container view and the popup view controller need
    // overrideUserInterfaceStyle set because the overall popup background
    // comes from the container, but overrideUserInterfaceStyle won't
    // propagate from a view to any subviews in a different view controller.
    _popupContainerView.overrideUserInterfaceStyle = userInterfaceStyle;
    viewController.overrideUserInterfaceStyle = userInterfaceStyle;

    if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
      _popupContainerView.backgroundColor =
          [UIColor colorNamed:kPrimaryBackgroundColor];
    } else {
      _popupContainerView.backgroundColor =
          [self.delegate popupBackgroundColorForPresenter:self];
    }

    _popupContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    viewController.view.translatesAutoresizingMaskIntoConstraints = NO;

    if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
      self.viewController.view.layer.masksToBounds = YES;

      AddSameConstraints(viewController.view, _popupContainerView);
    } else {
      AddSameConstraintsToSides(viewController.view, _popupContainerView,
                                LayoutSides::kLeading | LayoutSides::kTrailing |
                                    LayoutSides::kBottom);
      _popupTopConstraint = [viewController.view.topAnchor
          constraintEqualToAnchor:_popupContainerView.topAnchor];
      _popupTopConstraint.active = YES;

      // Add bottom separator. This will only be visible on iPad where
      // the omnibox doesn't fill the whole screen.
      _bottomSeparator = [[UIView alloc] initWithFrame:CGRectZero];
      _bottomSeparator.translatesAutoresizingMaskIntoConstraints = NO;
      _bottomSeparator.backgroundColor =
          [UIColor colorNamed:kToolbarShadowColor];

      [_popupContainerView addSubview:self.bottomSeparator];

      CGFloat separatorHeight =
          ui::AlignValueToUpperPixel(kToolbarSeparatorHeight);
      [NSLayoutConstraint activateConstraints:@[
        [self.bottomSeparator.heightAnchor
            constraintEqualToConstant:separatorHeight],
        [self.bottomSeparator.leadingAnchor
            constraintEqualToAnchor:_popupContainerView.leadingAnchor],
        [self.bottomSeparator.trailingAnchor
            constraintEqualToAnchor:_popupContainerView.trailingAnchor],
        [self.bottomSeparator.topAnchor
            constraintEqualToAnchor:_popupContainerView.bottomAnchor],
      ]];
    }
  }
  return self;
}

- (void)updatePopupOnFocus:(BOOL)isFocusingOmnibox {
  BOOL popupHasContent = self.viewController.hasContent;
  BOOL popupIsOnscreen = self.popupContainerView.superview != nil;
  if (!popupHasContent && popupIsOnscreen) {
    // If intrinsic size is 0 and popup is onscreen, we want to remove the
    // popup view.
    if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
      self.bottomConstraintPhone.active = NO;
      self.bottomSeparator.hidden = YES;
    }

    [self.viewController willMoveToParentViewController:nil];
    [self.popupContainerView removeFromSuperview];
    [self.viewController removeFromParentViewController];

    self.open = NO;
    [self.delegate popupDidCloseForPresenter:self];
  } else if (popupHasContent && !popupIsOnscreen) {
    // If intrinsic size is nonzero and popup is offscreen, we want to add it.
    UIViewController* parentVC =
        [self.delegate popupParentViewControllerForPresenter:self];
    [parentVC addChildViewController:self.viewController];
    [[self.delegate popupParentViewForPresenter:self]
        addSubview:self.popupContainerView];
    [self.viewController didMoveToParentViewController:parentVC];

    BOOL enableFocusAnimation =
        IsBottomOmniboxAvailable() && isFocusingOmnibox &&
        _unfocusedOmniboxToolbarType == ToolbarType::kSecondary;

    if (IsVivaldiRunning()) {
      [self initialLayoutVivaldi];
      if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
        self.bottomSeparator.hidden = NO;
      } else {
        self.heightConstraintTablet.active = YES;
      }
    } else {
    [self initialLayoutAnimated:enableFocusAnimation];

    [self updatePopupConstraints];
    } // End Vivaldi

    self.open = YES;
    [self.delegate popupDidOpenForPresenter:self];
  }
}

/// With popout omnibox, the popup might be in either of two states:
/// a) regular x regular state, where the popup matches OB width
/// b) compact state, where popup takes whole screen width
/// Therefore, on trait collection change, re-add the popup and recreate the
/// constraints to make sure the correct ones are used.
- (void)updatePopupAfterTraitCollectionChange {
  // Re-add the popup container to break any existing constraints.
  [self.popupContainerView removeFromSuperview];
  [[self.delegate popupParentViewForPresenter:self]
      addSubview:self.popupContainerView];

  if (IsVivaldiRunning()) {
    // Re-add necessary constraints.
    [self initialLayoutVivaldi];
    if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
      self.bottomSeparator.hidden = NO;
    }
  } else {
  // Re-add necessary constraints.
  [self initialLayoutAnimated:NO];
  [self updatePopupConstraints];
  } // End Vivaldi

}

- (void)updatePopupConstraints {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    BOOL showRegularLayout =
        IsRegularXRegularSizeClass(self.popupContainerView.traitCollection);
    self.bottomConstraintPhone.active = !showRegularLayout;
    self.heightConstraintTablet.active = showRegularLayout;
  } else {
    self.bottomConstraintPhone.active = YES;
    self.bottomSeparator.hidden = NO;
  }
}

#pragma mark - ToolbarOmniboxConsumer

- (void)steadyStateOmniboxMovedToToolbar:(ToolbarType)toolbarType {
  _unfocusedOmniboxToolbarType = toolbarType;
}

#pragma mark - Private

/// Layouts the popup when it is just added to the view hierarchy.
- (void)initialLayoutAnimated:(BOOL)isAnimated {
  UIView* popup = self.popupContainerView;
  // Creates the constraints if the view is newly added to the view hierarchy.

  // Vivaldi: We would prefer not to take full height for phone form factor.
  // This means only height based on suggested contents will be covered by
  // the suggestions. and the contents underneath will be interactable on a
  // new tab page, for example the speed dials will be visible and can be tapped
  // However, in browsing mode, tapping on the contents underneath will remove
  // focus from the omnibox and remove suggestions popup.
  if (IsVivaldiRunning()) {
    self.bottomConstraintPhone =
      [popup.superview.bottomAnchor
          constraintGreaterThanOrEqualToAnchor:popup.bottomAnchor];

    // Add rounded corner on the bottom of the suggestions view.
    self.popupContainerView.clipsToBounds = YES;
    self.popupContainerView.layer.cornerRadius = vPopupContainerCornerRadius;
    self.popupContainerView.layer.maskedCorners =
        kCALayerMinXMaxYCorner | kCALayerMaxXMaxYCorner;
  } else {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    self.bottomConstraintPhone = [popup.superview.safeAreaLayoutGuide
                                      .bottomAnchor
        constraintGreaterThanOrEqualToAnchor:popup.bottomAnchor
                                    constant:
                                        kPopupBottomPaddingTablet +
                                        kSecondaryToolbarWithoutOmniboxHeight];
  } else {
    self.bottomConstraintPhone = [popup.bottomAnchor
        constraintEqualToAnchor:popup.superview.bottomAnchor];
  }
  } // End Vivaldi

  // On tablet form factor the popup is padded on the bottom to allow the user
  // to defocus the omnibox.
  self.heightConstraintTablet = [popup.heightAnchor
      constraintLessThanOrEqualToAnchor:popup.superview.heightAnchor
                             multiplier:0.7];

  // Install in the superview the guide tracking the top omnibox.
  if (self.topOmniboxGuide) {
    [[popup superview] removeLayoutGuide:self.topOmniboxGuide];
    self.topOmniboxGuide = nil;
  }
  GuideName* omniboxGuideName =
      [self.delegate omniboxGuideNameForPresenter:self];
  if (omniboxGuideName) {
    self.topOmniboxGuide =
        [self.layoutGuideCenter makeLayoutGuideNamed:omniboxGuideName];
    [[popup superview] addLayoutGuide:self.topOmniboxGuide];
  }

  [self updatePopupLayer];
  [self updateConstraints];

  [[popup superview] layoutIfNeeded];

  if (isAnimated) {
    [self animatePopupOnOmniboxFocus];
  }
}

// Updates the popup's view layer.
- (void)updatePopupLayer {
  if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }

  _popupContainerView.layer.masksToBounds = NO;

  BOOL showRegularLayout =
      IsRegularXRegularSizeClass(self.popupContainerView.traitCollection);

  _popupContainerView.layer.cornerRadius = showRegularLayout ? 16 : 0;
  _popupContainerView.layer.shadowColor = UIColor.blackColor.CGColor;
  _popupContainerView.layer.shadowRadius = 60;
  _popupContainerView.layer.shadowOffset = CGSizeMake(0, 10);
  _popupContainerView.layer.shadowOpacity = 0.2;
  self.viewController.view.layer.cornerRadius = showRegularLayout ? 16 : 0;
}

// Updates and activates the constraints based on the popup's current view state
- (void)updateConstraints {
  UIView* popup = self.popupContainerView;

  NSLayoutConstraint* topConstraint;
  if (self.topOmniboxGuide) {
    // Position the top anchor of the popup relatively to that layout guide.
    topConstraint = [popup.topAnchor
        constraintEqualToAnchor:self.topOmniboxGuide.bottomAnchor
                       constant:kVerticalOffset];
  } else {
    topConstraint = [popup.topAnchor
        constraintEqualToAnchor:[self.delegate popupParentViewForPresenter:self]
                                    .topAnchor];
  }

  NSMutableArray<NSLayoutConstraint*>* constraintsToActivate =
      [NSMutableArray arrayWithObject:topConstraint];

  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET &&
      IsRegularXRegularSizeClass(self.popupContainerView.traitCollection) &&
      self.topOmniboxGuide) {
    NSLayoutConstraint* leadingConstraint = [popup.leadingAnchor
        constraintEqualToAnchor:self.topOmniboxGuide.leadingAnchor
                       constant:-16];
    leadingConstraint.priority = UILayoutPriorityDefaultHigh;

    NSLayoutConstraint* trailingConstraint = [popup.trailingAnchor
        constraintEqualToAnchor:self.topOmniboxGuide.trailingAnchor
                       constant:16];
    trailingConstraint.priority = UILayoutPriorityDefaultHigh;

    NSLayoutConstraint* centerXConstraint = [popup.centerXAnchor
        constraintEqualToAnchor:self.topOmniboxGuide.centerXAnchor];

    [constraintsToActivate addObjectsFromArray:@[
      leadingConstraint, trailingConstraint, centerXConstraint
    ]];
  } else {
    [constraintsToActivate addObjectsFromArray:@[
      [popup.leadingAnchor
          constraintEqualToAnchor:popup.superview.leadingAnchor],
      [popup.trailingAnchor
          constraintEqualToAnchor:popup.superview.trailingAnchor],
    ]];
  }

  [NSLayoutConstraint activateConstraints:constraintsToActivate];
}

/// Animates the popup for omnibox focus.
- (void)animatePopupOnOmniboxFocus {
  __weak __typeof__(self) weakSelf = self;
  self.viewController.view.alpha = 0.0;
  self.popupTopConstraint.constant = kFadeAnimationVerticalOffset;
  [self.popupContainerView.superview layoutIfNeeded];

  auto constraintForVisiblePopup = ^{
    weakSelf.viewController.view.alpha = 1.0;
    weakSelf.popupTopConstraint.constant = 0.0;
    [weakSelf.popupContainerView.superview layoutIfNeeded];
  };

  [UIView animateWithDuration:kFadeInAnimationDuration
                   animations:constraintForVisiblePopup
                   completion:^(BOOL _) {
                     constraintForVisiblePopup();
                   }];
}

#pragma mark - Vivaldi
/// Layouts the popup when it is just added to the view hierarchy.
- (void)initialLayoutVivaldi {
  UIView* popup = self.popupContainerView;
  // Creates the constraints if the view is newly added to the view hierarchy.

  // We would prefer not to take full height for phone form factor.
  // This means only height based on suggested contents will be covered by
  // the suggestions. and the contents underneath will be interactable on a
  // new tab page, for example the speed dials will be visible and can be tapped
  // However, in browsing mode, tapping on the contents underneath will remove
  // focus from the omnibox and remove suggestions popup.

  self.popupTopConstraintBottomOmnibox =
      [popup.superview.topAnchor
          constraintLessThanOrEqualToAnchor:popup.topAnchor
                constant:-popup.superview.window.safeAreaInsets.top];
  self.popupBottomConstraintTopOmnibox =
      [popup.superview.bottomAnchor
          constraintGreaterThanOrEqualToAnchor:popup.bottomAnchor];

  // Add rounded corner on the bottom of the suggestions view.
  self.popupContainerView.clipsToBounds = YES;
  self.popupContainerView.layer.cornerRadius = vPopupContainerCornerRadius;
  self.popupContainerView.layer.maskedCorners =
      kCALayerMinXMaxYCorner | kCALayerMaxXMaxYCorner;

  // On tablet form factor the popup is padded on the bottom to allow the user
  // to defocus the omnibox.
  self.heightConstraintTablet =
      [popup.heightAnchor
          constraintLessThanOrEqualToAnchor:popup.superview.heightAnchor
                                 multiplier:0.7];

  // Install in the superview the guide tracking the omnibox.
  if (self.vivaldiOmniboxGuide) {
    [[popup superview] removeLayoutGuide:self.vivaldiOmniboxGuide];
  }
  self.vivaldiOmniboxGuide =
      [self.layoutGuideCenter makeLayoutGuideNamed:
          _unfocusedOmniboxToolbarType == ToolbarType::kSecondary ?
              vivaldiBottomOmniboxGuide : kTopOmniboxGuide];
  [[popup superview] addLayoutGuide:self.vivaldiOmniboxGuide];

  // Install in the superview the guide tracking the top omnibox.
  if (self.topOmniboxGuide) {
    [[popup superview] removeLayoutGuide:self.topOmniboxGuide];
  }
  self.topOmniboxGuide =
      [self.layoutGuideCenter makeLayoutGuideNamed:kTopOmniboxGuide];
  [[popup superview] addLayoutGuide:self.topOmniboxGuide];

  // Position the top/bottom anchor of the popup relatively to omnibox layout
  // guide.
  self.popupBottomConstraintBottomOmnibox =
      [popup.bottomAnchor
          constraintEqualToAnchor:self.vivaldiOmniboxGuide.topAnchor];
  self.popupTopConstraintTopOmnibox =
      [popup.topAnchor
          constraintEqualToAnchor:self.vivaldiOmniboxGuide.bottomAnchor];

  NSMutableArray<NSLayoutConstraint*>* constraintsToActivate =
      [NSMutableArray arrayWithObject:self.popupTopConstraintTopOmnibox];

  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET &&
      IsRegularXRegularSizeClass(self.popupContainerView)) {
    [constraintsToActivate addObjectsFromArray:@[
      [popup.leadingAnchor
          constraintEqualToAnchor:self.topOmniboxGuide.leadingAnchor],
      [popup.trailingAnchor
          constraintEqualToAnchor:self.topOmniboxGuide.trailingAnchor],
    ]];
  } else {
    [constraintsToActivate addObjectsFromArray:@[
      [popup.leadingAnchor
          constraintEqualToAnchor:popup.superview.leadingAnchor],
      [popup.trailingAnchor
          constraintEqualToAnchor:popup.superview.trailingAnchor],
    ]];
  }

  // Activate common constraints
  [NSLayoutConstraint activateConstraints:constraintsToActivate];

  // Update constraints based on address bar position
  [self updateConstraintsForBottomOmniboxEnabled:
      _unfocusedOmniboxToolbarType == ToolbarType::kSecondary];
  // Update corner radius for the popup.
  [self updateCornerRadiusForBottomOmniboxEnabled:
      _unfocusedOmniboxToolbarType == ToolbarType::kSecondary];

  [[popup superview] layoutIfNeeded];
}

- (void)updateConstraintsForBottomOmniboxEnabled:(BOOL)isBottomOmniboxEnabled {
  self.popupTopConstraintBottomOmnibox.active = isBottomOmniboxEnabled;
  self.popupBottomConstraintBottomOmnibox.active = isBottomOmniboxEnabled;
  self.popupTopConstraintTopOmnibox.active = !isBottomOmniboxEnabled;
  self.popupBottomConstraintTopOmnibox.active = !isBottomOmniboxEnabled;
}

- (void)updateCornerRadiusForBottomOmniboxEnabled:(BOOL)isBottomOmniboxEnabled {
  self.popupContainerView.layer.maskedCorners = isBottomOmniboxEnabled ?
      (kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner) :
      (kCALayerMinXMaxYCorner | kCALayerMaxXMaxYCorner);
}
// End Vivaldi

@end
