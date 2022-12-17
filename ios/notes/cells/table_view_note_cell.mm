// Copyright 2022 Vivaldi Technologies AS. All rights reserved.

#import "ios/notes/cells/table_view_note_cell.h"

#include "base/i18n/rtl.h"
#include "base/mac/foundation_util.h"
#import "ios/notes/note_ui_constants.h"
#import "ios/notes/note_utils_ios.h"
#import "ios/notes/cells/note_table_cell_title_edit_delegate.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#import "vivaldi/mobile_common/grit/vivaldi_mobile_common_native_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The amount in points by which to inset horizontally the cell contents.
const CGFloat kCellHorizonalInset = 17.0;
}  // namespace

#pragma mark - TableViewNoteCell

@interface TableViewNoteCell ()<UITextFieldDelegate>
// Re-declare as readwrite.
@property(nonatomic, strong, readwrite)
    NSLayoutConstraint* indentationConstraint;
// True when title text has ended editing and committed.
@property(nonatomic, assign) BOOL isTextCommitted;
@end

@implementation TableViewNoteCell
@synthesize noteAccessoryType = _noteAccessoryType;
@synthesize imageView = _imageView;
@synthesize titleTextField = _titleTextFieldl;
@synthesize indentationConstraint = _indentationConstraint;
@synthesize isTextCommitted = _isTextCommitted;
@synthesize textDelegate = _textDelegate;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.selectionStyle = UITableViewCellSelectionStyleGray;
    self.isAccessibilityElement = YES;

    self.imageView = [[UIImageView alloc] init];
    self.imageView.contentMode = UIViewContentModeScaleAspectFit;
    [self.imageView
        setContentHuggingPriority:UILayoutPriorityRequired
                          forAxis:UILayoutConstraintAxisHorizontal];
    [self.imageView
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];

    self.titleTextField = [[UITextField alloc] initWithFrame:CGRectZero];
    self.titleTextField.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    self.titleTextField.userInteractionEnabled = NO;
    self.titleTextField.adjustsFontForContentSizeCategory = YES;
    [self.titleTextField
        setContentHuggingPriority:UILayoutPriorityDefaultLow
                          forAxis:UILayoutConstraintAxisHorizontal];
      self.titleTextField.text = l10n_util::GetNSString(IDS_VIVALDI_NOTE_CREATE_GROUP);

    // Container StackView.
    UIStackView* horizontalStack =
        [[UIStackView alloc] initWithArrangedSubviews:@[
          self.imageView, self.titleTextField
        ]];
    horizontalStack.axis = UILayoutConstraintAxisHorizontal;
    horizontalStack.spacing = kNoteCellViewSpacing;
    horizontalStack.distribution = UIStackViewDistributionFill;
    horizontalStack.alignment = UIStackViewAlignmentCenter;
    horizontalStack.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:horizontalStack];

    // Set up constraints.
    self.indentationConstraint = [horizontalStack.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:kCellHorizonalInset];
    [NSLayoutConstraint activateConstraints:@[
      [horizontalStack.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:kNoteCellVerticalInset],
      [horizontalStack.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kNoteCellVerticalInset],
      [horizontalStack.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kCellHorizonalInset],
      self.indentationConstraint,
    ]];
  }
  return self;
}

- (void)setNoteAccessoryType:
    (TableViewNoteAccessoryType)noteAccessoryType {
  _noteAccessoryType = noteAccessoryType;
  switch (_noteAccessoryType) {
    case TableViewNoteAccessoryTypeNone:
      self.accessoryView = nil;
      break;
  }
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.noteAccessoryType = TableViewNoteAccessoryTypeNone;
  self.indentationWidth = 0;
  self.imageView.image = nil;
  self.indentationConstraint.constant = kCellHorizonalInset;
  self.titleTextField.userInteractionEnabled = NO;
  self.textDelegate = nil;
  self.titleTextField.accessibilityIdentifier = nil;
  self.accessoryType = UITableViewCellAccessoryNone;
  self.isAccessibilityElement = YES;
}

#pragma mark NoteTableCellTitleEditing

- (void)startEdit {
  self.isAccessibilityElement = NO;
  self.isTextCommitted = NO;
  self.titleTextField.userInteractionEnabled = YES;
  self.titleTextField.enablesReturnKeyAutomatically = YES;
  self.titleTextField.keyboardType = UIKeyboardTypeDefault;
  self.titleTextField.returnKeyType = UIReturnKeyDone;
  self.titleTextField.accessibilityIdentifier = @"note_editing_text";
  [self.titleTextField becomeFirstResponder];
  // selectAll doesn't work immediately after calling becomeFirstResponder.
  // Do selectAll on the next run loop.
  dispatch_async(dispatch_get_main_queue(), ^{
    if ([self.titleTextField isFirstResponder]) {
      [self.titleTextField selectAll:nil];
    }
  });
  self.titleTextField.delegate = self;
}

- (void)stopEdit {
  if (self.isTextCommitted) {
    return;
  }
  self.isTextCommitted = YES;
  self.isAccessibilityElement = YES;
  [self.textDelegate textDidChangeTo:self.titleTextField.text];
  self.titleTextField.userInteractionEnabled = NO;
  [self.titleTextField endEditing:YES];
}

#pragma mark UITextFieldDelegate

// This method hides the keyboard when the return key is pressed.
- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [self stopEdit];
  return YES;
}

// This method is called when titleText resigns its first responder status.
// (when return/dimiss key is pressed, or when navigating away.)
- (void)textFieldDidEndEditing:(UITextField*)textField
                        reason:(UITextFieldDidEndEditingReason)reason {
  [self stopEdit];
}

#pragma mark Accessibility

- (NSString*)accessibilityLabel {
  return self.titleTextField.text;
}

@end
