// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_view.h"

#import "base/check.h"
#import "base/notreached.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tab_grid_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tab_group_state.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_progress_bar.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ui/gfx/ios/uikit_util.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "ios/ui/toolbar/vivaldi_toolbar_constants.h"

using vivaldi::IsVivaldiRunning;
// End Vivaldi

namespace {
const CGFloat kToolsMenuOffset = -7;

// Button shown when the view is collapsed to exit fullscreen.
UIButton* SecondaryToolbarCollapsedToolbarButton() {
  UIButton* collapsedToolbarButton = [[UIButton alloc] init];
  collapsedToolbarButton.translatesAutoresizingMaskIntoConstraints = NO;
  collapsedToolbarButton.hidden = YES;
  return collapsedToolbarButton;
}

// Container for the location bar view.
UIView* SecondaryToolbarLocationBarContainerView(
    ToolbarButtonFactory* buttonFactory) {
  UIView* locationBarContainer = [[UIView alloc] init];
  locationBarContainer.translatesAutoresizingMaskIntoConstraints = NO;

  if (!IsVivaldiRunning()) {
  locationBarContainer.backgroundColor = [buttonFactory.toolbarConfiguration
      locationBarBackgroundColorWithVisibility:1];
  } // End Vivaldi

  [locationBarContainer
      setContentHuggingPriority:UILayoutPriorityDefaultLow
                        forAxis:UILayoutConstraintAxisHorizontal];
  return locationBarContainer;
}

}  // namespace

@interface SecondaryToolbarView ()
// Factory used to create the buttons.
@property(nonatomic, strong) ToolbarButtonFactory* buttonFactory;

// Redefined as readwrite
@property(nonatomic, strong, readwrite) NSArray<ToolbarButton*>* allButtons;

// Separator above the toolbar, redefined as readwrite.
@property(nonatomic, strong, readwrite) UIView* separator;
// Progress bar displayed below the toolbar, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarProgressBar* progressBar;

// The stack view containing the buttons, redefined as readwrite.
@property(nonatomic, strong, readwrite) UIStackView* buttonStackView;

// Button to navigate back, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* backButton;
// Buttons to navigate forward, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* forwardButton;
// Button to display the tools menu, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* toolsMenuButton;
// Button to display the tab grid, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarTabGridButton* tabGridButton;
// Button to create a new tab, redefined as readwrite.
@property(nonatomic, strong, readwrite) ToolbarButton* openNewTabButton;
// Separator below the location bar. Used when collapsed above the keyboard,
// redefined as readwrite.
@property(nonatomic, strong, readwrite) UIView* bottomSeparator;

#pragma mark** Location bar. **
// Location bar containing the omnibox.
@property(nonatomic, strong) UIView* locationBarView;
// Container for the location bar, redefined as readwrite.
@property(nonatomic, strong, readwrite) UIView* locationBarContainer;
// The height of the container for the location bar, redefined as readwrite.
@property(nonatomic, strong, readwrite)
    NSLayoutConstraint* locationBarContainerHeight;
// Button taking the full size of the toolbar. Expands the toolbar when tapped,
// redefined as readwrite.
@property(nonatomic, strong, readwrite) UIButton* collapsedToolbarButton;

// Vivaldi
// Button to display the panels.
@property(nonatomic, strong, readwrite) ToolbarButton* panelButton;
// Search button on the new tab page.
@property(nonatomic, strong, readwrite) ToolbarButton* searchButton;
// Home button on page.
@property(nonatomic, strong, readwrite) ToolbarButton* homeButton;
// End Vivaldi

@end

@implementation SecondaryToolbarView {
  // Constraint between `self.topAnchor` and `buttonStackView.topAnchor`. Active
  // when the omnibox is not in the bottom toolbar.
  NSLayoutConstraint* _buttonStackViewNoOmniboxConstraint;
  // Constraint between `locationBarContainer.bottomAnchor` and
  // `buttonStackView.topAnchor`. Active when the omnibox is in the bottom
  // toolbar.
  NSLayoutConstraint* _locationBarBottomConstraint;

  // Visual effect view to make the toolbar appear translucent when necessary.
  UIVisualEffectView* _visualEffectView;
  // Content view to hold the main toolbar content above the visual effect view.
  UIView* _contentView;
}

