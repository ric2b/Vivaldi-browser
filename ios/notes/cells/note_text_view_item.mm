// Copyright 2022 Vivaldi Technologies AS. All rights reserved.

#import "ios/notes/cells/note_text_view_item.h"

#import "base/check_op.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/notes/note_ui_constants.h"
#import "ios/notes/note_utils_ios.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - NoteTextViewItem

@implementation NoteTextViewItem

@synthesize text = _text;
@synthesize placeholder = _placeholder;
@synthesize delegate = _delegate;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  self.cellClass = [NoteTextViewCell class];
  return self;
}

#pragma mark TableViewItem

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];

  NoteTextViewCell* cell =
      base::mac::ObjCCastStrict<NoteTextViewCell>(tableCell);
  cell.textView.text = self.text;
  cell.textView.tag = self.type;
  cell.textView.delegate = self.delegate;
  cell.textView.accessibilityLabel = self.text;
  cell.textView.accessibilityIdentifier =
      [NSString stringWithFormat:@"%@_textField", self.accessibilityIdentifier];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
}

#pragma mark UITextViewDelegate

- (void)textViewDidChange:(UITextView*)textView {
    DCHECK_EQ(textView.tag, self.type);
    self.text = textView.text;
    NSLog(@"text view");

    //[self.delegate textDidChangeForTextViewItem:self];
}
@end

#pragma mark - NoteTextViewCell

@interface NoteTextViewCell ()
// Stack view to display label / value which we'll switch from horizontal to
// vertical based on preferredContentSizeCategory.
@end

@implementation NoteTextViewCell
@synthesize textView = _textView;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (!self)
    return nil;

  // Textfield.
  self.textView = [[UITextView alloc] init];
  self.textView.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  self.textView.translatesAutoresizingMaskIntoConstraints = NO;

  self.textView.adjustsFontForContentSizeCategory = YES;
  self.textView.textColor = [NoteTextViewCell textColorForEditing:NO];
  self.textView.textAlignment = NSTextAlignmentRight;
  self.textView.scrollEnabled = NO;
  self.textView.editable = YES;
  [self.textView setTextContainerInset:UIEdgeInsetsMake(kNoteCellVerticalInset,
                                                          kNoteCellVerticalInset,
                                                          kNoteCellVerticalInset,
                                                          kNoteCellVerticalInset)];
  [self.textView setContentHuggingPriority:UILayoutPriorityDefaultLow
                                    forAxis:UILayoutConstraintAxisHorizontal];
  [self.textView
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];

  [self.contentView addSubview:self.textView];
  self.textView.center = self.contentView.center;
  self.textView.scrollEnabled = YES;

  [self.contentView addConstraint:[NSLayoutConstraint
                                   constraintWithItem:self.contentView
                                   attribute:NSLayoutAttributeTop
                                   relatedBy:NSLayoutRelationEqual
                                   toItem:self.textView
                                   attribute:NSLayoutAttributeTop
                                   multiplier:1.0
                                   constant:0.0]];
  [self.contentView addConstraint:[NSLayoutConstraint
                                   constraintWithItem:self.contentView
                                   attribute:NSLayoutAttributeBottom
                                   relatedBy:NSLayoutRelationEqual
                                   toItem:self.textView
                                   attribute:NSLayoutAttributeBottom
                                   multiplier:1.0
                                   constant:0.0]];
  [self.contentView addConstraint:[NSLayoutConstraint
                                   constraintWithItem:self.contentView
                                   attribute:NSLayoutAttributeLeading
                                   relatedBy:NSLayoutRelationEqual
                                   toItem:self.textView
                                   attribute:NSLayoutAttributeLeading
                                   multiplier:1.0
                                   constant:0.0]];
  [self.contentView addConstraint:[NSLayoutConstraint
                                   constraintWithItem:self.contentView
                                   attribute:NSLayoutAttributeTrailing
                                   relatedBy:NSLayoutRelationEqual
                                   toItem:self.textView
                                   attribute:NSLayoutAttributeTrailing
                                   multiplier:1.0
                                   constant:0.0]];

  [self applyContentSizeCategoryStyles];
  return self;
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (self.traitCollection.preferredContentSizeCategory !=
      previousTraitCollection.preferredContentSizeCategory) {
    [self applyContentSizeCategoryStyles];
  }
}

- (void)applyContentSizeCategoryStyles {
    self.textView.textAlignment = NSTextAlignmentLeft;
}

+ (UIColor*)textColorForEditing:(BOOL)editing {
  return editing ? [UIColor colorNamed:kTextPrimaryColor]
                 : [UIColor colorNamed:kTextSecondaryColor];
}

- (void)prepareForReuse {
  [super prepareForReuse];
  [self.textView resignFirstResponder];
  self.textView.delegate = nil;
  self.textView.text = nil;
}

@end
