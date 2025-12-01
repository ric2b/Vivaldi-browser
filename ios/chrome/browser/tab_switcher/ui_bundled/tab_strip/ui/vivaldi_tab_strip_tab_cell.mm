// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note:(prio@vivaldi.com) - It is a chromium implementation which we moved
// to Vivaldi module to reduce the patch on Chromium git.
// This file is more heavily changed hence no // Vivaldi mark is added
// for the patches.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_strip/ui/vivaldi_tab_strip_tab_cell.h"

#import <MaterialComponents/MaterialActivityIndicator.h>

#import <algorithm>

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/strings/string_number_conversions.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/image/image_util.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_strip/ui/swift_constants_for_objective_c.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_strip/ui/tab_strip_features_utils.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_strip/ui/tab_strip_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/raccoon/raccoon_api.h"
#import "ui/base/l10n/l10n_util.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_strip/ui/vivaldi_tab_strip_group_stroke_view.h"
#import "ios/chrome/browser/ui/tab_strip/vivaldi_tab_strip_constants.h"
#import "ios/ui/context_menu/vivaldi_context_menu_constants.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
// End Vivaldi

namespace {

// The size of the close button.
constexpr CGFloat kCloseButtonSize = 16;
constexpr CGFloat kCloseButtonMinimumTouchTarget = 36;

// Size of the decoration corner and corner radius when the cell is selected.
constexpr CGFloat kCornerSize = 8;
constexpr CGFloat kCornerSizeExpanded = 10;

// Visibility constants.
constexpr CGFloat kCloseButtonVisibilityThreshold = 0.3;

// Content view constants.
constexpr CGFloat kFaviconLeadingMargin = 10;
constexpr CGFloat kCloseButtonMargin = 10;
constexpr CGFloat kTitleInset = 10;
constexpr CGFloat kTitleOverflowWidth = 20;
constexpr CGFloat kFaviconSize = 16;
constexpr CGFloat kTitleGradientWidth = 16;

// The stroke width around a blue dot view.
constexpr CGFloat kBlueDotStrokeWidth = 2;

// Size of a blue dot on icon view with considering the stroke width.
constexpr CGFloat kBlueDotSize = 6 + kBlueDotStrokeWidth * 2;
constexpr CGFloat kBlueDotInset = 1;

// Tab Group Stroke
constexpr CGFloat kGroupStrokeOverlapAmount = 2.0;
constexpr CGFloat kGroupStrokeBottomOffset = 2.0;
constexpr CGFloat kTabGroupInset = 4.0;
constexpr CGFloat kGroupEdgeInset = 3.0;

// Returns the default favicon image.
UIImage* DefaultFavicon() {
  return DefaultSymbolWithPointSize(kGlobeAmericasSymbol, 14);
}

NSString* closeSymbolName = @"tabstrip_close_tab";

}  // namespace