@synthesize allButtons = _allButtons;
@synthesize backButton = _backButton;
@synthesize buttonFactory = _buttonFactory;
@synthesize buttonStackView = _buttonStackView;
@synthesize collapsedToolbarButton = _collapsedToolbarButton;
@synthesize forwardButton = _forwardButton;
@synthesize locationBarContainer = _locationBarContainer;
@synthesize locationBarContainerHeight = _locationBarContainerHeight;
@synthesize openNewTabButton = _openNewTabButton;
@synthesize progressBar = _progressBar;
@synthesize toolsMenuButton = _toolsMenuButton;
@synthesize tabGridButton = _tabGridButton;

// Vivaldi
@synthesize bottomOmniboxEnabled = _bottomOmniboxEnabled;
@synthesize tabBarEnabled = _tabBarEnabled;
@synthesize canShowBack = _canShowBack;
@synthesize canShowForward = _canShowForward;
@synthesize canShowAdTrackerBlocker = _canShowAdTrackerBlocker;
@synthesize homePageEnabled = _homePageEnabled;
// End Vivaldi

#pragma mark - Public

- (instancetype)initWithButtonFactory:(ToolbarButtonFactory*)factory {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _buttonFactory = factory;
    [self setUp];
  }
  return self;
}

- (void)makeTranslucent {
  // Note:(prio@vivaldi.com) - We set background color outside of this view
  // based on accent color. Skip this upstream override.
  if (!IsVivaldiRunning()) {
  _visualEffectView.hidden = NO;
  _contentView.backgroundColor = nil;
  } // End Vivaldi
}

- (void)makeOpaque {

  // Note:(prio@vivaldi.com) - We set background color outside of this view
  // based on accent color. Skip this upstream override.
  if (!IsVivaldiRunning()) {
  _visualEffectView.hidden = YES;
  _contentView.backgroundColor =
      self.buttonFactory.toolbarConfiguration.backgroundColor;
  } // End Vivaldi

}

#pragma mark - UIView

- (CGSize)intrinsicContentSize {
  return CGSizeMake(UIViewNoIntrinsicMetric,
                    kSecondaryToolbarWithoutOmniboxHeight);
}

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  if (IsBottomOmniboxAvailable() && newSuperview) {
    _locationBarKeyboardConstraint.active = NO;

    // UIKeyboardLayoutGuide is updated sooner in superview's
    // keyboardLayoutGuide rendering smoother animation. Constraint is
    // updated
    // in view controller.
    _locationBarKeyboardConstraint = [newSuperview.keyboardLayoutGuide.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.locationBarContainer
                                                 .topAnchor];
  }
}

#pragma mark - Setup

