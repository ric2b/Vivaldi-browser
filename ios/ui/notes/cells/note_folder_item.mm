// Copyright 2022 Vivaldi Technologies AS. All rights reserved.

#import "ios/ui/notes/cells/note_folder_item.h"

#import "base/apple/foundation_util.h"
#import "base/i18n/rtl.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/ui/notes/cells/note_table_cell_title_edit_delegate.h"
#import "ios/ui/notes/note_ui_constants.h"
#import "ios/ui/notes/note_utils_ios.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Width by which to indent folder cell's content. This is multiplied by the
// |indentationLevel| of the cell.
const CGFloat kFolderCellIndentationWidth = 32.0;
// The amount in points by which to inset horizontally the cell contents.
const CGFloat kFolderCellHorizonalInset = 17.0;
const CGFloat textStackSpacing = 4;
}  // namespace

#pragma mark - NoteFolderItem

@interface NoteFolderItem ()
@property(nonatomic, assign) NoteFolderStyle style;
@end

@implementation NoteFolderItem
@synthesize currentFolder = _currentFolder;
@synthesize indentationLevel = _indentationLevel;
@synthesize style = _style;
@synthesize title = _title;
@synthesize noteNode = _noteNode;

- (instancetype)initWithType:(NSInteger)type style:(NoteFolderStyle)style {
  if ((self = [super initWithType:type])) {
    self.cellClass = [TableViewNoteFolderCell class];
    self.style = style;
  }
  return self;
}

- (void)configureCell:(TableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  TableViewNoteFolderCell* folderCell =
      base::apple::ObjCCastStrict<TableViewNoteFolderCell>(cell);
  switch (self.style) {
    case NoteFolderStyleNewFolder: {
      folderCell.folderTitleTextField.text =
          l10n_util::GetNSString(IDS_VIVALDI_NOTE_CREATE_GROUP);
      // Note using the same image for folders as bookmarks now
      folderCell.folderImageView.image =
          [UIImage imageNamed:vNoteAddFolder];
      folderCell.accessibilityIdentifier =
          kNoteCreateNewFolderCellIdentifier;
      folderCell.accessibilityTraits |= UIAccessibilityTraitButton;
      break;
    }
    case NoteFolderStyleFolderEntry: {
      folderCell.folderTitleTextField.text = self.title;
      folderCell.accessibilityIdentifier = self.title;
      folderCell.accessibilityTraits |= UIAccessibilityTraitButton;
      if (self.isCurrentFolder)
        folderCell.noteAccessoryType =
            TableViewNoteFolderAccessoryTypeCheckmark;
      // In order to indent the cell's content we need to modify its
      // indentation constraint.
      folderCell.indentationConstraint.constant =
          folderCell.indentationConstraint.constant +
          kFolderCellIndentationWidth * self.indentationLevel;
      folderCell.folderImageView.image =
          [UIImage imageNamed:vNotesFolderIcon];
      break;
    }
  }
}

@end

#pragma mark - TableViewNoteFolderCell

@interface TableViewNoteFolderCell ()<UITextFieldDelegate>
// Re-declare as readwrite.
@property(nonatomic, strong, readwrite)
    NSLayoutConstraint* indentationConstraint;
// True when title text has ended editing and committed.
@property(nonatomic, assign) BOOL isTextCommitted;
@end

