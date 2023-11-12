// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/sync/vivaldi_sync_encryption_password_view_controller.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/settings/cells/passphrase_error_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/settings/sync/cells/vivaldi_table_view_text_edit_item.h"
#import "ios/ui/settings/sync/vivaldi_sync_settings_constants.h"
#import "ios/ui/table_view/cells/vivaldi_table_view_illustrated_item.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"
#import "vivaldi/mobile_common/grit/vivaldi_mobile_common_native_strings.h"

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

@end

@implementation VivaldiSyncEncryptionPasswordViewController
@synthesize delegate;

- (instancetype)initWithStyle:(UITableViewStyle)style {
  if ((self = [super initWithStyle:style])) {
    self.shouldHideDoneButton = YES;
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
  self.title = l10n_util::GetNSString(IDS_PREFS_VIVALDI_SYNC);
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierHeader];
  [model addSectionWithIdentifier:SectionIdentifierEncryptionPassword];
  [model addSectionWithIdentifier:SectionIdentifierSaveButton];

  VivaldiTableViewIllustratedItem* header = [
    [VivaldiTableViewIllustratedItem alloc] initWithType:ItemTypeHeader
  ];
  header.image = [UIImage imageNamed:kVivaldiIcon];
  header.title =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_ENCRYPTION_PASSWORD_TITLE);
  NSMutableParagraphStyle* paragraphStyle =
      [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
  paragraphStyle.alignment = NSTextAlignmentCenter;
  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline],
    NSParagraphStyleAttributeName : paragraphStyle
  };
  header.subtitle = [[NSAttributedString alloc]
        initWithString:l10n_util::GetNSString(IDS_VIVALDI_DECRYPTION_TEXT)
        attributes:textAttributes];

  [model addItem:header
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

  TableViewTextButtonItem* loginButton =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeSaveButton];
  loginButton.buttonText =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_DECRYPT);
  loginButton.textAlignment = NSTextAlignmentNatural;
  loginButton.buttonBackgroundColor = [UIColor clearColor];
  loginButton.buttonTextColor = [UIColor colorNamed:kBlueColor];
  loginButton.disableButtonIntrinsicWidth = YES;

  [self.tableViewModel addItem:loginButton
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
      editCell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
    }
    case ItemTypeHeader:
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
    case ItemTypeError:
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
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
    if (![self hasErrorCell]) {
      // Insert cell at top of section
      [self.tableViewModel insertItem:
          [self errorCellItemWithMessage:l10n_util::GetNSString(
              IDS_VIVALDI_ACCOUNT_ENCRYPTION_PASSPHRASE_REQUIRED)]
          inSectionWithIdentifier:SectionIdentifierEncryptionPassword
          atIndex:0];
      [self reloadSection:SectionIdentifierEncryptionPassword];
    }
    return;
  } else if ([self hasErrorCell]) {
    [self removeErrorCell];
    [self reloadSection:SectionIdentifierEncryptionPassword];
  }

  [self.delegate decryptButtonPressed:
      self.encryptionPasswordItem.textFieldValue];
}

- (void)removeErrorCell {
  TableViewModel* model = self.tableViewModel;
  if ([self hasErrorCell]) {
    NSIndexPath* path =
        [model indexPathForItemType:ItemTypeError
                  sectionIdentifier:SectionIdentifierEncryptionPassword];
    [model removeItemWithType:ItemTypeError
        fromSectionWithIdentifier:SectionIdentifierEncryptionPassword];
    [self.tableView deleteRowsAtIndexPaths:@[ path ]
                          withRowAnimation:UITableViewRowAnimationAutomatic];
  }
}

- (BOOL)hasErrorCell {
  return [self.tableViewModel hasItemForItemType:ItemTypeError
      sectionIdentifier:SectionIdentifierEncryptionPassword];
}

- (void)reloadSection:(NSInteger)section {
    NSUInteger index = [self.tableViewModel
      sectionForSectionIdentifier:section];
    [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:index]
                withRowAnimation:UITableViewRowAnimationAutomatic];
}

- (TableViewItem*)errorCellItemWithMessage:(NSString*)errorMessage {
  PassphraseErrorItem* item =
      [[PassphraseErrorItem alloc] initWithType:ItemTypeError];
  item.text = errorMessage;
  return item;
}

@end
