// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note:(prio@vivaldi.com) - It is a chromium implementation which we moved
// to Vivaldi module to reduce the patch on Chromium git.
// This file is more heavily changed hence no // Vivaldi mark is added
// for the patches.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_strip/ui/tab_strip_group_cell.h"

#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/shared/ui/elements/fade_truncating_label.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_strip/ui/swift_constants_for_objective_c.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_strip/ui/tab_strip_group_stroke_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

constexpr double kCollapseUpdateGroupStrokeDelaySeconds = 0.25;
constexpr double kTitleContainerFadeAnimationSeconds = 0.25;
constexpr double kTitleContainerExpandedRadius = 10.0;
constexpr double kTitleContainerCollapsedRadius = 6.0;

}  // namespace

@implementation TabStripGroupCell {
  FadeTruncatingLabel* _titleLabel;
  UIView* _titleContainer;
  UIView* _notificationDotView;
  NSLayoutConstraint* _titleLabelTrailingConstraint;
  NSLayoutConstraint* _notificationDotViewTrailingConstraint;

  // `_collapsed` state of the cell before starting a drag action.
  BOOL _collapsedBeforeDrag;

  // Local property for group stroke color to compare before updating the view.
  UIColor* _strokeColor;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _titleContainer = [self createTitleContainer];
    [self.contentView addSubview:_titleContainer];
    [self setupConstraints];
    [self updateGroupStroke];
    [self updateAccessibilityValue];
  }
  return self;
}

#pragma mark - TabStripCell

- (UIDragPreviewParameters*)dragPreviewParameters {
  UIBezierPath* visiblePath = [UIBezierPath
      bezierPathWithRoundedRect:_titleContainer.frame
                   cornerRadius:_titleContainer.layer.cornerRadius];
  UIDragPreviewParameters* params = [[UIDragPreviewParameters alloc] init];
  params.visiblePath = visiblePath;
  return params;
}

#pragma mark - UICollectionViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  _titleContainer.accessibilityValue = nil;
  _titleContainer.accessibilityLabel = nil;
  _titleLabel.text = nil;
  self.delegate = nil;
  self.titleContainerBackgroundColor = nil;
  self.collapsed = NO;
  self.hasNotificationDot = NO;
}

- (void)applyLayoutAttributes:
    (UICollectionViewLayoutAttributes*)layoutAttributes {
  [super applyLayoutAttributes:layoutAttributes];
  // Update the transition state asynchronously to ensure bounds of subviews
  // have been updated accordingly.
  __weak __typeof(self) weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf updateTransitionState];
      }));
}

- (void)layoutSubviews {
  [super layoutSubviews];
  [self updateTransitionState];
}

- (void)dragStateDidChange:(UICollectionViewCellDragState)dragState {
  switch (dragState) {
    case UICollectionViewCellDragStateNone:
      [self setCollapsed:_collapsedBeforeDrag];
      break;
    case UICollectionViewCellDragStateLifting: {
      _collapsedBeforeDrag = _collapsed;
      [self setCollapsed:YES];
      break;
    }
    case UICollectionViewCellDragStateDragging:
      break;
  }
}

#pragma mark - Setters

- (void)setTitle:(NSString*)title {
  [super setTitle:title];
  _titleContainer.accessibilityLabel = title;
  _titleLabel.text = [title copy];
}

- (void)setTitleContainerBackgroundColor:(UIColor*)color {
  _titleContainerBackgroundColor = color;
  _titleContainer.backgroundColor = color;
}

- (void)setTitleTextColor:(UIColor*)titleTextColor {
  _titleTextColor = titleTextColor;
  _titleLabel.textColor = titleTextColor;
  _notificationDotView.backgroundColor = _titleTextColor;
}

- (void)setGroupStrokeColor:(UIColor*)color {
  [super setGroupStrokeColor:color];
  if ([_strokeColor isEqual:color]) {
    return;
  }
  _strokeColor = color;
  [self updateGroupStroke];
}

- (void)setCollapsed:(BOOL)collapsed {
  if (_collapsed == collapsed) {
    return;
  }
  _collapsed = collapsed;
  if (!collapsed) {
    [self updateGroupStroke];
  } else {
    __weak __typeof(self) weakSelf = self;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(^{
          [weakSelf updateGroupStroke];
        }),
        base::Seconds(kCollapseUpdateGroupStrokeDelaySeconds));
  }
  [self updateAccessibilityValue];
}