@implementation VivaldiTabStripTabCell {
  // Content view subviews.
  UIButton* _closeButton;
  UIImageView* _faviconView;

  VivaldiTabStripGroupStrokeView* _groupStrokeView;

  // Circular spinner that shows the loading state of the tab.
  MDCActivityIndicator* _activityIndicator;

  // The cell's title is always displayed between the favicon and the close
  // button (or the trailing end of the cell if there is no close button). The
  // text of the title will follow its language direction. If the text is too
  // long, it is cut using a gradient.
  // To allow for changing the alpha of the cell, the gradient will be done
  // using a mask on the title and not having a view on top. Resizing
  // dynamically a gradient is not visually pleasant. So to achieve that, the
  // `_titleContainer` will have a fixed size (the max size of the view) and a
  // gradient on both sides. Based on the text reading direction, its right/left
  // edge will be positioned at the same side of `_titleLabel`.
  UIView* _titleContainer;
  UILabel* _titleLabel;
  // Title's trailing constraints.
  NSLayoutConstraint* _titleCollapsedTrailingConstraint;
  NSLayoutConstraint* _titleTrailingConstraint;
  // Title's gradient constraints.
  NSLayoutConstraint* _titleContainerLeftConstraint;
  NSLayoutConstraint* _titleContainerRightConstraint;
  // As NSLineBreakByClipping doesn't work with RTL languages, the `_titleLabel`
  // will be longer than its displayed position to have the ellipsis
  // non-visible. `_titlePositioner` has the "correct" right/left bounds. Based
  // on the title's text, the correct constraint will be activated.
  UILayoutGuide* _titlePositioner;
  NSLayoutConstraint* _titleLeftConstraint;
  NSLayoutConstraint* _titleRightConstraint;

  // Stroke view's constraints.
  NSLayoutConstraint* _groupStrokeViewWidthConstraint;
  NSLayoutConstraint* _containerViewTopConstraint;
  NSLayoutConstraint* _containerViewBottomConstraint;
  NSLayoutConstraint* _containerViewLeadingConstraint;
  NSLayoutConstraint* _containerViewTrailingConstraint;

  // Separator height constraints.
  NSArray<NSLayoutConstraint*>* _separatorHeightConstraints;
  CGFloat _separatorHeight;

  // whether the view is hovered.
  BOOL _hovered;

  // View used to provide accessibility labels/values while letting the close
  // button selectable by VoiceOver.
  UIView* _accessibilityContainerView;

  // View used to display the blue dot at right bottom corner of the favicon.
  UIView* _blueDotView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    self.layer.masksToBounds = NO;
    _hovered = NO;
    _separatorHeight = 0;

    [self addInteraction:[[UIPointerInteraction alloc] initWithDelegate:self]];

    if (ios::provider::IsRaccoonEnabled()) {
      if (@available(iOS 17.0, *)) {
        self.hoverStyle = [UIHoverStyle
            styleWithShape:[UIShape rectShapeWithCornerRadius:kCornerSize]];
      }
    }

    UIView* contentView = self.contentView;
    contentView.layer.masksToBounds = NO;

    _accessibilityContainerView = [[UIView alloc] init];
    _accessibilityContainerView.isAccessibilityElement = YES;
    _accessibilityContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    _accessibilityContainerView.layer.cornerRadius = kCornerSize;
    _accessibilityContainerView.clipsToBounds = YES;
    [contentView addSubview:_accessibilityContainerView];

    _faviconView = [self createFaviconView];
    [_accessibilityContainerView addSubview:_faviconView];

    _activityIndicator = [self createActivityIndicatior];
    [_accessibilityContainerView addSubview:_activityIndicator];

    _closeButton = [self createCloseButton];
    [contentView addSubview:_closeButton];

    _titleContainer = [self createTitleContainer];
    [_accessibilityContainerView addSubview:_titleContainer];

    _titlePositioner = [[UILayoutGuide alloc] init];
    [_accessibilityContainerView addLayoutGuide:_titlePositioner];

    // Create the group stroke view
    _groupStrokeView = [[VivaldiTabStripGroupStrokeView alloc] init];
    _groupStrokeView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_groupStrokeView];

    [self setupConstraints];
    [self updateGroupStroke];

    self.selected = NO;

    if (@available(iOS 17, *)) {
      NSArray<UITrait>* traits = TraitCollectionSetForTraits(nil);
      [self registerForTraitChanges:traits withAction:@selector(updateColors)];
    }
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  [self updateGroupStroke];
}

- (void)setFaviconImage:(UIImage*)image {
  if (!image) {
    _faviconView.image = DefaultFavicon();
  } else {
    _faviconView.image = image;
  }
}

#pragma mark - TabStripCell

- (UIDragPreviewParameters*)dragPreviewParameters {
  UIBezierPath* visiblePath =
      [UIBezierPath bezierPathWithRoundedRect:_accessibilityContainerView.frame
                                 cornerRadius:kCornerSize];
  UIDragPreviewParameters* params = [[UIDragPreviewParameters alloc] init];
  params.visiblePath = visiblePath;
  return params;
}

#pragma mark - Setters

- (void)setTitle:(NSString*)title {
  [super setTitle:title];
  _accessibilityContainerView.accessibilityLabel = title;
  NSTextAlignment titleTextAligment = DetermineBestAlignmentForText(title);
  _titleLabel.text = [title copy];
  _titleLabel.textAlignment = titleTextAligment;
  [self updateTitleConstraints];
}

- (void)setGroupStrokeColor:(UIColor*)color {
  [super setGroupStrokeColor:color];
  [_groupStrokeView setStrokeColor:color];
  [self updateGroupStroke];
}

- (void)setIsLastTabInGroup:(BOOL)isLastTabInGroup {
  if (_isLastTabInGroup == isLastTabInGroup) {
    return;
  }
  _isLastTabInGroup = isLastTabInGroup;
  [self updateGroupStroke];
}

