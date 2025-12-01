// Copyright 2025 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/sync/cells/vivaldi_table_view_text_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

#pragma mark - VivaldiTableViewTextItem

@implementation VivaldiTableViewTextItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [VivaldiTableViewTextCell class];
    _enabled = YES;
  }
  return self;
}

- (void)configureCell:(LegacyTableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];
  VivaldiTableViewTextCell* cell =
      base::apple::ObjCCastStrict<VivaldiTableViewTextCell>(tableCell);
  cell.isAccessibilityElement = YES;

  cell.textLabel.text = self.text;
    cell.accessibilityLabel = self.text;

  if (self.textFont) {
    cell.textLabel.font = self.textFont;
  } else {
    cell.textLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  }

  // Decide cell.textLabel.textColor in order:
  //   1. this.textColor;
  //   2. styler.cellTitleColor;
  //   3. [UIColor colorNamed:kTextPrimaryColor].
  if (self.textColor) {
    cell.textLabel.textColor = self.textColor;
  } else if (styler.cellTitleColor) {
    cell.textLabel.textColor = styler.cellTitleColor;
  } else {
    cell.textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  }

  if (self.backgroundColor) {
    cell.backgroundColor = self.backgroundColor;
  }

  if (self.enabled) {
    cell.contentView.alpha = 1;
  } else {
    cell.contentView.alpha = 0.5;
  }

  cell.textLabel.textAlignment =
      self.textAlignment ? self.textAlignment : NSTextAlignmentNatural;

  cell.userInteractionEnabled = self.enabled;
}

@end

#pragma mark - VivaldiTableViewTextCell

@implementation VivaldiTableViewTextCell
@synthesize textLabel = _textLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    // Text Label, set font sizes using dynamic type.
    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.numberOfLines = 0;
    _textLabel.lineBreakMode = NSLineBreakByWordWrapping;
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.isAccessibilityElement = NO;

    // Add subviews to View Hierarchy.
    [self.contentView addSubview:_textLabel];

    // Set and activate constraints.
    [NSLayoutConstraint activateConstraints:@[
      [self.contentView.heightAnchor
          constraintGreaterThanOrEqualToConstant:kChromeTableViewCellHeight],

      // Title Label Constraints.
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_textLabel.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:kTableViewOneLabelCellVerticalSpacing],
      [_textLabel.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kTableViewOneLabelCellVerticalSpacing],
      [_textLabel.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing]
    ]];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.contentView.alpha = 1;
  self.userInteractionEnabled = YES;
}

@end
