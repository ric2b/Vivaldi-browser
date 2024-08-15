// Copyright 2024 Vivaldi Technologies. All rights reserved

#import "ios/ui/settings/sync/cells/vivaldi_table_view_attributed_text_view_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/text_view_util.h"

#pragma mark - VivaldiTableViewAttributedTextViewItem

@implementation VivaldiTableViewAttributedTextViewItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [VivaldiTableViewAttributedTextViewCell class];
  }
  return self;
}

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];
  VivaldiTableViewAttributedTextViewCell* cell =
    base::apple::ObjCCastStrict<VivaldiTableViewAttributedTextViewCell>(
      tableCell);
  cell.textView.attributedText = _text;
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.backgroundColor = UIColor.clearColor;
}

@end

#pragma mark - VivaldiTableViewAttributedTextViewCell

@interface VivaldiTableViewAttributedTextViewCell ()
@end

@implementation VivaldiTableViewAttributedTextViewCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _textView = CreateUITextViewWithTextKit1();
    _textView.scrollEnabled = NO;
    _textView.editable = NO;
    _textView.translatesAutoresizingMaskIntoConstraints = NO;
    _textView.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _textView.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _textView.linkTextAttributes =
        @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};
    _textView.backgroundColor = UIColor.clearColor;

    [self.contentView addSubview:_textView];

    [NSLayoutConstraint activateConstraints:@[
      [_textView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor],
      [_textView.topAnchor constraintEqualToAnchor:self.contentView.topAnchor],
      [_textView.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor],
      [_textView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor]
    ]];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
}

@end
