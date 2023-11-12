// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/sync/vivaldi_sync_encryption_password_view_controller.h"

#import "base/mac/foundation_util.h"
#import "ios/ui/table_view/cells/vivaldi_input_error_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/settings/sync/cells/vivaldi_table_view_text_edit_item.h"
#import "ios/ui/settings/sync/vivaldi_sync_settings_constants.h"
#import "ios/ui/table_view/cells/vivaldi_table_view_illustrated_item.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierHeader = kSectionIdentifierEnumZero,
  SectionIdentifierEncryptionPassword,
  SectionIdentifierSaveButton,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypePassword,
  ItemTypeSaveButton,
  ItemTypeError,
};

@interface VivaldiSyncEncryptionPasswordViewController () <UITextFieldDelegate>

@property(nonatomic, strong) VivaldiTableViewTextEditItem* encryptionPasswordItem;
@property(nonatomic, strong) VivaldiTableViewIllustratedItem* header;
@property(nonatomic, strong) TableViewTextButtonItem* saveButton;
@property(nonatomic, assign) BOOL creatingPasscode;

@end

@implementation VivaldiSyncEncryptionPasswordViewController
@synthesize delegate;

- (instancetype)initWithStyle:(UITableViewStyle)style {
  if ((self = [super initWithStyle:style])) {
    _creatingPasscode = NO;
  }
  return self;
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.delegate vivaldiSyncEncryptionPasswordViewControllerWasRemoved:self];
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadModel];
  self.title =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_ENCRYPTION_PASSWORD_TITLE);
}

- (void)setCreatingPasscode:(BOOL)isCreating {
  _creatingPasscode = isCreating;
  [self setHeaderSubtitleText];
  [self reloadSection:SectionIdentifierHeader];
  [self setButtonText];
  [self reloadSection:SectionIdentifierSaveButton];
}

- (void)setHeaderSubtitleText {
  NSMutableParagraphStyle* paragraphStyle =
      [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
  paragraphStyle.alignment = NSTextAlignmentCenter;
  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline],
    NSParagraphStyleAttributeName : paragraphStyle
  };
  _header.subtitle = [[NSAttributedString alloc]
        initWithString: _creatingPasscode ?
            l10n_util::GetNSString(IDS_VIVALDI_SET_UP_ENCRYPTION_DESC) :
            l10n_util::GetNSString(IDS_VIVALDI_DECRYPTION_TEXT)
        attributes:textAttributes];
}

- (void)setButtonText {
  _saveButton.buttonText = _creatingPasscode ?
      l10n_util::GetNSString(IDS_VIVALDI_SAVE) :
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_DECRYPT);
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierHeader];
  [model addSectionWithIdentifier:SectionIdentifierEncryptionPassword];
  [model addSectionWithIdentifier:SectionIdentifierSaveButton];

  _header = [
    [VivaldiTableViewIllustratedItem alloc] initWithType:ItemTypeHeader
  ];
  _header.image = [UIImage imageNamed:kVivaldiIcon];
  _header.title =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_ENCRYPTION_PASSWORD_TITLE);
  [self setHeaderSubtitleText];

  [model addItem:_header
      toSectionWithIdentifier:SectionIdentifierHeader];

  self.encryptionPasswordItem =
      [[VivaldiTableViewTextEditItem alloc] initWithType:ItemTypePassword];
  self.encryptionPasswordItem.textFieldPlaceholder =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_ENCRYPTION_PASSWORD_TITLE);
  self.encryptionPasswordItem.identifyingIcon =
      [UIImage imageNamed:kShowPasswordIcon];
  self.encryptionPasswordItem.identifyingIconEnabled = YES;
  self.encryptionPasswordItem.hideIcon = YES;
  self.encryptionPasswordItem.textFieldEnabled = YES;
  self.encryptionPasswordItem.textFieldSecureTextEntry = YES;
  self.encryptionPasswordItem.autoCapitalizationType =
      UITextAutocapitalizationTypeNone;
  self.encryptionPasswordItem.identifyingIconAccessibilityLabel =
    l10n_util::GetNSString(IDS_VIVALDI_IOS_SETTINGS_SHOW_PASSWORD_HINT);
  [model addItem:self.encryptionPasswordItem
      toSectionWithIdentifier:SectionIdentifierEncryptionPassword];

  _saveButton =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeSaveButton];
  [self setButtonText];
  _saveButton.textAlignment = NSTextAlignmentNatural;
  _saveButton.buttonBackgroundColor = [UIColor clearColor];
  _saveButton.buttonTextColor = [UIColor colorNamed:kBlueColor];
  _saveButton.disableButtonIntrinsicWidth = YES;

  [self.tableViewModel addItem:_saveButton
      toSectionWithIdentifier:SectionIdentifierSaveButton];
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  return kFooterSectionHeight;
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);
  switch (itemType) {
    case ItemTypeSaveButton: {
      TableViewTextButtonCell* tableViewTextButtonCell =
          base::mac::ObjCCastStrict<TableViewTextButtonCell>(cell);
      [tableViewTextButtonCell.button
                 addTarget:self
                    action:@selector(decryptButtonPressed:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    case ItemTypePassword: {
      VivaldiTableViewTextEditCell* editCell =
          base::mac::ObjCCast<VivaldiTableViewTextEditCell>(cell);
      [editCell.identifyingIconButton addTarget:self
                                         action:@selector(togglePasswordMasking)
                               forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    default:
      break;
  }

  return cell;
}

#pragma mark - Private Methods

- (void)togglePasswordMasking {
  self.encryptionPasswordItem.textFieldSecureTextEntry =
      !self.encryptionPasswordItem.textFieldSecureTextEntry;
  [self reconfigureCellsForItems:@[ self.encryptionPasswordItem ]];
}

- (void)decryptButtonPressed:(UIButton*)sender {
  if ([self.encryptionPasswordItem.textFieldValue length] == 0) {
    [self showErrorCellWithMessage:l10n_util::GetNSString(
        IDS_VIVALDI_ACCOUNT_ENCRYPTION_PASSPHRASE_REQUIRED)
                           section:SectionIdentifierEncryptionPassword
                          itemType:ItemTypeError];
    return;
  } else {
    [self removeErrorCell:SectionIdentifierEncryptionPassword
                itemType:ItemTypeError];
  }

  [self.delegate decryptButtonPressed:
      self.encryptionPasswordItem.textFieldValue];
}

@end
