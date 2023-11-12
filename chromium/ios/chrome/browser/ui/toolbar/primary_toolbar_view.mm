// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view.h"

#import "base/check.h"
#import "base/ios/ios_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/dynamic_type_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tab_grid_button.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_progress_bar.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ui/gfx/ios/uikit_util.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_constants.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/ntp/vivaldi_ntp_constants.h"
#import "ios/ui/toolbar/vivaldi_toolbar_constants.h"
#import "ui/base/device_form_factor.h"

using ui::GetDeviceFormFactor;
using ui::DEVICE_FORM_FACTOR_PHONE;
using ui::DEVICE_FORM_FACTOR_TABLET;
using vivaldi::IsVivaldiRunning;
// End Vivaldi

@interface PrimaryToolbarView ()
// Factory used to create the buttons.
@property(nonatomic, strong) ToolbarButtonFactory* buttonFactory;

// ContentView of the vibrancy effect if there is one, self otherwise.
@property(nonatomic, strong) UIView* contentView;

// StackView containing the leading buttons (relative to the location bar). It
// should only contain ToolbarButtons. Redefined as readwrite.
@property(nonatomic, strong, readwrite) UIStackView* leadingStackView;
// Buttons from the leading stack view.
@property(nonatomic, strong) NSArray<ToolbarButton*>* leadingStackViewButtons;
// StackView containing the trailing buttons (relative to the location bar). It
// should only contain ToolbarButtons. Redefined as readwrite.
@property(nonatomic, strong, readwrite) UIStackView* trailingStackView;
// Buttons from the trailing stack view.
@property(nonatomic, strong) NSArray<ToolbarButton*>* trailingStackViewButtons;

// Progress bar displayed below the toolbar, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarProgressBar* progressBar;

// Separator below the toolbar, redefined as readwrite.
@property(nonatomic, strong, readwrite) UIView* separator;

#pragma mark** Buttons in the leading stack view. **
// Button to navigate back, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* backButton;
// Button to navigate forward, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* forwardButton;
// Button to display the TabGrid, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarTabGridButton* tabGridButton;
// Button to stop the loading of the page, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* stopButton;
// Button to reload the page, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* reloadButton;

#pragma mark** Buttons in the trailing stack view. **
// Button to display the share menu, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* shareButton;
// Button to display the tools menu, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* toolsMenuButton;

// Button to cancel the edit of the location bar, redefined as readwrite.
@property(nonatomic, strong, readwrite) UIButton* cancelButton;

#pragma mark** Location bar. **
// Location bar containing the omnibox.
@property(nonatomic, strong) UIView* locationBarView;
// Container for the location bar, redefined as readwrite.
@property(nonatomic, strong, readwrite) UIView* locationBarContainer;
// The height of the container for the location bar, redefined as readwrite.
@property(nonatomic, strong, readwrite)
    NSLayoutConstraint* locationBarContainerHeight;
// Button taking the full size of the toolbar. Expands the toolbar when  tapped.
// Redefined as readwrite.
@property(nonatomic, strong, readwrite) UIButton* collapsedToolbarButton;

// Constraints for the location bar, redefined as readwrite.
@property(nonatomic, strong, readwrite)
    NSMutableArray<NSLayoutConstraint*>* expandedConstraints;
@property(nonatomic, strong, readwrite)
    NSMutableArray<NSLayoutConstraint*>* contractedConstraints;
@property(nonatomic, strong, readwrite)
    NSMutableArray<NSLayoutConstraint*>* contractedNoMarginConstraints;

// Vivaldi
// Button to open and close panel, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* panelButton;
// Button to show tracker blocker summery, this is for iPad only.
@property(nonatomic, strong, readwrite) ToolbarButton* shieldButton;
// Button to show more options such as shield, share button when iPhone is on
// landscape mode.
@property(nonatomic, strong, readwrite) ToolbarButton* vivaldiMoreButton;
// End Vivaldi

@end

@implementation PrimaryToolbarView

