// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/sync/vivaldi_sync_create_encryption_password_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/settings/sync/cells/vivaldi_table_view_link_and_button_item.h"
#import "ios/ui/settings/sync/cells/vivaldi_table_view_text_edit_item.h"
#import "ios/ui/settings/sync/vivaldi_sync_settings_constants.h"
#import "ios/ui/table_view/cells/vivaldi_input_error_item.h"
#import "ios/ui/table_view/cells/vivaldi_table_view_illustrated_item.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierHeader = kSectionIdentifierEnumZero,
  SectionIdentifierEncryptionPassword,
  SectionIdentifierSaveButton,
  SectionIdentifierLogOutButton,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypePassword,
  ItemTypeSaveButton,
  ItemTypeLogOutButton,
  ItemTypeError,
};

@interface VivaldiSyncCreateEncryptionPasswordViewController () <
    UITextFieldDelegate,
    UITableViewDelegate>

@property(nonatomic, strong) VivaldiTableViewTextEditItem* encryptionPasswordItem;
@property(nonatomic, strong) VivaldiTableViewIllustratedItem* header;
@property(nonatomic, strong) VivaldiTableViewLinkAndButtonItem* saveButton;

@end

@implementation VivaldiSyncCreateEncryptionPasswordViewController
@synthesize delegate;

- (instancetype)initWithStyle:(UITableViewStyle)style {
  if ((self = [super initWithStyle:style])) {}
  return self;
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.delegate
        vivaldiSyncCreateEncryptionPasswordViewControllerWasRemoved:self];
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadModel];
  self.title =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_ENCRYPTION_PASSWORD_TITLE);
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
      initWithString:l10n_util::GetNSString(IDS_VIVALDI_SET_UP_ENCRYPTION_DESC)
      attributes:textAttributes];
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierHeader];
  [model addSectionWithIdentifier:SectionIdentifierEncryptionPassword];
  [model addSectionWithIdentifier:SectionIdentifierSaveButton];
  [model addSectionWithIdentifier:SectionIdentifierLogOutButton];

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
      [UIImage systemImageNamed:kShowPasswordIcon];
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

  _saveButton = [[VivaldiTableViewLinkAndButtonItem alloc]
      initWithType:ItemTypeSaveButton];
  _saveButton.buttonText = l10n_util::GetNSString(IDS_VIVALDI_SAVE);
  _saveButton.textAlignment = NSTextAlignmentNatural;

  [self.tableViewModel addItem:_saveButton
      toSectionWithIdentifier:SectionIdentifierSaveButton];
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  return 0;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  return kCommonHeaderSectionHeight;
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
      VivaldiTableViewLinkAndButtonCell* editCell =
          base::apple::ObjCCastStrict<VivaldiTableViewLinkAndButtonCell>(cell);
      [editCell.button addTarget:self
                          action:@selector(saveButtonPressed:)
                forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    case ItemTypePassword: {
      VivaldiTableViewTextEditCell* editCell =
          base::apple::ObjCCast<VivaldiTableViewTextEditCell>(cell);
      [editCell.identifyingIconButton addTarget:self
                                         action:@selector(togglePasswordMasking)
                               forControlEvents:UIControlEventTouchUpInside];
      editCell.textField.delegate = self;
      break;
    }
    default:
      break;
  }

  return cell;
}

#pragma mark - UITextFieldDelegate

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [self saveButtonPressed:nil];
  return NO;
}

#pragma mark - Private Methods

- (void)togglePasswordMasking {
  self.encryptionPasswordItem.textFieldSecureTextEntry =
      !self.encryptionPasswordItem.textFieldSecureTextEntry;
  [self reconfigureCellsForItems:@[ self.encryptionPasswordItem ]];
}

- (void)saveButtonPressed:(UIButton*)sender {
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

  __weak __typeof__(self) weakSelf = self;
  [self.delegate
      saveEncryptionKeyButtonPressed:self.encryptionPasswordItem.textFieldValue
                   completionHandler:^(BOOL success) {
    dispatch_async(dispatch_get_main_queue(), ^{
      if (!success) {
        [weakSelf showPasswordWrongMessage];
      }
    });
  }];
}

- (void)showPasswordWrongMessage {
  [self showErrorCellWithMessage:l10n_util::GetNSString(
      IDS_VIVALDI_ACCOUNT_ENCRYPTION_PASSPHRASE_WRONG)
                      section:SectionIdentifierEncryptionPassword
                    itemType:ItemTypeError];
}

@end
