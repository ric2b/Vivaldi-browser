// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_image_detail_text_cell.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

namespace {
const CGFloat kImageSize = 30;

}  // namespace

@interface SendTabToSelfImageDetailTextCell ()

// Image view for the cell.
@property(nonatomic, strong) UIImageView* imageView;

@end

@implementation SendTabToSelfImageDetailTextCell

@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;
@synthesize imageView = _imageView;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;
    [self addSubviews];
    [self setViewConstraints];
  }
  return self;
}

// Creates and adds subviews.
- (void)addSubviews {
  UIView* contentView = self.contentView;

  _imageView = [[UIImageView alloc] init];
  _imageView.translatesAutoresizingMaskIntoConstraints = NO;
  _imageView.tintColor = [UIColor colorNamed:kTextPrimaryColor];
  [_imageView setContentHuggingPriority:UILayoutPriorityRequired
                                forAxis:UILayoutConstraintAxisHorizontal];
  [_imageView
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  _imageView.contentMode = UIViewContentModeScaleAspectFit;
  [contentView addSubview:_imageView];

  _textLabel = [[UILabel alloc] init];
  _textLabel.numberOfLines = 0;
  _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  _textLabel.adjustsFontForContentSizeCategory = YES;
  _textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];

  _detailTextLabel = [[UILabel alloc] init];
  _detailTextLabel.numberOfLines = 0;
  _detailTextLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  _detailTextLabel.adjustsFontForContentSizeCategory = YES;
}

// Sets constraints on subviews.
- (void)setViewConstraints {
  UIView* contentView = self.contentView;

  UIStackView* textStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _textLabel, _detailTextLabel ]];
  textStackView.axis = UILayoutConstraintAxisVertical;
  textStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:textStackView];

  [NSLayoutConstraint activateConstraints:@[
    [_imageView.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:contentView.leadingAnchor
                                    constant:kTableViewHorizontalSpacing],
    [_imageView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
    [_imageView.widthAnchor constraintEqualToConstant:kImageSize],
    [_imageView.heightAnchor constraintEqualToConstant:kImageSize],
    [textStackView.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:_imageView.trailingAnchor
                                    constant:kTableViewImagePadding],
    [contentView.trailingAnchor
        constraintEqualToAnchor:textStackView.trailingAnchor
                       constant:kTableViewHorizontalSpacing],
    [contentView.bottomAnchor
        constraintGreaterThanOrEqualToAnchor:_imageView.bottomAnchor
                                    constant:kTableViewVerticalSpacing],
    [textStackView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
    [textStackView.topAnchor
        constraintGreaterThanOrEqualToAnchor:contentView.topAnchor
                                    constant:
                                        kTableViewTwoLabelsCellVerticalSpacing],
    [contentView.bottomAnchor
        constraintGreaterThanOrEqualToAnchor:textStackView.bottomAnchor
                                    constant:
                                        kTableViewTwoLabelsCellVerticalSpacing],

    // Leading constraint for `customSepartor`.
    [self.customSeparator.leadingAnchor
        constraintEqualToAnchor:self.textLabel.leadingAnchor],
  ]];
}

- (void)setImage:(UIImage*)image {
  BOOL hidden = !image;
  self.imageView.image = image;
  self.imageView.hidden = hidden;
}

- (UIImage*)image {
  return self.imageView.image;
}

- (void)setImageViewAlpha:(CGFloat)alpha {
  _imageView.alpha = alpha;
}

- (void)setImageViewTintColor:(UIColor*)color {
  _imageView.tintColor = color;
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  _imageView.alpha = 1.0f;
}

#pragma mark - UIAccessibility

- (NSString*)accessibilityLabel {
  if (!self.textLabel.text) {
    return self.detailTextLabel.text;
  }

  if (self.detailTextLabel.text) {
    return [NSString stringWithFormat:@"%@, %@", self.textLabel.text,
                                      self.detailTextLabel.text];
  }

  return self.textLabel.text;
}

- (NSArray<NSString*>*)accessibilityUserInputLabels {
  // The name for Voice Control includes only `self.textLabel.text`.
  if (!self.textLabel.text) {
    return @[];
  }
  return @[ self.textLabel.text ];
}

@end
