// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/custom_app_icon/cells/vivaldi_app_icon_item.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Proportion of Cell's textLabel/detailTextLabel. This guarantees that the
// textLabel occupies 75% of the row space and detailTextLabel occupies 25%.
constexpr CGFloat kCellLabelsWidthProportion = 3.f;

// Minimum cell height when the cell has 2 lines.
constexpr CGFloat kChromeTableViewTwoLinesCellHeight = 58.f;

// kBlueDotColor is the specific blue used for the blue dot notification badge.
constexpr NSString* kBlueDotColor = @"blue_600_color";

const UIEdgeInsets iconViewPadding = UIEdgeInsetsMake(3, 3, 3, 3);
const CGFloat iconViewShadowRadius = 1.5;
const CGFloat iconViewShadowOpacity = 0.25;
const CGSize iconViewShadowOffset = CGSizeMake(0, 1);
}  // namespace

@implementation VivaldiAppIconItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [VivaldiAppIconItemCell class];
  }
  return self;
}

#pragma mark TableViewItem

- (void)configureCell:(VivaldiAppIconItemCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.textLabel.text = self.text;
  [cell setDetailText:self.detailText];

  [cell setIconImage:self.iconImage
           tintColor:self.iconTintColor
     backgroundColor:self.iconBackgroundColor
        cornerRadius:self.iconCornerRadius];
  [cell setTextLayoutConstraintAxis:self.textLayoutConstraintAxis];
}

@end

#pragma mark - VivaldiAppIconItemCell

@interface VivaldiAppIconItemCell ()

// View containing UILabels `text` and `detailText`.
@property(nonatomic, strong) UIStackView* textStackView;
// Padding layout constraints.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* verticalPaddingConstraints;
// Constraint to set the cell minimum height.
@property(nonatomic, strong) NSLayoutConstraint* minimumCellHeightConstraint;
// Text width constraint between the title and the detail text.
@property(nonatomic, strong) NSLayoutConstraint* textWidthConstraint;
// Detail text. Can be nil if no text is set.
@property(nonatomic, strong) UILabel* detailTextLabel;

@end

@implementation VivaldiAppIconItemCell {
  UIView* _iconBackground;
  UIImageView* _iconImageView;
  NSLayoutConstraint* _iconHiddenConstraint;
  NSLayoutConstraint* _iconVisibleConstraint;
}

@synthesize detailTextLabel = _detailTextLabel;
@synthesize textLabel = _textLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;
    UIView* contentView = self.contentView;

    _iconBackground = [[UIView alloc] init];
    _iconBackground.translatesAutoresizingMaskIntoConstraints = NO;
    _iconBackground.hidden = YES;
    [contentView addSubview:_iconBackground];

    _iconImageView = [[UIImageView alloc] init];
    _iconImageView.contentMode = UIViewContentModeScaleAspectFit;

    // Set shadow properties
    _iconImageView.layer.shadowColor = [[UIColor blackColor] CGColor];
    _iconImageView.layer.shadowOpacity = iconViewShadowOpacity;
    _iconImageView.layer.shadowRadius = iconViewShadowRadius;
    _iconImageView.layer.shadowOffset = iconViewShadowOffset;

    [_iconBackground addSubview:_iconImageView];
    [_iconImageView fillSuperviewWithPadding:iconViewPadding];

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _textLabel.backgroundColor = UIColor.clearColor;

    _textStackView =
        [[UIStackView alloc] initWithArrangedSubviews:@[ _textLabel ]];
    _textStackView.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_textStackView];

    // Set up the constraints for when the icon is visible and hidden.  One of
    // these will be active at a time, defaulting to hidden.
    _iconHiddenConstraint = [_textStackView.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor
                       constant:kTableViewHorizontalSpacing];
    _iconVisibleConstraint = [_textStackView.leadingAnchor
        constraintEqualToAnchor:_iconBackground.trailingAnchor
                       constant:kTableViewImagePadding];

    _minimumCellHeightConstraint = [contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kChromeTableViewCellHeight];
    // Lower the priority for transition. The content view has autoresizing mask
    // to have the same height than the cell. To avoid breaking the constaints
    // while updating the minimum height constant, the constraint has to have
    // a lower priority.
    _minimumCellHeightConstraint.priority = UILayoutPriorityDefaultHigh - 1;
    _minimumCellHeightConstraint.active = YES;

    [NSLayoutConstraint activateConstraints:@[
      // Icon.
      [_iconBackground.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_iconBackground.widthAnchor
          constraintEqualToConstant:kTableViewIconImageSize],
      [_iconBackground.heightAnchor
          constraintEqualToAnchor:_iconBackground.widthAnchor],
      [_iconBackground.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],

      // Text labels.
      [_textStackView.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [_textStackView.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      _iconHiddenConstraint,

      // Leading constraint for `customSeparator`.
      [self.customSeparator.leadingAnchor
          constraintEqualToAnchor:_textStackView.leadingAnchor],
    ]];

    _verticalPaddingConstraints = AddOptionalVerticalPadding(
        contentView, _textStackView, kTableViewTwoLabelsCellVerticalSpacing);

    [self updateCellForAccessibilityContentSizeCategory:
              UIContentSizeCategoryIsAccessibilityCategory(
                  self.traitCollection.preferredContentSizeCategory)];
  }
  return self;
}