- (void)setIsTabInGroup:(BOOL)isTabInGroup {
  if (_isTabInGroup == isTabInGroup) {
    return;
  }
  _isTabInGroup = isTabInGroup;
  [self updateGroupStroke];
}

- (void)setLoading:(BOOL)loading {
  if (_loading == loading) {
    return;
  }
  _loading = loading;
  if (loading) {
    _activityIndicator.hidden = NO;
    [_activityIndicator startAnimating];
    _faviconView.hidden = YES;
    _faviconView.image = DefaultFavicon();
  } else {
    _activityIndicator.hidden = YES;
    [_activityIndicator stopAnimating];
    _faviconView.hidden = NO;
  }
}

- (void)setSelected:(BOOL)selected {
  BOOL oldSelected = self.selected;
  [super setSelected:selected];

  if (selected) {
    _accessibilityContainerView.accessibilityTraits |=
        UIAccessibilityTraitSelected;
  } else {
    _accessibilityContainerView.accessibilityTraits &=
        ~UIAccessibilityTraitSelected;
  }

  [self updateColors];

  [self updateCollapsedState];
  if (oldSelected != self.selected) {
    [self updateGroupStroke];
  }

  _accessibilityContainerView.layer.cornerRadius = kCornerSize;

  [self updateCollapsedState];
}

- (void)setSeparatorsHeight:(CGFloat)height {
  if (_separatorHeight == height) {
    return;
  }
  _separatorHeight = height;
}

- (void)setTabIndex:(NSInteger)tabIndex {
  if (_tabIndex == tabIndex) {
    return;
  }
  _tabIndex = tabIndex;
  [self updateAccessibilityValue];
}

- (void)setNumberOfTabs:(NSInteger)numberOfTabs {
  if (_numberOfTabs == numberOfTabs) {
    return;
  }
  _numberOfTabs = numberOfTabs;
  [self updateAccessibilityValue];
}

- (void)setCloseButtonVisibility:(CGFloat)visibility {
  CGFloat closeButtonAlpha =
      std::clamp<CGFloat>((visibility - kCloseButtonVisibilityThreshold) /
                              (1 - kCloseButtonVisibilityThreshold),
                          0, 1);
  _closeButton.alpha = closeButtonAlpha;
  // Check if the alpha is low and not just 0 to avoid potential rounding
  // errors.
  _closeButton.hidden = closeButtonAlpha < 0.01;
  _titleTrailingConstraint.constant =
      -kTitleInset + (1 - visibility) * (kCloseButtonSize + kCloseButtonMargin);
}

// Hides the close button view if the cell is collapsed.
- (void)updateCollapsedState {
  BOOL collapsed =
      (!self.selected && !self.closeButtonVisible) || self.isPinnedTab;
  if (collapsed == _closeButton.hidden) {
    return;
  }

  _closeButton.hidden = collapsed;

  // To avoid breaking the layout, always disable the active constraint first.
  if (collapsed) {
    _titleTrailingConstraint.active = NO;
    _titleCollapsedTrailingConstraint.active = YES;
  } else {
    _titleCollapsedTrailingConstraint.active = NO;
    _titleTrailingConstraint.active = YES;
  }
}

- (void)setCloseButtonVisible:(BOOL)closeButtonVisible {
  if (_closeButtonVisible == closeButtonVisible)
    return;
  _closeButtonVisible = closeButtonVisible;
  [self updateCollapsedState];
}

- (void)selectedTabBackgroundColor:(UIColor*)selectedTabBackgroundColor {
  if (_selectedTabBackgroundColor == selectedTabBackgroundColor)
    return;
  _selectedTabBackgroundColor = selectedTabBackgroundColor;
  [self updateColors];
}

- (void)setBackgroundTabBackgroundColor:(UIColor*)backgroundTabBackgroundColor {
  if (_backgroundTabBackgroundColor == backgroundTabBackgroundColor)
    return;
  _backgroundTabBackgroundColor = backgroundTabBackgroundColor;
  [self updateColors];
}

- (void)setUseDarkTintForBackgroundTab:(BOOL)useDarkTintForBackgroundTab {
  if (_useDarkTintForBackgroundTab == useDarkTintForBackgroundTab)
    return;
  _useDarkTintForBackgroundTab = useDarkTintForBackgroundTab;
  [self updateColors];
}

