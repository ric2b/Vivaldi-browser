// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/sync/vivaldi_sync_encryption_password_view_controller.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/settings/sync/cells/vivaldi_table_view_link_and_button_item.h"
#import "ios/ui/settings/sync/cells/vivaldi_table_view_text_edit_item.h"
#import "ios/ui/settings/sync/vivaldi_create_account_ui_helper.h"
#import "ios/ui/settings/sync/vivaldi_sync_settings_constants.h"
#import "ios/ui/table_view/cells/vivaldi_input_error_item.h"
#import "ios/ui/table_view/cells/vivaldi_table_view_illustrated_item.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierHeader = kSectionIdentifierEnumZero,
  SectionIdentifierEncryptionPassword,
  SectionIdentifierDecryptButton,
  SectionIdentifierLogOutButton,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypePassword,
  ItemTypeDecryptButton,
  ItemTypeLogOutButton,
  ItemTypeError,
};

@interface VivaldiSyncEncryptionPasswordViewController () <
    UITextFieldDelegate,
    UITableViewDelegate,
    UITextViewDelegate,
    UIDocumentPickerDelegate>

@property(nonatomic, strong) VivaldiTableViewTextEditItem* encryptionPasswordItem;
@property(nonatomic, strong) VivaldiTableViewIllustratedItem* header;

@end

@implementation VivaldiSyncEncryptionPasswordViewController
@synthesize delegate;

- (instancetype)initWithStyle:(UITableViewStyle)style {
  if ((self = [super initWithStyle:style])) {}
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
        initWithString:l10n_util::GetNSString(IDS_VIVALDI_DECRYPTION_TEXT)
        attributes:textAttributes];
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierHeader];
  [model addSectionWithIdentifier:SectionIdentifierEncryptionPassword];
  [model addSectionWithIdentifier:SectionIdentifierDecryptButton];
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

  VivaldiTableViewLinkAndButtonItem* linkAndButton =
    [[VivaldiTableViewLinkAndButtonItem alloc]
        initWithType:ItemTypeDecryptButton];
  linkAndButton.reverseOrder = YES;
  linkAndButton.buttonText =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_DECRYPT);
  linkAndButton.textAlignment = NSTextAlignmentCenter;

  NSDictionary* linkAttributes = @{
      NSLinkAttributeName : [NSURL URLWithString:@""],
  };
  NSMutableParagraphStyle* paragraphStyle =
      [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
  paragraphStyle.alignment = NSTextAlignmentCenter;
  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline],
    NSParagraphStyleAttributeName : paragraphStyle
  };
  auto linkText = AttributedStringFromStringWithLink(
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_IMPORT_ENCRYPTION_KEY),
      textAttributes, linkAttributes);

  linkAndButton.linkText = linkText;

  [self.tableViewModel addItem:linkAndButton
      toSectionWithIdentifier:SectionIdentifierDecryptButton];

  VivaldiTableViewLinkAndButtonItem* logOutButton =
      [[VivaldiTableViewLinkAndButtonItem alloc]
          initWithType:ItemTypeLogOutButton];
  logOutButton.buttonText =
      l10n_util::GetNSString(IDS_VIVALDI_ACCOUNT_LOG_OUT);
  logOutButton.textAlignment = NSTextAlignmentNatural;
  logOutButton.buttonBackgroundColor = [UIColor colorNamed:kBlueHaloColor];
  logOutButton.buttonTextColor = [UIColor colorNamed:kBlueColor];

  [self.tableViewModel addItem:logOutButton
      toSectionWithIdentifier:SectionIdentifierLogOutButton];
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:section];

  if (sectionIdentifier == SectionIdentifierEncryptionPassword) {
    return kPasswordSectionFooterHeight;
  }
  return kFooterSectionHeight;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:section];

  if (sectionIdentifier == SectionIdentifierHeader) {
    return kHeaderSectionHeight;
  }
  if (sectionIdentifier == SectionIdentifierLogOutButton) {
    return kLogoutSectionHeaderHeight;
  }
  if (sectionIdentifier == SectionIdentifierDecryptButton) {
    return kDecryptSectionHeaderHeight;
  }
  return 0;
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
    case ItemTypeDecryptButton: {
      VivaldiTableViewLinkAndButtonCell* editCell =
          base::apple::ObjCCastStrict<VivaldiTableViewLinkAndButtonCell>(cell);
      [editCell.button addTarget:self
                          action:@selector(decryptButtonPressed:)
                forControlEvents:UIControlEventTouchUpInside];
      editCell.label.delegate = self;
      break;
    }
    case ItemTypeLogOutButton: {
      VivaldiTableViewLinkAndButtonCell* editCell =
          base::apple::ObjCCastStrict<VivaldiTableViewLinkAndButtonCell>(cell);
      [editCell.button addTarget:self
                          action:@selector(logOutButtonPressed:)
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

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  UIDocumentPickerViewController* picker =
      [[UIDocumentPickerViewController alloc]
        initForOpeningContentTypes:@[[UTType typeWithFilenameExtension:@"txt"]]];
  picker.directoryURL =
      [NSURL fileURLWithPath:GetDocumentsFolderPath() isDirectory:YES];
  picker.delegate = self;

  [self presentViewController:picker animated:YES completion:nil];

  // Returns NO as the app is handling it.
  return NO;
}

#pragma mark - UITextFieldDelegate

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [self decryptButtonPressed:nil];
  return NO;
}

#pragma mark - UIDocumentPickerDelegate

- (void)documentPicker:(UIDocumentPickerViewController*)controller
    didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {

  if ([urls count] < 1)
    return;
  NSURL* fileSelected = [urls objectAtIndex:0];

  if (![fileSelected startAccessingSecurityScopedResource]) {
    [self showErrorCellWithMessage:l10n_util::GetNSString(
      IDS_VIVALDI_SYNC_ENCRYPTION_FILE_PERMISSION_ERROR)
                           section:SectionIdentifierEncryptionPassword
                          itemType:ItemTypeError];
    [fileSelected stopAccessingSecurityScopedResource];
    return;
  }

  __weak __typeof__(self) weakSelf = self;
  [self.delegate importPasskey:fileSelected
      completionHandler:^(BOOL success) {
    dispatch_async(dispatch_get_main_queue(), ^{
      if (!success) {
        [weakSelf showPasswordWrongMessage];
      }
    });
  }];
}

#pragma mark - Private Methods

- (void)logOutButtonPressed:(UIButton*)sender {
  [self.delegate logOutButtonPressed];
}

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

  __weak __typeof__(self) weakSelf = self;
  [self.delegate decryptButtonPressed:self.encryptionPasswordItem.textFieldValue
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