- (void)setIconImage:(UIImage*)image
           tintColor:(UIColor*)tintColor
     backgroundColor:(UIColor*)backgroundColor
        cornerRadius:(CGFloat)cornerRadius {
  if (image == nil && _iconImageView.image == nil) {
    return;
  }

  if (tintColor) {
    image = [image imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  }

  _iconImageView.image = image;
  _iconImageView.tintColor = tintColor;
  _iconBackground.backgroundColor = backgroundColor;
  _iconBackground.layer.cornerRadius = cornerRadius;

  BOOL hidden = (image == nil);
  _iconBackground.hidden = hidden;
  if (hidden) {
    _iconVisibleConstraint.active = NO;
    _iconHiddenConstraint.active = YES;
  } else {
    _iconHiddenConstraint.active = NO;
    _iconVisibleConstraint.active = YES;
  }
}

#pragma mark - Properties

- (void)setTextLayoutConstraintAxis:
    (UILayoutConstraintAxis)textLayoutConstraintAxis {
  CGFloat verticalPaddingConstant = 0;
  switch (textLayoutConstraintAxis) {
    case UILayoutConstraintAxisVertical:
      verticalPaddingConstant = kTableViewTwoLabelsCellVerticalSpacing;
      self.minimumCellHeightConstraint.constant =
          kChromeTableViewTwoLinesCellHeight;
      DCHECK(self.detailTextLabel);
      break;
    case UILayoutConstraintAxisHorizontal:
      verticalPaddingConstant = kTableViewOneLabelCellVerticalSpacing;
      self.minimumCellHeightConstraint.constant = kChromeTableViewCellHeight;
      break;
  }
  DCHECK(verticalPaddingConstant);
  for (NSLayoutConstraint* constraint in self.verticalPaddingConstraints) {
    constraint.constant = verticalPaddingConstant;
  }

  self.textStackView.axis = textLayoutConstraintAxis;

  [self updateCellForAccessibilityContentSizeCategory:
            UIContentSizeCategoryIsAccessibilityCategory(
                self.traitCollection.preferredContentSizeCategory)];
}

- (UILayoutConstraintAxis)textLayoutConstraintAxis {
  return self.textStackView.axis;
}

- (void)setDetailText:(NSString*)detailText {
  if (detailText.length > 0) {
    if (!self.detailTextLabel) {
      [self createDetailTextLabel];
    }
    self.detailTextLabel.text = detailText;
  } else if (self.detailTextLabel) {
    [self removeDetailTextLabel];
  }
}

#pragma mark - UIView

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  BOOL isCurrentCategoryAccessibility =
      UIContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory);
  if (isCurrentCategoryAccessibility !=
      UIContentSizeCategoryIsAccessibilityCategory(
          previousTraitCollection.preferredContentSizeCategory)) {
    [self updateCellForAccessibilityContentSizeCategory:
              isCurrentCategoryAccessibility];
  }
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];

  [self setTextLayoutConstraintAxis:UILayoutConstraintAxisHorizontal];
  [self setIconImage:nil tintColor:nil backgroundColor:nil cornerRadius:0];
  [self setDetailText:nil];
}