- (void)setIsPinnedTab:(BOOL)isPinnedTab {
  if (_isPinnedTab == isPinnedTab)
    return;
  _isPinnedTab = isPinnedTab;
  [self updateCollapsedState];
}

- (void)setCellVisibility:(CGFloat)visibility {
  _accessibilityContainerView.alpha = visibility;
}

- (void)setHasBlueDot:(BOOL)hasBlueDot {
  if (_hasBlueDot == hasBlueDot) {
    return;
  }

  _hasBlueDot = hasBlueDot;

  if (hasBlueDot) {
    [self showBlueDotView];
  } else {
    [self hideBlueDotView];
  }
}

#pragma mark - UICollectionViewCell

- (void)applyLayoutAttributes:
    (UICollectionViewLayoutAttributes*)layoutAttributes {
  [super applyLayoutAttributes:layoutAttributes];

  [self updateCollapsedState];
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.selected = NO;
  [self setFaviconImage:nil];
  self.item = nil;
  self.numberOfTabs = 0;
  self.tabIndex = 0;
  _accessibilityContainerView.accessibilityValue = nil;
  self.loading = NO;
  self.isTabInGroup = NO;
  self.isFirstTabInGroup = NO;
  self.isLastTabInGroup = NO;
  self.hasBlueDot = NO;
  self.selectedTabBackgroundColor = nil;
  self.backgroundTabBackgroundColor = nil;
  self.isIncognito = NO;
}

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  [self updateColors];
}

- (void)dragStateDidChange:(UICollectionViewCellDragState)dragState {
  [super dragStateDidChange:dragState];
  [self updateColors];
}

#pragma mark - UITraitEnvironment

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }
  [self updateColors];
}
#endif

#pragma mark - UIAccessibility

- (NSArray*)accessibilityCustomActions {
  return @[ [[UIAccessibilityCustomAction alloc]
      initWithName:l10n_util::GetNSString(IDS_IOS_TAB_SWITCHER_CLOSE_TAB)
            target:self
          selector:@selector(closeButtonTapped:)] ];
}

#pragma mark - UIPointerInteractionDelegate

- (UIPointerRegion*)pointerInteraction:(UIPointerInteraction*)interaction
                      regionForRequest:(UIPointerRegionRequest*)request
                         defaultRegion:(UIPointerRegion*)defaultRegion {
  return defaultRegion;
}

- (void)pointerInteraction:(UIPointerInteraction*)interaction
           willEnterRegion:(UIPointerRegion*)region
                  animator:(id<UIPointerInteractionAnimating>)animator {
  _hovered = YES;
  [self updateColors];
}

- (void)pointerInteraction:(UIPointerInteraction*)interaction
            willExitRegion:(UIPointerRegion*)region
                  animator:(id<UIPointerInteractionAnimating>)animator {
  _hovered = NO;
  [self updateColors];
}

#pragma mark - Private

// Updates view colors.
- (void)updateColors {
  BOOL isSelected = self.isSelected;

  UIColor* tabColor = isSelected ?
      _selectedTabBackgroundColor : _backgroundTabBackgroundColor;
  CGFloat alpha =
      CGColorGetAlpha(tabColor.CGColor);
  // Multiple alpha twice to show highlighted effect
  _accessibilityContainerView.backgroundColor = [tabColor
          colorWithAlphaComponent:_hovered ? alpha * 2 : alpha];

  _titleLabel.textColor = [self tabContentsTintColor];
  _faviconView.tintColor = [self tabContentsTintColor];
  _closeButton.tintColor = [self tabContentsTintColor];
}

- (UIColor*)tabContentsTintColor {
  // If incognito:
  if (self.isIncognito) {
    if (self.isSelected) {
      return UIColor.whiteColor;
    } else {
      return [UIColor colorNamed:vTabViewNotSelectedTintColor];
    }
  } else {
    if (self.isSelected) {
      return [UIColor colorNamed:vTabViewSelectedTintColor];
    } else {
      if (self.useDarkTintForBackgroundTab) {
        return [UIColor blackColor];
      } else {
        return [UIColor colorNamed:vTabViewNotSelectedTintColor];
      }
    }
  }
}

