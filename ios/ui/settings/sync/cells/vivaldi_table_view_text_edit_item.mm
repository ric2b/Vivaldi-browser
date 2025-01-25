// Copyright 2023 Vivaldi Technologies. All rights reserved

#import "ios/ui/settings/sync/cells/vivaldi_table_view_text_edit_item.h"

#import "base/notreached.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Height/width of the edit icon.
const CGFloat kEditIconLength = 18;
// Height/width of the error icon.
const CGFloat kErrorIconLength = 20;
// Size of the symbols.
const CGFloat kSymbolSize = 15;

}  // namespace

@interface VivaldiTableViewTextEditItem ()

// Whether the field has valid text.
@property(nonatomic, assign) BOOL hasValidText;

@end

@implementation VivaldiTableViewTextEditItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [VivaldiTableViewTextEditCell class];
    _returnKeyType = UIReturnKeyNext;
    _keyboardType = UIKeyboardTypeDefault;
    _autoCapitalizationType = UITextAutocapitalizationTypeWords;
    _hasValidText = YES;
  }
  return self;
}

#pragma mark TableViewItem

- (void)configureCell:(VivaldiTableViewTextEditCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];

  if (self.textFieldPlaceholder) {
    cell.textField.attributedPlaceholder = [[NSAttributedString alloc]
        initWithString:self.textFieldPlaceholder
            attributes:@{
              NSForegroundColorAttributeName :
                  [UIColor colorNamed:kTextSecondaryColor]
            }];
  }
  cell.textField.text = self.textFieldValue;
  cell.textField.secureTextEntry = self.textFieldSecureTextEntry;
  if (self.textFieldName.length) {
    cell.textField.accessibilityIdentifier =
        [NSString stringWithFormat:@"%@_textField", self.textFieldName];
  }

  if (self.textFieldBackgroundColor) {
    cell.textField.backgroundColor = self.textFieldBackgroundColor;
  } else if (styler.cellBackgroundColor) {
    cell.textField.backgroundColor = styler.cellBackgroundColor;
  } else {
    cell.textField.backgroundColor = styler.tableViewBackgroundColor;
  }

  cell.textField.enabled = self.textFieldEnabled;

  if (self.hideIcon) {
    cell.textField.textColor = [UIColor colorNamed:kTextPrimaryColor];

    [cell setIcon:TableViewTextEditItemIconTypeNone];
  } else {
    if (self.hasValidText) {
      cell.textField.textColor = [UIColor colorNamed:kTextPrimaryColor];
    } else {
      cell.textField.textColor = [UIColor colorNamed:kRedColor];
    }

    if (!self.hasValidText) {
      cell.iconView.accessibilityIdentifier =
          [NSString stringWithFormat:@"%@_errorIcon", self.textFieldName];
      [cell setIcon:TableViewTextEditItemIconTypeError];
    } else if (cell.textField.editing && cell.textField.text.length > 0) {
      cell.iconView.accessibilityIdentifier =
          [NSString stringWithFormat:@"%@_noIcon", self.textFieldName];
      [cell setIcon:TableViewTextEditItemIconTypeNone];
    } else {
      cell.iconView.accessibilityIdentifier =
          [NSString stringWithFormat:@"%@_editIcon", self.textFieldName];
      [cell setIcon:TableViewTextEditItemIconTypeEdit];
    }
  }

  [cell.textField addTarget:self
                     action:@selector(textFieldChanged:)
           forControlEvents:UIControlEventEditingChanged];
  cell.textField.returnKeyType = self.returnKeyType;
  cell.textField.keyboardType = self.keyboardType;
  cell.textField.autocapitalizationType = self.autoCapitalizationType;

  [cell setIdentifyingIcon:self.identifyingIcon];
  cell.identifyingIconButton.enabled = self.identifyingIconEnabled;
  if ([self.identifyingIconAccessibilityLabel length]) {
    cell.identifyingIconButton.accessibilityLabel =
        self.identifyingIconAccessibilityLabel;
  }

  if (self.textContentType) {
    cell.textField.textContentType = self.textContentType;
  }

  // If the TextField or IconButton are enabled, the cell needs to make its
  // inner TextField or button accessible to voice over. In order to achieve
  // this the cell can't be an A11y element.
  cell.isAccessibilityElement =
      !(self.textFieldEnabled || self.identifyingIconEnabled);
}

#pragma mark Actions

- (void)textFieldChanged:(UITextField*)textField {
  self.textFieldValue = textField.text;
}

#pragma mark - Public

- (void)setHasValidText:(BOOL)hasValidText {
  if (_hasValidText == hasValidText)
    return;
  _hasValidText = hasValidText;
}

@end

#pragma mark - VivaldiTableViewTextEditCell

@interface VivaldiTableViewTextEditCell ()

@property(nonatomic, strong) NSLayoutConstraint* iconHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* iconWidthConstraint;
@property(nonatomic, strong) NSLayoutConstraint* textFieldTrailingConstraint;
@property(nonatomic, strong) NSLayoutConstraint* editIconHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* iconTrailingConstraint;

@end

@implementation VivaldiTableViewTextEditCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;
    UIView* contentView = self.contentView;

    _textField = [[UITextField alloc] init];
    _textField.translatesAutoresizingMaskIntoConstraints = NO;
    _textField.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textField.adjustsFontForContentSizeCategory = YES;
    _textField.textAlignment = NSTextAlignmentLeft;
    [_textField
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [contentView addSubview:_textField];

    _textField.autocorrectionType = UITextAutocorrectionTypeNo;
    _textField.clearButtonMode = UITextFieldViewModeWhileEditing;
    _textField.contentVerticalAlignment =
        UIControlContentVerticalAlignmentCenter;

    // Trailing icon button.
    _identifyingIconButton =
        [ExtendedTouchTargetButton buttonWithType:UIButtonTypeCustom];
    _identifyingIconButton.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_identifyingIconButton];

    // Edit icon.
    _iconView = [[UIImageView alloc] initWithImage:[self editImage]];
    _iconView.tintColor = [UIColor colorNamed:kGrey400Color];
    _iconView.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_iconView];

    // Set up the icons size constraints. They are activated here and updated in
    // layoutSubviews.
    _iconHeightConstraint =
        [_identifyingIconButton.heightAnchor constraintEqualToConstant:0];
    _iconWidthConstraint =
        [_identifyingIconButton.widthAnchor constraintEqualToConstant:0];
    _editIconHeightConstraint =
        [_iconView.heightAnchor constraintEqualToConstant:0];

    _textFieldTrailingConstraint = [_textField.trailingAnchor
        constraintEqualToAnchor:_iconView.leadingAnchor];
    _iconTrailingConstraint = [_iconView.trailingAnchor
        constraintEqualToAnchor:_identifyingIconButton.leadingAnchor];

    // Set up the constraints.
    [NSLayoutConstraint activateConstraints:@[
      [_textField.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      _textFieldTrailingConstraint,
      [_identifyingIconButton.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [_identifyingIconButton.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      [_iconView.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      _iconHeightConstraint,
      _iconWidthConstraint,
      _iconTrailingConstraint,
      _editIconHeightConstraint,
      [_iconView.widthAnchor constraintEqualToAnchor:_iconView.heightAnchor],
    ]];
    AddOptionalVerticalPadding(contentView, _textField,
                               kTableViewOneLabelCellVerticalSpacing);
  }
  return self;
}

#pragma mark Public

- (void)setIcon:(TableViewTextEditItemIconType)iconType {
  switch (iconType) {
    case TableViewTextEditItemIconTypeNone:
      self.iconView.hidden = YES;
      [self.iconView setImage:nil];
      self.textFieldTrailingConstraint.constant = 0;

      _editIconHeightConstraint.constant = 0;
      break;
    case TableViewTextEditItemIconTypeEdit:
      self.iconView.hidden = NO;
      [self.iconView setImage:[self editImage]];
      self.iconView.tintColor = [UIColor colorNamed:kGrey400Color];

      _editIconHeightConstraint.constant = kEditIconLength;
      break;
    case TableViewTextEditItemIconTypeError:
      self.iconView.hidden = NO;
      [self.iconView setImage:[self errorImage]];
      self.iconView.tintColor = [UIColor colorNamed:kRedColor];

      _editIconHeightConstraint.constant = kErrorIconLength;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

- (void)setIdentifyingIcon:(UIImage*)icon {
  // Set Image as UIImageRenderingModeAlwaysTemplate to allow the Button tint
  // color to propagate.
  [self.identifyingIconButton
      setImage:[icon imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]
      forState:UIControlStateNormal];
  // Set the same image for the button's disable state so it's not grayed out
  // when disabled.
  [self.identifyingIconButton setImage:icon forState:UIControlStateDisabled];
  if (icon) {
    // Set the size constraints of the icon view to the dimensions of the image.
    self.iconHeightConstraint.constant = icon.size.height;
    self.iconWidthConstraint.constant = icon.size.width;
  } else {
    self.iconTrailingConstraint.constant = 0;
    self.iconHeightConstraint.constant = 0;
    self.iconWidthConstraint.constant = 0;
  }
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textField.text = nil;
  self.textField.attributedPlaceholder = nil;
  self.textField.returnKeyType = UIReturnKeyNext;
  self.textField.keyboardType = UIKeyboardTypeDefault;
  self.textField.autocapitalizationType = UITextAutocapitalizationTypeWords;
  self.textField.autocorrectionType = UITextAutocorrectionTypeNo;
  self.textField.clearButtonMode = UITextFieldViewModeWhileEditing;
  self.isAccessibilityElement = YES;
  self.textField.accessibilityIdentifier = nil;
  self.textField.enabled = NO;
  self.textField.textAlignment = NSTextAlignmentLeft;
  self.textField.secureTextEntry = NO;
  self.textField.textContentType = nil;
  [self.textField removeTarget:nil
                        action:nil
              forControlEvents:UIControlEventAllEvents];
  [self setIdentifyingIcon:nil];
  self.identifyingIconButton.enabled = NO;
  [self.identifyingIconButton removeTarget:nil
                                    action:nil
                          forControlEvents:UIControlEventAllEvents];
}

#pragma mark Accessibility

- (NSString*)accessibilityLabel {
  // If `textFieldSecureTextEntry` is
  // YES, the voice over should not read the text value.
  NSString* textFieldText =
      self.textField.secureTextEntry ? @"" : self.textField.text;
  return textFieldText;
}

#pragma mark Private

// Returns the edit icon image.
- (UIImage*)editImage {
  return DefaultSymbolWithPointSize(kEditActionSymbol, kSymbolSize);
}

// Returns the error icon image.
- (UIImage*)errorImage {
  return DefaultSymbolWithPointSize(kErrorCircleFillSymbol, kSymbolSize);
}

@end