// Sets all the subviews and constraints of the view.
- (void)setUp {
  if (self.subviews.count > 0) {
    // Make sure the view is instantiated only once.
    return;
  }
  DCHECK(self.buttonFactory);

  self.translatesAutoresizingMaskIntoConstraints = NO;

  UIBlurEffect* blurEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleRegular];
  _visualEffectView = [[UIVisualEffectView alloc] initWithEffect:blurEffect];
  _visualEffectView.translatesAutoresizingMaskIntoConstraints = NO;
  _visualEffectView.hidden = YES;

  [self addSubview:_visualEffectView];
  AddSameConstraints(self, _visualEffectView);

  _contentView = [[UIView alloc] init];
  _contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_contentView];
  AddSameConstraints(self, _contentView);

  // Note:(prio@vivaldi.com) - We set background color outside of this view
  // based on accent color. Skip this upstream override.
  if (!IsVivaldiRunning()) {
  _contentView.backgroundColor =
      self.buttonFactory.toolbarConfiguration.backgroundColor;
  } // End Vivaldi

  UIView* contentView = _contentView;

  // Toolbar buttons.
  self.backButton = [self.buttonFactory backButton];
  self.forwardButton = [self.buttonFactory forwardButton];
  self.openNewTabButton = [self.buttonFactory openNewTabButton];
  self.tabGridButton = [self.buttonFactory tabGridButton];
  self.toolsMenuButton = [self.buttonFactory toolsMenuButton];

  // Vivaldi
  self.panelButton = [self.buttonFactory panelButton];
  self.searchButton = [self.buttonFactory vivaldiSearchButton];
  self.homeButton = [self.buttonFactory vivaldiHomeButton];
  // End Vivaldi

  // Move the tools menu button such as it looks visually balanced with the
  // button on the other side of the toolbar.
  NSInteger textDirection = base::i18n::IsRTL() ? -1 : 1;

  if (IsVivaldiRunning()) {
    self.allButtons = @[
      self.panelButton,
      self.backButton,
      self.homeButton,
      self.openNewTabButton,
      self.forwardButton,
      self.tabGridButton
    ];
  } else {
  self.toolsMenuButton.transform =
      CGAffineTransformMakeTranslation(textDirection * kToolsMenuOffset, 0);

  self.allButtons = @[
    self.backButton, self.forwardButton, self.openNewTabButton,
    self.tabGridButton, self.toolsMenuButton
  ];
  } // End Vivaldi

  // Separator.
  self.separator = [[UIView alloc] init];
  self.separator.backgroundColor = [UIColor colorNamed:kToolbarShadowColor];
  self.separator.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:self.separator];

  // Button StackView.
  self.buttonStackView =
      [[UIStackView alloc] initWithArrangedSubviews:self.allButtons];
  self.buttonStackView.distribution = UIStackViewDistributionEqualSpacing;
  self.buttonStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:self.buttonStackView];

  UILayoutGuide* safeArea = self.safeAreaLayoutGuide;

  if (IsBottomOmniboxAvailable()) {
    self.collapsedToolbarButton = SecondaryToolbarCollapsedToolbarButton();
    self.locationBarContainer =
        SecondaryToolbarLocationBarContainerView(self.buttonFactory);

    // Add locationBarContainer below buttons as it might move under the
    // buttons.
    [contentView insertSubview:self.locationBarContainer
                  belowSubview:self.buttonStackView];

    // Put `collapsedToolbarButton` on top of everything.
    [contentView addSubview:self.collapsedToolbarButton];
    [contentView bringSubviewToFront:self.collapsedToolbarButton];
    AddSameConstraints(self, self.collapsedToolbarButton);

    // Add progress bar on the top edge.
    _progressBar = [[ToolbarProgressBar alloc] init];
    _progressBar.translatesAutoresizingMaskIntoConstraints = NO;
    _progressBar.hidden = YES;
    [_progressBar.heightAnchor constraintEqualToConstant:kProgressBarHeight]
        .active = YES;
    [contentView addSubview:_progressBar];
    AddSameConstraintsToSides(
        self, _progressBar,
        LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);

    // LocationBarView constraints.
    if (self.locationBarView) {
      AddSameConstraints(self.locationBarView, self.locationBarContainer);
    }

    // Height of location bar, constant controlled by view controller.
    self.locationBarContainerHeight =
        [self.locationBarContainer.heightAnchor constraintEqualToConstant:0];
    // Top margin of location bar, constant controlled by view controller.
    self.locationBarTopConstraint = [self.locationBarContainer.topAnchor
        constraintEqualToAnchor:self.topAnchor];

    if (IsVivaldiRunning()) {
      self.buttonStackView.backgroundColor = UIColor.clearColor;
      _locationBarBottomConstraint = [self.buttonStackView.topAnchor
          constraintEqualToAnchor:self.locationBarContainer.bottomAnchor
                         constant:vBottomAdaptiveLocationBarBottomMargin];

      _buttonStackViewNoOmniboxConstraint = [self.buttonStackView.topAnchor
          constraintEqualToAnchor:self.topAnchor
                         constant:vBottomButtonsTopMargin];
    } else {
    _locationBarBottomConstraint = [self.buttonStackView.topAnchor
        constraintEqualToAnchor:self.locationBarContainer.bottomAnchor
                       constant:kBottomAdaptiveLocationBarBottomMargin];

    _buttonStackViewNoOmniboxConstraint = [self.buttonStackView.topAnchor
        constraintEqualToAnchor:self.topAnchor
                       constant:kBottomButtonsTopMargin];
    } // End Vivaldi

    [self updateButtonStackViewConstraint];

    // Bottom separator used when collapsed above the keyboard.
    self.bottomSeparator = [[UIView alloc] init];
    self.bottomSeparator.backgroundColor =
        [UIColor colorNamed:kToolbarShadowColor];
    self.bottomSeparator.translatesAutoresizingMaskIntoConstraints = NO;
    self.bottomSeparator.alpha = 0.0;
    [contentView addSubview:self.bottomSeparator];
    AddSameConstraintsToSides(self, self.bottomSeparator,
                              LayoutSides::kLeading | LayoutSides::kTrailing);

    if (IsVivaldiRunning()) {
      [NSLayoutConstraint activateConstraints:@[
        self.locationBarTopConstraint,
        self.locationBarContainerHeight,
        [self.locationBarContainer.leadingAnchor
            constraintEqualToAnchor:safeArea.leadingAnchor],
        [self.locationBarContainer.trailingAnchor
            constraintEqualToAnchor:safeArea.trailingAnchor],
        [self.buttonStackView.topAnchor
            constraintGreaterThanOrEqualToAnchor:self.topAnchor
                                        constant:kBottomButtonsTopMargin],
        [self.bottomSeparator.heightAnchor
            constraintEqualToConstant:ui::AlignValueToUpperPixel(
                                          kToolbarSeparatorHeight)],
        [self.bottomSeparator.bottomAnchor
            constraintEqualToAnchor:self.locationBarContainer.bottomAnchor],
      ]];
    } else {
    [NSLayoutConstraint activateConstraints:@[
      self.locationBarTopConstraint,
      self.locationBarContainerHeight,
      [self.locationBarContainer.leadingAnchor
          constraintEqualToAnchor:safeArea.leadingAnchor
                         constant:kExpandedLocationBarHorizontalMargin],
      [self.locationBarContainer.trailingAnchor
          constraintEqualToAnchor:safeArea.trailingAnchor
                         constant:-kExpandedLocationBarHorizontalMargin],
      [self.buttonStackView.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.topAnchor
                                      constant:kBottomButtonsTopMargin],
      [self.bottomSeparator.heightAnchor
          constraintEqualToConstant:ui::AlignValueToUpperPixel(
                                        kToolbarSeparatorHeight)],
      [self.bottomSeparator.bottomAnchor
          constraintEqualToAnchor:self.locationBarContainer.bottomAnchor],
    ]];
    } // End Vivaldi

  } else {  // Bottom omnibox flag disabled.
    [self.buttonStackView.topAnchor
        constraintEqualToAnchor:self.topAnchor
                       constant:kBottomButtonsTopMargin]
        .active = YES;
  }

  if (IsVivaldiRunning()) {
    [NSLayoutConstraint activateConstraints:@[
      [self.buttonStackView.leadingAnchor
          constraintEqualToAnchor:safeArea.leadingAnchor
                         constant:vAdaptiveToolbarMargin],
      [self.buttonStackView.trailingAnchor
          constraintEqualToAnchor:safeArea.trailingAnchor
                         constant:-vAdaptiveToolbarMargin],

      [self.separator.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [self.separator.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
      [self.separator.bottomAnchor constraintEqualToAnchor:self.topAnchor],
      [self.separator.heightAnchor
          constraintEqualToConstant:ui::AlignValueToUpperPixel(
                                        kToolbarSeparatorHeight)],
    ]];
  } else {
  [NSLayoutConstraint activateConstraints:@[
    [self.buttonStackView.leadingAnchor
        constraintEqualToAnchor:safeArea.leadingAnchor
                       constant:kAdaptiveToolbarMargin],
    [self.buttonStackView.trailingAnchor
        constraintEqualToAnchor:safeArea.trailingAnchor
                       constant:-kAdaptiveToolbarMargin],

    [self.separator.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [self.separator.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
    [self.separator.bottomAnchor constraintEqualToAnchor:self.topAnchor],
    [self.separator.heightAnchor
        constraintEqualToConstant:ui::AlignValueToUpperPixel(
                                      kToolbarSeparatorHeight)],
  ]];
  } // End Vivaldi

}

#pragma mark - AdaptiveToolbarView

- (ToolbarButton*)stopButton {
  return nil;
}

- (ToolbarButton*)reloadButton {
  return nil;
}

- (ToolbarButton*)shareButton {
  return nil;
}

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

  [self updateButtonStackViewConstraint];
  if (!self.locationBarContainer || !locationBarView) {
    return;
  }

  [self.locationBarContainer addSubview:locationBarView];
  AddSameConstraints(self.locationBarView, self.locationBarContainer);
}

- (void)updateTabGroupState:(ToolbarTabGroupState)tabGroupState {
  const BOOL inGroup = tabGroupState == ToolbarTabGroupState::kTabGroup;
  self.openNewTabButton.accessibilityLabel =
      [self.buttonFactory.toolbarConfiguration
          accessibilityLabelForOpenNewTabButtonInGroup:inGroup];
  self.tabGridButton.tabGroupState = tabGroupState;
}

#pragma mark - Private

/// Updates `buttonStackView.topAnchor` constraints when adding/removing the
/// location bar.
- (void)updateButtonStackViewConstraint {
  // Reset `buttonStackView` top constraints.
  _locationBarBottomConstraint.active = NO;
  _buttonStackViewNoOmniboxConstraint.active = NO;

  // Set the correct constraint for `buttonStackView.topAnchor`.
  if (self.locationBarView) {
    _locationBarBottomConstraint.active = YES;
  } else {
    _buttonStackViewNoOmniboxConstraint.active = YES;
  }
}

#pragma mark: - Vivaldi
- (void)reloadButtonsWithNewTabPage:(BOOL)isNewTabPage
                  desktopTabEnabled:(BOOL)desktopTabEnabled {
  NSMutableArray *buttons = [NSMutableArray arrayWithObjects:
                             self.panelButton,
                             self.backButton,
                             nil];
  // Determine the homebutton/search/newtab button based on conditions
  UIButton *middleButton = nil;
  if (isNewTabPage) {
    middleButton = self.searchButton;
  } else if (desktopTabEnabled) {
    middleButton = _homePageEnabled ? self.homeButton : self.searchButton;
  } else if (_bottomOmniboxEnabled) {
    middleButton = self.openNewTabButton;
  } else {
    middleButton = _homePageEnabled ? self.homeButton : self.openNewTabButton;
  }

  [buttons addObject:middleButton];

  // Add the remaining common buttons
  [buttons addObjectsFromArray:@[self.forwardButton, self.tabGridButton]];

  // Remove old buttons from the stack view
  for (UIView *button in self.allButtons) {
    [self.buttonStackView removeArrangedSubview:button];
    [button removeFromSuperview];
  }

  // Add new buttons to the stack view
  for (ToolbarButton *button in buttons) {
    [button checkImageVisibility];
    [self.buttonStackView addArrangedSubview:button];
  }

  // Update the allButtons property
  self.allButtons = buttons;
}
// End Vivaldi

@end