// Updates the title gradient and text position horizontal constraints.
- (void)updateTitleConstraints {
  if (_titleLabel.textAlignment == NSTextAlignmentLeft) {
    _titleContainerLeftConstraint.active = NO;
    _titleContainerRightConstraint.active = YES;
    _titleRightConstraint.active = NO;
    _titleLeftConstraint.active = YES;
  } else {
    _titleContainerRightConstraint.active = NO;
    _titleContainerLeftConstraint.active = YES;
    _titleLeftConstraint.active = NO;
    _titleRightConstraint.active = YES;
  }
}

- (void)updateGroupStroke {
  if (!_isTabInGroup) {
    _groupStrokeView.hidden = YES;
    _containerViewTopConstraint.constant = 0.0;
    _containerViewBottomConstraint.constant = 0.0;
    _containerViewTrailingConstraint.constant = 0.0;
    return;
  }
  _groupStrokeView.hidden = NO;

  // Set vertical insets for grouped tabs
  _containerViewTopConstraint.constant = kTabGroupInset;
  _containerViewBottomConstraint.constant = -kTabGroupInset;

  // Set horizontal insets based on position in group
  if (!self.isFirstTabInGroup && !self.isLastTabInGroup) {
    // Middle tab in group
    _containerViewLeadingConstraint.constant = 0.0;
    _containerViewTrailingConstraint.constant = 0.0;
  } else if (self.isFirstTabInGroup && !self.isLastTabInGroup) {
    // First tab in group
    _containerViewLeadingConstraint.constant = kGroupEdgeInset;
    _containerViewTrailingConstraint.constant = 0.0;
  } else if (!self.isFirstTabInGroup && self.isLastTabInGroup) {
    // Last tab in group
    _containerViewLeadingConstraint.constant = 0.0;
    _containerViewTrailingConstraint.constant = -kGroupEdgeInset;
  } else {
    // Single tab in group
    _containerViewLeadingConstraint.constant = kGroupEdgeInset;
    _containerViewTrailingConstraint.constant = -kGroupEdgeInset;
  }

  // Draw top horizontal stroke
  UIBezierPath* topPath = [UIBezierPath bezierPath];
  CGFloat startX = -kGroupStrokeOverlapAmount;
  [topPath moveToPoint:CGPointMake(startX, 0)];

  CGFloat endX = self.isLastTabInGroup
                     ? self.bounds.size.width - kCornerSizeExpanded
                     : self.bounds.size.width;

  [topPath addLineToPoint:CGPointMake(endX, 0)];
  [_groupStrokeView setTopPath:topPath.CGPath];

  // Draw bottom horizontal stroke
  UIBezierPath* bottomPath = [UIBezierPath bezierPath];
  CGFloat bottomY = self.frame.size.height - kGroupStrokeBottomOffset;
  [bottomPath moveToPoint:CGPointMake(startX, bottomY)];
  [bottomPath addLineToPoint:CGPointMake(endX, bottomY)];
  [_groupStrokeView setBottomPath:bottomPath.CGPath];

  // Draw left rounded corner for first tab in group
  if (self.isFirstTabInGroup) {
    UIBezierPath* leftCorner = [UIBezierPath bezierPath];
    CGFloat cornerX = kCornerSizeExpanded;
    CGFloat radius = kCornerSizeExpanded;

    [leftCorner moveToPoint:CGPointMake(-kGroupStrokeOverlapAmount, 0)];
    [leftCorner addLineToPoint:CGPointMake(cornerX, 0)];

    // Top-left quarter circle
    CGPoint topLeftCenter = CGPointMake(cornerX, radius);
    [leftCorner addArcWithCenter:topLeftCenter
                          radius:radius
                      startAngle:-M_PI_2
                        endAngle:M_PI
                       clockwise:NO];

    [leftCorner addLineToPoint:CGPointMake(0, bottomY - radius)];

    // Bottom-left quarter circle
    CGPoint bottomLeftCenter = CGPointMake(cornerX, bottomY - radius);
    [leftCorner addArcWithCenter:bottomLeftCenter
                          radius:radius
                      startAngle:M_PI
                        endAngle:M_PI_2
                       clockwise:NO];

    [leftCorner addLineToPoint:CGPointMake(cornerX, bottomY)];
    [leftCorner
        addLineToPoint:CGPointMake(-kGroupStrokeOverlapAmount, bottomY)];
    [leftCorner addLineToPoint:CGPointMake(-kGroupStrokeOverlapAmount, 0)];
    [leftCorner closePath];

    [_groupStrokeView setLeftCornerPath:leftCorner.CGPath];
    [_groupStrokeView setLeftCornerFill:YES];
  } else {
    [_groupStrokeView setLeftCornerPath:nil];
    [_groupStrokeView setLeftCornerFill:NO];
  }

  // Draw right rounded corner for last tab in group
  if (self.isLastTabInGroup) {
    UIBezierPath* rightCorner = [UIBezierPath bezierPath];
    CGFloat cornerX = self.bounds.size.width - kCornerSizeExpanded;
    CGFloat radius = kCornerSizeExpanded;

    [rightCorner moveToPoint:CGPointMake(cornerX, 0)];

    // Top-right quarter circle
    CGPoint topRightCenter = CGPointMake(cornerX, radius);
    [rightCorner addArcWithCenter:topRightCenter
                           radius:radius
                       startAngle:-M_PI_2
                         endAngle:0
                        clockwise:YES];

    [rightCorner
        addLineToPoint:CGPointMake(self.bounds.size.width, bottomY - radius)];

    // Bottom-right quarter circle
    CGPoint bottomRightCenter = CGPointMake(cornerX, bottomY - radius);
    [rightCorner addArcWithCenter:bottomRightCenter
                           radius:radius
                       startAngle:0
                         endAngle:M_PI_2
                        clockwise:YES];

    [rightCorner addLineToPoint:CGPointMake(cornerX, bottomY)];

    [_groupStrokeView setRightCornerPath:rightCorner.CGPath];
  } else {
    [_groupStrokeView setRightCornerPath:nil];
  }

  // Ensure close button is always on top
  if (!_closeButton.hidden) {
    [self.contentView bringSubviewToFront:_closeButton];
  }
}

