// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_view.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tab_grid_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tools_menu_button.h"
#import "ios/chrome/browser/ui/toolbar_container/toolbar_collapsing.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ui/gfx/ios/uikit_util.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_constants+vivaldi.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"

using vivaldi::IsVivaldiRunning;
// End Vivaldi

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kToolsMenuOffset = -7;
}  // namespace

@interface SecondaryToolbarView ()<ToolbarCollapsing>
// Factory used to create the buttons.
@property(nonatomic, strong) ToolbarButtonFactory* buttonFactory;

// Redefined as readwrite
@property(nonatomic, strong, readwrite) NSArray<ToolbarButton*>* allButtons;

// Separator above the toolbar, redefined as readwrite.
@property(nonatomic, strong, readwrite) UIView* separator;

// The stack view containing the buttons.
@property(nonatomic, strong) UIStackView* stackView;

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

// Vivaldi
// Button to display the panels.
@property(nonatomic, strong, readwrite) ToolbarButton* panelButton;
// Search button on the new tab page.
@property(nonatomic, strong, readwrite) ToolbarButton* searchButton;
// End Vivaldi

@end

@implementation SecondaryToolbarView

@synthesize allButtons = _allButtons;
@synthesize buttonFactory = _buttonFactory;
@synthesize stackView = _stackView;
@synthesize backButton = _backButton;
@synthesize forwardButton = _forwardButton;
@synthesize toolsMenuButton = _toolsMenuButton;
@synthesize openNewTabButton = _openNewTabButton;
@synthesize tabGridButton = _tabGridButton;

#pragma mark - Public

- (instancetype)initWithButtonFactory:(ToolbarButtonFactory*)factory {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _buttonFactory = factory;
    [self setUp];
  }
  return self;
}

#pragma mark - UIView

- (CGSize)intrinsicContentSize {
  return CGSizeMake(UIViewNoIntrinsicMetric, kSecondaryToolbarHeight);
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

  self.backgroundColor =
      self.buttonFactory.toolbarConfiguration.backgroundColor;

  UIView* contentView = self;

  self.backButton = [self.buttonFactory backButton];
  self.forwardButton = [self.buttonFactory forwardButton];
  self.openNewTabButton = [self.buttonFactory openNewTabButton];
  self.tabGridButton = [self.buttonFactory tabGridButton];
  self.toolsMenuButton = [self.buttonFactory toolsMenuButton];

  // Vivaldi
  self.panelButton = [self.buttonFactory panelButton];
  self.searchButton = [self.buttonFactory vivaldiSearchButton];
  // End Vivaldi

  // Move the tools menu button such as it looks visually balanced with the
  // button on the other side of the toolbar.
  NSInteger textDirection = base::i18n::IsRTL() ? -1 : 1;

  if (IsVivaldiRunning()) {
    self.allButtons = @[
      self.panelButton,
      self.backButton,
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

  self.separator = [[UIView alloc] init];
  self.separator.backgroundColor = [UIColor colorNamed:kToolbarShadowColor];
  self.separator.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:self.separator];

  self.stackView =
      [[UIStackView alloc] initWithArrangedSubviews:self.allButtons];
  self.stackView.distribution = UIStackViewDistributionEqualSpacing;
  self.stackView.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:self.stackView];

  id<LayoutGuideProvider> safeArea = self.safeAreaLayoutGuide;

  if (IsVivaldiRunning()) {
    [self.stackView anchorTop:self.topAnchor
                      leading:self.safeLeftAnchor
                       bottom:nil
                     trailing:self.safeRightAnchor
                      padding:UIEdgeInsetsMake(vSecondaryToolbarTopPadding,
                                               kAdaptiveToolbarMargin,
                                               0,
                                               kAdaptiveToolbarMargin)];
    [self.separator
      anchorTop:nil
        leading:self.leadingAnchor
         bottom:self.topAnchor
       trailing:self.trailingAnchor
           size:CGSizeMake(0,
                     ui::AlignValueToUpperPixel(kToolbarSeparatorHeight))];
  } else {
  [NSLayoutConstraint activateConstraints:@[
    [self.stackView.leadingAnchor
        constraintEqualToAnchor:safeArea.leadingAnchor
                       constant:kAdaptiveToolbarMargin],
    [self.stackView.trailingAnchor
        constraintEqualToAnchor:safeArea.trailingAnchor
                       constant:-kAdaptiveToolbarMargin],
    [self.stackView.topAnchor
        constraintEqualToAnchor:self.topAnchor
                       constant:kBottomButtonsBottomMargin],

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

- (MDCProgressView*)progressBar {
  return nil;
}

#pragma mark - ToolbarCollapsing

- (CGFloat)expandedToolbarHeight {
  return self.intrinsicContentSize.height;
}

- (CGFloat)collapsedToolbarHeight {
  return 0.0;
}

#pragma mark: - Vivaldi
- (void)reloadButtonsWithNewTabPage:(BOOL)isNewTabPage
                  desktopTabEnabled:(BOOL)desktopTabEnabled {
  if (desktopTabEnabled || isNewTabPage) {
    self.allButtons = @[
      self.panelButton,
      self.backButton,
      self.searchButton,
      self.forwardButton,
      self.tabGridButton
    ];
  } else {
    self.allButtons = @[
      self.panelButton,
      self.backButton,
      self.openNewTabButton,
      self.forwardButton,
      self.tabGridButton
    ];
  }

  [self.stackView removeFromSuperview];

  self.stackView =
      [[UIStackView alloc] initWithArrangedSubviews:self.allButtons];
  self.stackView.distribution = UIStackViewDistributionEqualSpacing;
  [self addSubview:self.stackView];

  [self.stackView anchorTop:self.topAnchor
                    leading:self.safeLeftAnchor
                     bottom:nil
                   trailing:self.safeRightAnchor
                    padding:UIEdgeInsetsMake(vSecondaryToolbarTopPadding,
                                             kAdaptiveToolbarMargin,
                                             0,
                                             kAdaptiveToolbarMargin)];
}

@end
