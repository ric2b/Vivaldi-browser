// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/table_view/cells/vivaldi_input_error_item.h"

#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation VivaldiInputErrorItem

@synthesize text = _text;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [VivaldiInputErrorCell class];
  }
  return self;
}

- (void)configureCell:(TableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.textLabel.text = self.text;
  cell.selectionStyle = UITableViewCellSelectionStyleNone;

  if (self.textColor) {
    cell.textLabel.textColor = self.textColor;
  }
}

@end

@interface VivaldiInputErrorCell ()

@end

@implementation VivaldiInputErrorCell

@synthesize textLabel = _textLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    UIView* contentView = self.contentView;

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.textColor = UIColor.redColor;

    _textLabel.adjustsFontSizeToFitWidth = YES;
    _textLabel.minimumScaleFactor = 0.8;
    _textLabel.numberOfLines = 4;
    [contentView addSubview:_textLabel];

    // Set up the constraints.
    [_textLabel fillSuperviewWithPadding:
      UIEdgeInsetsMake(kTableViewOneLabelCellVerticalSpacing,
                       kTableViewHorizontalSpacing,
                       kTableViewOneLabelCellVerticalSpacing,
                       kTableViewHorizontalSpacing)];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textLabel.text = nil;
}

@end