@synthesize fakeOmniboxTarget = _fakeOmniboxTarget;
@synthesize locationBarBottomConstraint = _locationBarBottomConstraint;
@synthesize locationBarContainerHeight = _locationBarContainerHeight;
@synthesize buttonFactory = _buttonFactory;
@synthesize allButtons = _allButtons;
@synthesize progressBar = _progressBar;
@synthesize leadingStackView = _leadingStackView;
@synthesize leadingStackViewButtons = _leadingStackViewButtons;
@synthesize backButton = _backButton;
@synthesize forwardButton = _forwardButton;
@synthesize tabGridButton = _tabGridButton;
@synthesize stopButton = _stopButton;
@synthesize reloadButton = _reloadButton;
@synthesize locationBarContainer = _locationBarContainer;
@synthesize trailingStackView = _trailingStackView;
@synthesize trailingStackViewButtons = _trailingStackViewButtons;
@synthesize shareButton = _shareButton;
@synthesize toolsMenuButton = _toolsMenuButton;
@synthesize cancelButton = _cancelButton;
@synthesize collapsedToolbarButton = _collapsedToolbarButton;
@synthesize expandedConstraints = _expandedConstraints;
@synthesize contractedConstraints = _contractedConstraints;
@synthesize contractedNoMarginConstraints = _contractedNoMarginConstraints;
@synthesize contentView = _contentView;

#pragma mark - Public

- (instancetype)initWithButtonFactory:(ToolbarButtonFactory*)factory {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _buttonFactory = factory;
  }
  return self;
}

- (void)setUp {
  if (self.subviews.count > 0) {
    // Setup the view only once.
    return;
  }
  DCHECK(self.buttonFactory);

  self.translatesAutoresizingMaskIntoConstraints = NO;

  [self setUpToolbarBackground];
  [self setUpLeadingStackView];
  [self setUpTrailingStackView];
  [self setUpCancelButton];
  [self setUpLocationBar];
  [self setUpProgressBar];
  [self setUpCollapsedToolbarButton];
  [self setUpSeparator];

  [self setUpConstraints];
}

- (void)setHidden:(BOOL)hidden {
  [super setHidden:hidden];
}

- (void)addFakeOmniboxTarget {
  self.fakeOmniboxTarget = [[UIView alloc] init];
  self.fakeOmniboxTarget.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:self.fakeOmniboxTarget];
  AddSameConstraints(self.locationBarContainer, self.fakeOmniboxTarget);
}

- (void)removeFakeOmniboxTarget {
  [self.fakeOmniboxTarget removeFromSuperview];
  self.fakeOmniboxTarget = nil;
}

#pragma mark - UIView

- (CGSize)intrinsicContentSize {
  return CGSizeMake(
      UIViewNoIntrinsicMetric,
      ToolbarExpandedHeight(self.traitCollection.preferredContentSizeCategory));
}

#pragma mark - Setup

// Sets up the toolbar background.
- (void)setUpToolbarBackground {

  if (IsVivaldiRunning()) {
    self.backgroundColor = [UIColor colorNamed:vNTPBackgroundColor];
  } else {
  self.backgroundColor =
      self.buttonFactory.toolbarConfiguration.backgroundColor;
  } // End Vivaldi

  self.contentView = self;
}

// Sets the cancel button to stop editing the location bar.
- (void)setUpCancelButton {
  self.cancelButton = [self.buttonFactory cancelButton];
  self.cancelButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:self.cancelButton];
}

// Sets the location bar container and its view if present.
- (void)setUpLocationBar {
  self.locationBarContainer = [[UIView alloc] init];

  if (IsVivaldiRunning()) {
    self.locationBarContainer.backgroundColor =
      [UIColor colorNamed:vNTPBackgroundColor];;
  } else {
  self.locationBarContainer.backgroundColor =
      [self.buttonFactory.toolbarConfiguration
          locationBarBackgroundColorWithVisibility:1];
  } // End Vivaldi

  [self.locationBarContainer
      setContentHuggingPriority:UILayoutPriorityDefaultLow
                        forAxis:UILayoutConstraintAxisHorizontal];
  self.locationBarContainer.translatesAutoresizingMaskIntoConstraints = NO;

  // The location bar shouldn't have vibrancy.
  [self addSubview:self.locationBarContainer];
}

