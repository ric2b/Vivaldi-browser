// Copyright 2022 Vivaldi Technologies AS. All rights reserved.

#import "ios/notes/cells/table_view_note_cell.h"

#import "base/i18n/rtl.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/notes/cells/note_table_cell_title_edit_delegate.h"
#import "ios/notes/note_ui_constants.h"
#import "ios/notes/note_utils_ios.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat textStackSpacing = 4.0;
const UIEdgeInsets textStackPadding = UIEdgeInsetsMake(12, 52, 12, 8);
}  // namespace

#pragma mark - TableViewNoteCell

@interface TableViewNoteCell ()
// The note title displayed by this cell.
@property(nonatomic, strong) UILabel* titleLabel;
// The note created date displayed by this cell.
@property(nonatomic, strong) UILabel* createdLabel;
// Date formatter for createdLabel.
@property(nonatomic, strong) NSDateFormatter* formatter;
@end

@implementation TableViewNoteCell
@synthesize titleLabel = _titleLabel;
@synthesize createdLabel = _createdLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    [self setUpUI];
    self.formatter = [[NSDateFormatter alloc] init];
    [self.formatter setDateFormat:@"MMM dd, yyyy hh:mm a"];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.titleLabel.text = nil;
  self.createdLabel.text = nil;
}

#pragma mark:- PRIVATE
- (void)setUpUI {
  self.selectionStyle = UITableViewCellSelectionStyleGray;
  self.isAccessibilityElement = YES;
  self.accessoryType = UITableViewCellAccessoryNone;

  UILabel* titleLabel = [UILabel new];
  _titleLabel = titleLabel;
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  titleLabel.textColor = UIColor.labelColor;
  titleLabel.textAlignment = NSTextAlignmentLeft;

  UILabel* createdLabel = [UILabel new];
  _createdLabel = createdLabel;
  createdLabel.adjustsFontForContentSizeCategory = YES;
  createdLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
  createdLabel.textColor = UIColor.secondaryLabelColor;
  createdLabel.textAlignment = NSTextAlignmentLeft;

  UIStackView* textVStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    titleLabel, createdLabel
  ]];
  textVStack.axis = UILayoutConstraintAxisVertical;
  textVStack.spacing = textStackSpacing;
  textVStack.distribution = UIStackViewDistributionFillProportionally;

  [self.contentView addSubview:textVStack];
  [textVStack fillSuperviewWithPadding: textStackPadding];
}

#pragma mark: GETTERS

- (NSString*)accessibilityLabelString {
  return self.titleLabel.text;
}

#pragma mark: SETTERS

- (void)configureNoteWithTitle:(NSString*)title
                     createdAt:(NSDate*)createdAt {
  self.titleLabel.text = title;
  self.createdLabel.text = [self.formatter stringFromDate:createdAt];
}

@end