@implementation TableViewNoteFolderCell
@synthesize noteAccessoryType = _noteAccessoryType;
@synthesize folderImageView = _folderImageView;
@synthesize folderTitleTextField = _folderTitleTextFieldl;
@synthesize indentationConstraint = _indentationConstraint;
@synthesize isTextCommitted = _isTextCommitted;
@synthesize textDelegate = _textDelegate;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.selectionStyle = UITableViewCellSelectionStyleGray;
    self.isAccessibilityElement = YES;

    self.folderImageView = [[UIImageView alloc] init];
    self.folderImageView.contentMode = UIViewContentModeScaleAspectFit;
    [self.folderImageView
        setContentHuggingPriority:UILayoutPriorityRequired
                          forAxis:UILayoutConstraintAxisHorizontal];
    [self.folderImageView
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];

    self.folderTitleTextField = [[UITextField alloc] initWithFrame:CGRectZero];
    self.folderTitleTextField.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    self.folderTitleTextField.userInteractionEnabled = NO;
    self.folderTitleTextField.adjustsFontForContentSizeCategory = YES;
    [self.folderTitleTextField
        setContentHuggingPriority:UILayoutPriorityDefaultLow
                          forAxis:UILayoutConstraintAxisHorizontal];

    // Folder items count label
    UILabel* folderItemsLabel = [UILabel new];
    _folderItemsLabel = folderItemsLabel;
    folderItemsLabel.adjustsFontForContentSizeCategory = YES;
    folderItemsLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    folderItemsLabel.textColor = UIColor.secondaryLabelColor;
    folderItemsLabel.textAlignment = NSTextAlignmentLeft;

    // Vertical stack view for textfield and the items count label
    UIStackView* verticalStack =
        [[UIStackView alloc] initWithArrangedSubviews:@[
          self.folderTitleTextField, folderItemsLabel
        ]];
    verticalStack.axis = UILayoutConstraintAxisVertical;
    verticalStack.spacing = textStackSpacing;
    verticalStack.distribution = UIStackViewDistributionFillProportionally;
    verticalStack.alignment = UIStackViewAlignmentLeading;
    verticalStack.translatesAutoresizingMaskIntoConstraints = NO;

    // Container StackView.
    UIStackView* horizontalStack =
        [[UIStackView alloc] initWithArrangedSubviews:@[
          self.folderImageView, verticalStack
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
                       constant:kFolderCellHorizonalInset];
    [NSLayoutConstraint activateConstraints:@[
      [horizontalStack.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:kNoteCellVerticalInset],
      [horizontalStack.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kNoteCellVerticalInset],
      [horizontalStack.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kFolderCellHorizonalInset],
      self.indentationConstraint,
    ]];
  }
  return self;
}

- (void)setNoteAccessoryType:
    (TableViewNoteFolderAccessoryType)noteAccessoryType {
  _noteAccessoryType = noteAccessoryType;
  switch (_noteAccessoryType) {
    case TableViewNoteFolderAccessoryTypeCheckmark:
      self.accessoryView = [[UIImageView alloc]
          initWithImage:[UIImage imageNamed:vNoteFolderSelectionCheckmark]];
      break;
    case TableViewNoteFolderAccessoryTypeDisclosureIndicator: {
      self.accessoryView = [[UIImageView alloc]
          initWithImage:[UIImage imageNamed:@"table_view_cell_chevron"]];
      // TODO(crbug.com/870841): Use default accessory type.
      if (base::i18n::IsRTL())
        self.accessoryView.transform = CGAffineTransformMakeRotation(M_PI);
      break;
    }
    case TableViewNoteFolderAccessoryTypeNone:
      self.accessoryView = nil;
      break;
  }
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.noteAccessoryType = TableViewNoteFolderAccessoryTypeNone;
  self.indentationWidth = 0;
  self.imageView.image = nil;
  self.indentationConstraint.constant = kFolderCellHorizonalInset;
  self.folderTitleTextField.userInteractionEnabled = NO;
  self.textDelegate = nil;
  self.folderTitleTextField.accessibilityIdentifier = nil;
  self.accessoryType = UITableViewCellAccessoryNone;
  self.isAccessibilityElement = YES;
  self.folderItemsLabel.text = nil;
}

#pragma mark NoteTableCellTitleEditing

- (void)startEdit {
  self.isAccessibilityElement = NO;
  self.isTextCommitted = NO;
  self.folderTitleTextField.userInteractionEnabled = YES;
  self.folderTitleTextField.enablesReturnKeyAutomatically = YES;
  self.folderTitleTextField.keyboardType = UIKeyboardTypeDefault;
  self.folderTitleTextField.returnKeyType = UIReturnKeyDone;
  self.folderTitleTextField.accessibilityIdentifier = @"note_editing_text";
  [self.folderTitleTextField becomeFirstResponder];
  // selectAll doesn't work immediately after calling becomeFirstResponder.
  // Do selectAll on the next run loop.
  dispatch_async(dispatch_get_main_queue(), ^{
    if ([self.folderTitleTextField isFirstResponder]) {
      [self.folderTitleTextField selectAll:nil];
    }
  });
  self.folderTitleTextField.delegate = self;
}

- (void)stopEdit {
  if (self.isTextCommitted) {
    return;
  }
  self.isTextCommitted = YES;
  self.isAccessibilityElement = YES;
  [self.textDelegate textDidChangeTo:self.folderTitleTextField.text];
  self.folderTitleTextField.userInteractionEnabled = NO;
  [self.folderTitleTextField endEditing:YES];
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
  return self.folderTitleTextField.text;
}

@end