// Sets the leading stack view.
- (void)setUpLeadingStackView {
  self.backButton = [self.buttonFactory backButton];
  self.forwardButton = [self.buttonFactory forwardButton];
  self.stopButton = [self.buttonFactory stopButton];
  self.stopButton.hiddenInCurrentState = YES;
  self.reloadButton = [self.buttonFactory reloadButton];

  if (IsVivaldiRunning()) {
    self.leadingStackViewButtons = [self buttonsForLeadingStackView];
  } else {
  self.leadingStackViewButtons = @[
    self.backButton, self.forwardButton, self.stopButton, self.reloadButton
  ];
  } // End Vivaldi

  self.leadingStackView = [[UIStackView alloc]
      initWithArrangedSubviews:self.leadingStackViewButtons];
  self.leadingStackView.translatesAutoresizingMaskIntoConstraints = NO;
  self.leadingStackView.spacing = kAdaptiveToolbarStackViewSpacing;
  [self.leadingStackView
      setContentHuggingPriority:UILayoutPriorityDefaultHigh
                        forAxis:UILayoutConstraintAxisHorizontal];

  [self.contentView addSubview:self.leadingStackView];
}

// Sets the trailing stack view.
- (void)setUpTrailingStackView {
  self.shareButton = [self.buttonFactory shareButton];
  self.tabGridButton = [self.buttonFactory tabGridButton];
  self.toolsMenuButton = [self.buttonFactory toolsMenuButton];

  if (IsVivaldiRunning()) {
    self.trailingStackViewButtons = [self buttonsForTrailingStackView];
  } else {
  self.trailingStackViewButtons =
      @[ self.shareButton, self.tabGridButton, self.toolsMenuButton ];
  } // End Vivaldi

  self.trailingStackView = [[UIStackView alloc]
      initWithArrangedSubviews:self.trailingStackViewButtons];
  self.trailingStackView.translatesAutoresizingMaskIntoConstraints = NO;
  self.trailingStackView.spacing = kAdaptiveToolbarStackViewSpacing;
  [self.trailingStackView
      setContentHuggingPriority:UILayoutPriorityDefaultHigh
                        forAxis:UILayoutConstraintAxisHorizontal];

  [self.contentView addSubview:self.trailingStackView];
}

// Sets the progress bar up.
- (void)setUpProgressBar {
  self.progressBar = [[ToolbarProgressBar alloc] init];
  self.progressBar.translatesAutoresizingMaskIntoConstraints = NO;
  self.progressBar.hidden = YES;
  [self addSubview:self.progressBar];
}

// Sets the collapsedToolbarButton up.
- (void)setUpCollapsedToolbarButton {
  self.collapsedToolbarButton = [[UIButton alloc] init];
  self.collapsedToolbarButton.translatesAutoresizingMaskIntoConstraints = NO;
  self.collapsedToolbarButton.hidden = YES;
  [self addSubview:self.collapsedToolbarButton];
}

// Sets the separator up.
- (void)setUpSeparator {
  self.separator = [[UIView alloc] init];
  self.separator.backgroundColor = [UIColor colorNamed:kToolbarShadowColor];
  self.separator.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:self.separator];
  if (IsVivaldiRunning() &&
      (GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE))
    self.separator.backgroundColor = UIColor.clearColor;
}