// Sets the cell constraints.
- (void)setupConstraints {
  UILayoutGuide* leadingImageGuide = [[UILayoutGuide alloc] init];
  [self addLayoutGuide:leadingImageGuide];

  UIView* contentView = self.contentView;

  /// `contentView` constraints.
  _containerViewTopConstraint = [_accessibilityContainerView.topAnchor
      constraintEqualToAnchor:contentView.topAnchor];
  _containerViewBottomConstraint = [_accessibilityContainerView.bottomAnchor
      constraintEqualToAnchor:contentView.bottomAnchor];
  _containerViewLeadingConstraint = [_accessibilityContainerView.leadingAnchor
      constraintEqualToAnchor:contentView.leadingAnchor
                     constant:0];
  _containerViewTrailingConstraint = [_accessibilityContainerView.trailingAnchor
      constraintEqualToAnchor:contentView.trailingAnchor
                     constant:0];

  [NSLayoutConstraint activateConstraints:@[
    _containerViewTopConstraint, _containerViewBottomConstraint,
    _containerViewLeadingConstraint, _containerViewTrailingConstraint
  ]];

  /// `leadingImageGuide` constraints.
  [NSLayoutConstraint activateConstraints:@[
    [leadingImageGuide.leadingAnchor
        constraintEqualToAnchor:_accessibilityContainerView.leadingAnchor
                       constant:kFaviconLeadingMargin],
    [leadingImageGuide.centerYAnchor
        constraintEqualToAnchor:_accessibilityContainerView.centerYAnchor],
    [leadingImageGuide.widthAnchor constraintEqualToConstant:kFaviconSize],
    [leadingImageGuide.heightAnchor
        constraintEqualToAnchor:leadingImageGuide.widthAnchor],
  ]];
  AddSameConstraints(leadingImageGuide, _faviconView);
  AddSameConstraints(leadingImageGuide, _activityIndicator);

  /// `_closeButton` constraints.
  [NSLayoutConstraint activateConstraints:@[
    [_closeButton.trailingAnchor
        constraintEqualToAnchor:_accessibilityContainerView.trailingAnchor
                       constant:-kCloseButtonMargin],
    [_closeButton.widthAnchor constraintEqualToConstant:kCloseButtonSize],
    [_closeButton.heightAnchor constraintEqualToConstant:kCloseButtonSize],
    [_closeButton.centerYAnchor
        constraintEqualToAnchor:_accessibilityContainerView.centerYAnchor],
  ]];

  /// `_titleLabel` constraints.
  _titleTrailingConstraint = [_titlePositioner.trailingAnchor
      constraintEqualToAnchor:_closeButton.leadingAnchor
                     constant:-kTitleInset];
  _titleCollapsedTrailingConstraint = [_titlePositioner.trailingAnchor
      constraintEqualToAnchor:_accessibilityContainerView.trailingAnchor
                     constant:-kTitleInset];
  // The trailing constraints have a lower priority to allow the cell to have a
  // size of 0.
  _titleTrailingConstraint.priority = UILayoutPriorityRequired - 1;
  _titleCollapsedTrailingConstraint.priority = UILayoutPriorityRequired - 1;
  _titleLeftConstraint = [_titleLabel.leftAnchor
      constraintEqualToAnchor:_titlePositioner.leftAnchor];
  _titleRightConstraint = [_titleLabel.rightAnchor
      constraintEqualToAnchor:_titlePositioner.rightAnchor];
  _titleContainerLeftConstraint = [_titleContainer.leftAnchor
      constraintEqualToAnchor:_titlePositioner.leftAnchor];
  _titleContainerRightConstraint = [_titleContainer.rightAnchor
      constraintEqualToAnchor:_titlePositioner.rightAnchor];
  [NSLayoutConstraint activateConstraints:@[
    [_titlePositioner.leadingAnchor
        constraintEqualToAnchor:leadingImageGuide.trailingAnchor
                       constant:kTitleInset],
    _titleTrailingConstraint,
    _titleLeftConstraint,
    [_titleLabel.centerYAnchor
        constraintEqualToAnchor:_accessibilityContainerView.centerYAnchor],
    [_titleLabel.widthAnchor
        constraintEqualToAnchor:_titlePositioner.widthAnchor
                       constant:kTitleOverflowWidth],
    [_titleContainer.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
  ]];

  _groupStrokeViewWidthConstraint = [_groupStrokeView.widthAnchor
      constraintEqualToAnchor:self.contentView.widthAnchor
                     constant:0];

  // Always active constraints
  [NSLayoutConstraint activateConstraints:@[
    [_groupStrokeView.topAnchor
        constraintEqualToAnchor:self.contentView.topAnchor
                       constant:1],
    [_groupStrokeView.bottomAnchor
        constraintEqualToAnchor:self.contentView.bottomAnchor
                       constant:-1],
    [_groupStrokeView.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor],
    [_groupStrokeView.trailingAnchor
        constraintEqualToAnchor:self.contentView.trailingAnchor],
  ]];
}

// Selector registered to the close button.
- (void)closeButtonTapped:(id)sender {
  base::RecordAction(base::UserMetricsAction("MobileTabStripCloseTab"));
  [self.delegate closeButtonTappedForCell:self];
}

// Returns a new favicon view.
- (UIImageView*)createFaviconView {
  UIImageView* faviconView =
      [[UIImageView alloc] initWithImage:DefaultFavicon()];
  faviconView.translatesAutoresizingMaskIntoConstraints = NO;
  faviconView.contentMode = UIViewContentModeScaleAspectFit;
  return faviconView;
}

// Returns a new close button.
- (UIButton*)createCloseButton {
  UIImage* closeSymbol = [[UIImage imageNamed:closeSymbolName]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  ExtendedTouchTargetButton* button = [[ExtendedTouchTargetButton alloc] init];
  button.minimumDiameter = kCloseButtonMinimumTouchTarget;
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.tintColor = [UIColor colorNamed:kTextSecondaryColor];
  [button setImage:closeSymbol forState:UIControlStateNormal];
  [button addTarget:self
                action:@selector(closeButtonTapped:)
      forControlEvents:UIControlEventTouchUpInside];
  button.pointerInteractionEnabled = YES;
  button.accessibilityIdentifier =
      TabStripTabItemConstants.closeButtonAccessibilityIdentifier;
  return button;
}

// Returns a new title label.
- (UILabel*)createTitleLabel {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.font = [UIFont systemFontOfSize:TabStripTabItemConstants.fontSize
                                      weight:UIFontWeightMedium];
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  titleLabel.adjustsFontForContentSizeCategory = YES;

  return titleLabel;
}

// Returns a new gradient view.
- (GradientView*)createGradientView {
  GradientView* gradientView = [[GradientView alloc]
      initWithStartColor:[TabStripHelper.cellBackgroundColor
                             colorWithAlphaComponent:0]
                endColor:TabStripHelper.cellBackgroundColor
              startPoint:CGPointMake(0.0f, 0.5f)
                endPoint:CGPointMake(1.0f, 0.5f)];
  gradientView.translatesAutoresizingMaskIntoConstraints = NO;
  return gradientView;
}

// Returns a new title container view.
- (UIView*)createTitleContainer {
  UIView* titleContainer = [[UIView alloc] init];
  titleContainer.translatesAutoresizingMaskIntoConstraints = NO;
  titleContainer.clipsToBounds = YES;

  CGFloat cellMaxWidth = TabStripTabItemConstants.maxWidth;
  // The percentage of width of the cell to have a gradient of
  // `kTitleGradientWidth` width on both sides.
  CGFloat gradientPercentage = kTitleGradientWidth / cellMaxWidth;

  CAGradientLayer* gradientMask = [[CAGradientLayer alloc] init];
  gradientMask.frame =
      CGRectMake(0, 0, cellMaxWidth, TabStripCollectionViewConstants.height);
  [NSLayoutConstraint activateConstraints:@[
    [titleContainer.widthAnchor constraintEqualToConstant:cellMaxWidth],
    [titleContainer.heightAnchor
        constraintEqualToConstant:TabStripCollectionViewConstants.height],
  ]];
  gradientMask.colors = @[
    (id)UIColor.clearColor.CGColor, (id)UIColor.blackColor.CGColor,
    (id)UIColor.blackColor.CGColor, (id)UIColor.clearColor.CGColor
  ];
  gradientMask.startPoint = CGPointMake(0, 0.5);
  gradientMask.endPoint = CGPointMake(1, 0.5);
  gradientMask.locations =
      @[ @(0), @(gradientPercentage), @(1 - gradientPercentage), @(1) ];

  titleContainer.layer.mask = gradientMask;

  _titleLabel = [self createTitleLabel];
  [titleContainer addSubview:_titleLabel];

  return titleContainer;
}

// Returns a new Activity Indicator.
- (MDCActivityIndicator*)createActivityIndicatior {
  MDCActivityIndicator* activityIndicator = [[MDCActivityIndicator alloc] init];
  activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
  activityIndicator.hidden = YES;
  return activityIndicator;
}

- (void)updateAccessibilityValue {
  // Use the accessibility Value as there is a pause when using the
  // accessibility hint.
  BOOL grouped = self.groupStrokeColor != nil;
  _accessibilityContainerView.accessibilityValue = l10n_util::GetNSStringF(
      grouped ? IDS_IOS_TAB_STRIP_TAB_CELL_IN_GROUP_VOICE_OVER_VALUE
              : IDS_IOS_TAB_STRIP_TAB_CELL_VOICE_OVER_VALUE,
      base::NumberToString16(self.tabIndex),
      base::NumberToString16(self.numberOfTabs));
}

// Shows the blue dot view. Adds the view to the cell if there is none yet.
- (void)showBlueDotView {
  if (_blueDotView) {
    _blueDotView.hidden = NO;
    return;
  }

  _blueDotView = [[UIView alloc] init];
  _blueDotView.translatesAutoresizingMaskIntoConstraints = NO;
  _blueDotView.layer.cornerRadius = kBlueDotSize / 2;
  _blueDotView.layer.borderWidth = kBlueDotStrokeWidth;
  _blueDotView.layer.borderColor = TabStripHelper.cellBackgroundColor.CGColor;
  _blueDotView.backgroundColor = [UIColor colorNamed:kBlue600Color];
  [_accessibilityContainerView addSubview:_blueDotView];

  [NSLayoutConstraint activateConstraints:@[
    [_blueDotView.widthAnchor constraintEqualToConstant:kBlueDotSize],
    [_blueDotView.heightAnchor constraintEqualToConstant:kBlueDotSize],
    // Position the blue dot at right bottom corner of the favicon image.
    [_blueDotView.centerXAnchor
        constraintEqualToAnchor:_faviconView.centerXAnchor
                       constant:kFaviconSize / 2 - kBlueDotInset],
    [_blueDotView.centerYAnchor
        constraintEqualToAnchor:_faviconView.centerYAnchor
                       constant:kFaviconSize / 2 - kBlueDotInset],
  ]];
}

// Hides the blue dot view.
- (void)hideBlueDotView {
  _blueDotView.hidden = YES;
}

@end