#pragma mark - Private

- (void)createDetailTextLabel {
  if (self.detailTextLabel) {
    return;
  }
  self.detailTextLabel = [[UILabel alloc] init];
  self.detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
  self.detailTextLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  self.detailTextLabel.adjustsFontForContentSizeCategory = YES;
  self.detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  self.detailTextLabel.backgroundColor = UIColor.clearColor;
  [self.textStackView addArrangedSubview:self.detailTextLabel];
  // In case the two labels don't fit in width, have the `textLabel` be 3
  // times the width of the `detailTextLabel` (so 75% / 25%).
  self.textWidthConstraint = [self.textLabel.widthAnchor
      constraintEqualToAnchor:self.detailTextLabel.widthAnchor
                   multiplier:kCellLabelsWidthProportion];
  // Set low priority to the proportion constraint between `self.textLabel` and
  // `self.detailTextLabel`, so that it won't break other layouts.
  self.textWidthConstraint.priority = UILayoutPriorityDefaultLow;
  [self updateCellForAccessibilityContentSizeCategory:
            UIContentSizeCategoryIsAccessibilityCategory(
                self.traitCollection.preferredContentSizeCategory)];
}

- (void)removeDetailTextLabel {
  if (!self.detailTextLabel) {
    return;
  }
  [self.detailTextLabel removeFromSuperview];
  self.detailTextLabel = nil;
  self.textWidthConstraint = nil;
  self.textLayoutConstraintAxis = UILayoutConstraintAxisHorizontal;
}

// Updates the cell such as it is layouted correctly with regard to the
// preferred content size category, if it is an
// `accessibilityContentSizeCategory` or not.
- (void)updateCellForAccessibilityContentSizeCategory:
    (BOOL)accessibilityContentSizeCategory {
  if (accessibilityContentSizeCategory) {
    _textWidthConstraint.active = NO;
    // detailTextLabel is laid below textLabel with accessibility content size
    // category.
    _detailTextLabel.textAlignment = NSTextAlignmentNatural;
    _detailTextLabel.numberOfLines = 0;
    _textLabel.numberOfLines = 0;
  } else {
    _textWidthConstraint.active = YES;
    // detailTextLabel is laid after textLabel and should have a trailing text
    // alignment with non-accessibility content size category if in horizontal
    // axis layout.
    if (_textStackView.axis == UILayoutConstraintAxisHorizontal) {
      _detailTextLabel.textAlignment =
          self.effectiveUserInterfaceLayoutDirection ==
                  UIUserInterfaceLayoutDirectionLeftToRight
              ? NSTextAlignmentRight
              : NSTextAlignmentLeft;
    } else {
      _detailTextLabel.textAlignment = NSTextAlignmentNatural;
    }
    _detailTextLabel.numberOfLines = 1;
    _textLabel.numberOfLines = 1;
  }
  UIFontTextStyle preferredFont =
      _textStackView.axis == UILayoutConstraintAxisVertical
          ? UIFontTextStyleFootnote
          : UIFontTextStyleBody;
  _detailTextLabel.font = [UIFont preferredFontForTextStyle:preferredFont];
}

- (NSString*)accessibilityLabel {
  return self.textLabel.text;
}

- (NSString*)accessibilityValue {
  return self.detailTextLabel.text;
}

- (NSArray<NSString*>*)accessibilityUserInputLabels {
  return @[ self.textLabel.text ];
}

@end