- (void)setHasNotificationDot:(BOOL)hasNotificationDot {
  if (_hasNotificationDot == hasNotificationDot) {
    return;
  }

  _hasNotificationDot = hasNotificationDot;

  if (hasNotificationDot) {
    [self showNotificationDotView];
  } else {
    [self hideNotificationDotView];
  }
}

#pragma mark - View creation helpers

// Returns a new title label.
- (FadeTruncatingLabel*)createTitleLabel {
  FadeTruncatingLabel* titleLabel = [[FadeTruncatingLabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.font = [UIFont systemFontOfSize:TabStripTabItemConstants.fontSize
                                      weight:UIFontWeightMedium];
  titleLabel.textColor = [UIColor colorNamed:kSolidWhiteColor];
  titleLabel.textAlignment = NSTextAlignmentCenter;
  [titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired - 1
                                      forAxis:UILayoutConstraintAxisHorizontal];
  return titleLabel;
}

// Returns a new title container view.
- (UIView*)createTitleContainer {
  UIView* titleContainer = [[UIView alloc] init];
  titleContainer.translatesAutoresizingMaskIntoConstraints = NO;
  titleContainer.layer.masksToBounds = YES;
  titleContainer.isAccessibilityElement = YES;
  _titleLabel = [self createTitleLabel];
  [titleContainer addSubview:_titleLabel];
  return titleContainer;
}

#pragma mark - UIAccessibility

- (NSArray*)accessibilityCustomActions {
  int stringID = self.collapsed ? IDS_IOS_TAB_STRIP_TAB_GROUP_EXPAND
                                : IDS_IOS_TAB_STRIP_TAB_GROUP_COLLAPSE;
  return @[ [[UIAccessibilityCustomAction alloc]
      initWithName:l10n_util::GetNSString(stringID)
            target:self
          selector:@selector(collapseOrExpandTapped:)] ];
}

// Selector registered to expand or collapse tab group.
- (void)collapseOrExpandTapped:(id)sender {
  [self.delegate collapseOrExpandTappedForCell:self];
}

#pragma mark - Private

// Sets up constraints.
- (void)setupConstraints {
  UIView* contentView = self.contentView;
  AddSameConstraintsToSides(_titleContainer, contentView,
                            LayoutSides::kLeading | LayoutSides::kTrailing |
                                LayoutSides::kTop | LayoutSides::kBottom);

  // The width of each cell is calculated in TabStripLayout.
  // The margin and padding at both edges and the title of a group is taken
  // account into the width of each cell.
  _titleLabelTrailingConstraint = [_titleLabel.trailingAnchor
      constraintEqualToAnchor:_titleContainer.trailingAnchor
                     constant:-TabStripGroupItemConstants
                                   .titleContainerHorizontalPadding];

  NSLayoutConstraint* titleLabelMaxWidthConstraint = [_titleLabel.widthAnchor
      constraintLessThanOrEqualToConstant:TabStripGroupItemConstants
                                              .maxTitleWidth];
  titleLabelMaxWidthConstraint.priority = UILayoutPriorityRequired;
  titleLabelMaxWidthConstraint.active = YES;

  [NSLayoutConstraint activateConstraints:@[
    [_titleLabel.centerYAnchor
        constraintEqualToAnchor:_titleContainer.centerYAnchor],
    [_titleLabel.leadingAnchor
        constraintEqualToAnchor:_titleContainer.leadingAnchor
                       constant:TabStripGroupItemConstants
                                    .titleContainerHorizontalPadding],
    _titleLabelTrailingConstraint,
  ]];
}

- (void)updateGroupStroke {
  // Define the corners based on collapsed state
  [self updateTitleContainerStateWithAlpha:NO alpha:1.0];
}

// Updates the title alpha value and title container height according to the
// difference between the size of the title and the size of its container.
- (void)updateTransitionState {
  CGFloat horizontalTitlePadding =
      TabStripGroupItemConstants.titleContainerHorizontalPadding;
  CGFloat titleContainerWidth = _titleContainer.bounds.size.width;
  CGFloat maxTitleContainerWidth =
      _titleLabel.frame.size.width + 2 * horizontalTitlePadding;
  CGFloat factor = 0;
  if (maxTitleContainerWidth - 2 * horizontalTitlePadding > 0) {
    factor = (titleContainerWidth - 2 * horizontalTitlePadding) /
             (maxTitleContainerWidth - 2 * horizontalTitlePadding);
  }
  _titleLabel.alpha = factor;
  // At the end of the group shrinking animation (factor is 0), if the group
  // intersects with the leading or trailing edge, then animate the title
  // container alpha to 0.
  CGFloat titleContainerAlpha = 1;
  if (factor == 0) {
    titleContainerAlpha = 0;
  }

  [self updateTitleContainerStateWithAlpha:YES alpha:titleContainerAlpha];
}

- (void)updateTitleContainerStateWithAlpha:(BOOL)withAlpha
                                     alpha:(CGFloat)alpha {
  UIRectCorner corners =
      self.collapsed ? UIRectCornerAllCorners
                     : (kCALayerMinXMinYCorner | kCALayerMinXMaxYCorner);

  UIView* titleContainer = _titleContainer;
  [UIView animateWithDuration:kTitleContainerFadeAnimationSeconds
                   animations:^{
                     if (withAlpha) {
                       titleContainer.alpha = alpha;
                     }
                     titleContainer.layer.cornerRadius =
                         self.collapsed ? kTitleContainerCollapsedRadius
                                        : kTitleContainerExpandedRadius;
                     titleContainer.layer.maskedCorners = corners;
                     [titleContainer setNeedsLayout];
                     [titleContainer layoutIfNeeded];
                   }];
}

- (void)updateAccessibilityValue {
  // Use the accessibility Value as there is a pause when using the
  // accessibility hint.
  _titleContainer.accessibilityValue = l10n_util::GetNSString(
      self.collapsed ? IDS_IOS_TAB_STRIP_GROUP_CELL_COLLAPSED_VOICE_OVER_VALUE
                     : IDS_IOS_TAB_STRIP_GROUP_CELL_EXPANDED_VOICE_OVER_VALUE);
}

// Shows the notification dow view. Adds the view to the cell if there is none
// yet.
- (void)showNotificationDotView {
  // Disable the center position of title label to avoid the conflict with the
  // constraint for the notification dot view.
  _titleLabelTrailingConstraint.active = NO;

  if (_notificationDotView) {
    // Enable the constraint for the notification dot view.
    CHECK(_notificationDotViewTrailingConstraint);
    _notificationDotViewTrailingConstraint.active = YES;

    _notificationDotView.hidden = NO;
    return;
  }

  _notificationDotView = [[UIView alloc] init];
  _notificationDotView.backgroundColor = _titleTextColor;
  _notificationDotView.translatesAutoresizingMaskIntoConstraints = NO;
  _notificationDotView.layer.cornerRadius =
      TabStripGroupItemConstants.notificationDotSize / 2;
  [_titleContainer addSubview:_notificationDotView];

  _notificationDotViewTrailingConstraint = [_notificationDotView.trailingAnchor
      constraintEqualToAnchor:_titleContainer.trailingAnchor
                     constant:-TabStripGroupItemConstants
                                   .titleContainerHorizontalPadding];

  [NSLayoutConstraint activateConstraints:@[
    [_notificationDotView.widthAnchor
        constraintEqualToConstant:TabStripGroupItemConstants
                                      .notificationDotSize],
    [_notificationDotView.heightAnchor
        constraintEqualToAnchor:_notificationDotView.widthAnchor],
    // Position the notification dot at right end of the cell.
    [_notificationDotView.centerYAnchor
        constraintEqualToAnchor:_titleContainer.centerYAnchor],
    [_notificationDotView.leadingAnchor
        constraintEqualToAnchor:_titleLabel.trailingAnchor
                       constant:TabStripGroupItemConstants
                                    .titleContainerHorizontalMargin],
    _notificationDotViewTrailingConstraint,
  ]];
}

// Hides the notification dot view in the cell and adjusts the constraints.
- (void)hideNotificationDotView {
  _notificationDotView.hidden = YES;

  // Disable the constraint for the notification dot view to avoid the conflict
  // with the constraint for the title label.
  _notificationDotViewTrailingConstraint.active = NO;

  // Enable the constraint for the title label to place it in the center of the
  // container view.
  _titleLabelTrailingConstraint.active = YES;
}

@end