// Sets the constraints up.
- (void)setUpConstraints {
  id<LayoutGuideProvider> safeArea = self.safeAreaLayoutGuide;
  self.expandedConstraints = [NSMutableArray array];
  self.contractedConstraints = [NSMutableArray array];
  self.contractedNoMarginConstraints = [NSMutableArray array];

  // Separator constraints.
  [NSLayoutConstraint activateConstraints:@[
    [self.separator.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [self.separator.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
    [self.separator.topAnchor constraintEqualToAnchor:self.bottomAnchor],
    [self.separator.heightAnchor
        constraintEqualToConstant:ui::AlignValueToUpperPixel(
                                      kToolbarSeparatorHeight)],
  ]];

  if (IsVivaldiRunning()) {
    [NSLayoutConstraint activateConstraints:@[
      [self.leadingStackView.leadingAnchor
          constraintEqualToAnchor:safeArea.leadingAnchor
                         constant:vPrimaryToolbarHorizontalPadding],
      [self.leadingStackView.centerYAnchor
          constraintEqualToAnchor:self.locationBarContainer.centerYAnchor],
      [self.leadingStackView.heightAnchor
          constraintEqualToConstant:kAdaptiveToolbarButtonHeight],
    ]];
  } else {
  // Leading StackView constraints.
  [NSLayoutConstraint activateConstraints:@[
    [self.leadingStackView.leadingAnchor
        constraintEqualToAnchor:safeArea.leadingAnchor
                       constant:kAdaptiveToolbarMargin],
    [self.leadingStackView.centerYAnchor
        constraintEqualToAnchor:self.locationBarContainer.centerYAnchor],
    [self.leadingStackView.heightAnchor
        constraintEqualToConstant:kAdaptiveToolbarButtonHeight],
  ]];
  } // End Vivaldi

  // LocationBar constraints. The constant value is set by the VC.
  self.locationBarContainerHeight =
      [self.locationBarContainer.heightAnchor constraintEqualToConstant:0];
  self.locationBarBottomConstraint = [self.locationBarContainer.bottomAnchor
      constraintEqualToAnchor:self.bottomAnchor];

  [NSLayoutConstraint activateConstraints:@[
    self.locationBarBottomConstraint,
    self.locationBarContainerHeight,
  ]];

  if (IsVivaldiRunning()) {
    [self.contractedConstraints addObjectsFromArray:@[
      [self.locationBarContainer.trailingAnchor
          constraintEqualToAnchor:self.trailingStackView.leadingAnchor
                         constant:-kContractedLocationBarHorizontalMargin],
      [self.locationBarContainer.leadingAnchor
          constraintEqualToAnchor:self.leadingStackView.trailingAnchor
                         constant:vLocationBarLeadingPadding],
    ]];
  } else {
  [self.contractedConstraints addObjectsFromArray:@[
    [self.locationBarContainer.trailingAnchor
        constraintEqualToAnchor:self.trailingStackView.leadingAnchor
                       constant:-kContractedLocationBarHorizontalMargin],
    [self.locationBarContainer.leadingAnchor
        constraintEqualToAnchor:self.leadingStackView.trailingAnchor
                       constant:kContractedLocationBarHorizontalMargin],
  ]];
  } // End Vivaldi

  if (IsVivaldiRunning()) {
    [self.contractedNoMarginConstraints addObjectsFromArray:@[
      [self.locationBarContainer.trailingAnchor
          constraintEqualToAnchor:self.trailingStackView.leadingAnchor],
      [self.locationBarContainer.leadingAnchor
          constraintEqualToAnchor:self.leadingStackView.trailingAnchor],
    ]];

    [self.expandedConstraints addObjectsFromArray:@[
      [self.locationBarContainer.trailingAnchor
          constraintEqualToAnchor:self.cancelButton.leadingAnchor],
      [self.locationBarContainer.leadingAnchor
          constraintEqualToAnchor:safeArea.leadingAnchor
                         constant:vLocationBarLeadingPadding]
    ]];

    // Trailing StackView constraints.
    [NSLayoutConstraint activateConstraints:@[
      [self.trailingStackView.trailingAnchor
          constraintEqualToAnchor:safeArea.trailingAnchor
                         constant:-vPrimaryToolbarHorizontalPadding],
      [self.trailingStackView.centerYAnchor
          constraintEqualToAnchor:self.locationBarContainer.centerYAnchor],
      [self.trailingStackView.heightAnchor
          constraintEqualToConstant:kAdaptiveToolbarButtonHeight],
    ]];
  } else {

  // Constraints for contractedNoMarginConstraints.
  [self.contractedNoMarginConstraints addObjectsFromArray:@[
    [self.locationBarContainer.leadingAnchor
        constraintEqualToAnchor:safeArea.leadingAnchor
                       constant:kExpandedLocationBarHorizontalMargin],
    [self.locationBarContainer.trailingAnchor
        constraintEqualToAnchor:safeArea.trailingAnchor
                       constant:-kExpandedLocationBarHorizontalMargin]
  ]];

  [self.expandedConstraints addObjectsFromArray:@[
    [self.locationBarContainer.trailingAnchor
        constraintEqualToAnchor:self.cancelButton.leadingAnchor],
    [self.locationBarContainer.leadingAnchor
        constraintEqualToAnchor:safeArea.leadingAnchor
                       constant:kExpandedLocationBarHorizontalMargin]
  ]];

  // Trailing StackView constraints.
  [NSLayoutConstraint activateConstraints:@[
    [self.trailingStackView.trailingAnchor
        constraintEqualToAnchor:safeArea.trailingAnchor
                       constant:-kAdaptiveToolbarMargin],
    [self.trailingStackView.centerYAnchor
        constraintEqualToAnchor:self.locationBarContainer.centerYAnchor],
    [self.trailingStackView.heightAnchor
        constraintEqualToConstant:kAdaptiveToolbarButtonHeight],
  ]];
  } // End Vivaldi

  // locationBarView constraints, if present.
  if (self.locationBarView) {
    AddSameConstraints(self.locationBarView, self.locationBarContainer);
  }

  // Cancel button constraints.
  [NSLayoutConstraint activateConstraints:@[
    [self.cancelButton.topAnchor
        constraintEqualToAnchor:self.trailingStackView.topAnchor],
    [self.cancelButton.bottomAnchor
        constraintEqualToAnchor:self.trailingStackView.bottomAnchor],
  ]];
  NSLayoutConstraint* visibleCancel = [self.cancelButton.trailingAnchor
      constraintEqualToAnchor:safeArea.trailingAnchor
                     constant:-kExpandedLocationBarHorizontalMargin];
  NSLayoutConstraint* hiddenCancel = [self.cancelButton.leadingAnchor
      constraintEqualToAnchor:self.trailingAnchor];
  [self.expandedConstraints addObject:visibleCancel];
  [self.contractedConstraints addObject:hiddenCancel];
  [self.contractedNoMarginConstraints addObject:hiddenCancel];

  // ProgressBar constraints.
  [NSLayoutConstraint activateConstraints:@[
    [self.progressBar.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [self.progressBar.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor],
    [self.progressBar.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    [self.progressBar.heightAnchor
        constraintEqualToConstant:kProgressBarHeight],
  ]];

  // CollapsedToolbarButton constraints.
  AddSameConstraints(self, self.collapsedToolbarButton);
}

#pragma mark - AdaptiveToolbarView

- (void)setLocationBarView:(UIView*)locationBarView {
  if (_locationBarView == locationBarView) {
    return;
  }

  if ([_locationBarView superview] == self.locationBarContainer) {
    [_locationBarView removeFromSuperview];
  }

  _locationBarView = locationBarView;
  locationBarView.translatesAutoresizingMaskIntoConstraints = NO;
  [locationBarView setContentHuggingPriority:UILayoutPriorityDefaultLow
                                     forAxis:UILayoutConstraintAxisHorizontal];

  if (!self.locationBarContainer || !locationBarView)
    return;

  [self.locationBarContainer addSubview:locationBarView];
  AddSameConstraints(self.locationBarView, self.locationBarContainer);
  [self.locationBarContainer.trailingAnchor
      constraintGreaterThanOrEqualToAnchor:self.locationBarView.trailingAnchor]
      .active = YES;
}

- (NSArray<ToolbarButton*>*)allButtons {
  if (!_allButtons) {
    _allButtons = [self.leadingStackViewButtons
        arrayByAddingObjectsFromArray:self.trailingStackViewButtons];
  }
  return _allButtons;
}

- (ToolbarButton*)openNewTabButton {
  return nil;
}

#pragma mark: - Vivaldi
- (void)redrawToolbarButtons {
  [self redrawLeadingStackView];
  [self redrawTrailingStackView];
}

- (void)redrawLeadingStackView {
  self.leadingStackViewButtons = [self buttonsForLeadingStackView];
  [self redrawStackView:self.leadingStackView
            withButtons:self.leadingStackViewButtons];
}

- (void)redrawTrailingStackView {
  self.trailingStackViewButtons = [self buttonsForTrailingStackView];
  [self redrawStackView:self.trailingStackView
            withButtons:self.trailingStackViewButtons];
}

- (NSArray*)buttonsForLeadingStackView {
  BOOL isPhone = GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE;
  BOOL isVerticalRegular = VivaldiGlobalHelpers.isVerticalTraitRegular;
  BOOL isHorizontalNotRegular = !VivaldiGlobalHelpers.isHorizontalTraitRegular;

  if (isPhone || !VivaldiGlobalHelpers.canShowSidePanel) {
    if (isHorizontalNotRegular && isVerticalRegular) {
      self.shieldButton = [self.buttonFactory shieldButton];
      return @[ self.shieldButton ];
    } else {
      return @[
        self.backButton,
        self.forwardButton,
        self.stopButton,
        self.reloadButton
      ];
    }
  } else {
    self.panelButton = [self.buttonFactory panelButton];
    self.shieldButton = [self.buttonFactory shieldButton];
    return @[
      self.panelButton,
      self.backButton,
      self.forwardButton,
      self.stopButton,
      self.reloadButton,
      self.shieldButton
    ];
  }
}

- (NSArray*)buttonsForTrailingStackView {
  BOOL isPhone = GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE;
  BOOL isVerticalRegular = VivaldiGlobalHelpers.isVerticalTraitRegular;
  BOOL isHorizontalNotRegular = !VivaldiGlobalHelpers.isHorizontalTraitRegular;

  if (isPhone || !VivaldiGlobalHelpers.canShowSidePanel) {
    if (isHorizontalNotRegular && isVerticalRegular) {
      return @[ self.toolsMenuButton ];
    } else {
      self.vivaldiMoreButton = [self.buttonFactory vivaldiMoreButton];
      return @[
        self.vivaldiMoreButton,
        self.tabGridButton,
        self.toolsMenuButton
      ];
    }
  } else {
    return @[
      self.tabGridButton,
      self.toolsMenuButton
    ];
  }
}

- (void)redrawStackView:(UIStackView*)stackView
            withButtons:(NSArray*)buttons {

  // Remove all subviews
  for (UIView *view in stackView.arrangedSubviews) {
    [stackView removeArrangedSubview:view];
    [view removeFromSuperview];
  }

  // Add new buttons
  for (UIView *button in buttons) {
    [stackView addArrangedSubview:button];
  }
}

- (void)handleToolbarButtonVisibility:(BOOL)show {
  self.leadingStackView.hidden = !show;
  self.trailingStackView.hidden = !show;
}

- (void)setVivaldiMoreActionItemsWithShareState:(BOOL)enabled
                              atbSettingType:(ATBSettingType)type {
  self.vivaldiMoreButton.menu =
      [self.buttonFactory contextMenuForMoreWithAllButtons:enabled
                                            atbSettingType:type];
}

- (void)reloadButtonsWithNewTabPage:(BOOL)isNewTabPage
                  desktopTabEnabled:(BOOL)desktopTabEnabled {
  // No op.
}

- (void)updateVivaldiShieldState:(ATBSettingType)setting {
  switch (setting) {
    case ATBSettingNoBlocking: {
      UIImage* noBlocking =
        [[UIImage imageNamed:vATBShieldNone]
            imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      [self.shieldButton setImage:noBlocking forState:UIControlStateNormal];
      break;
    }
    case ATBSettingBlockTrackers: {
      UIImage* trackersBlocking =
        [[UIImage imageNamed:vATBShieldTrackers]
            imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      [self.shieldButton setImage:trackersBlocking
                            forState:UIControlStateNormal];
      break;
    }
    case ATBSettingBlockTrackersAndAds: {
      UIImage* allBlocking =
        [[UIImage imageNamed:vATBShieldTrackesAndAds]
            imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      [self.shieldButton setImage:allBlocking forState:UIControlStateNormal];
      break;
    }
    default: {
      UIImage* trackersBlocking =
        [[UIImage imageNamed:vATBShieldNone]
            imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      [self.shieldButton setImage:trackersBlocking
                            forState:UIControlStateNormal];
      break;
    }
  }
}

@end
