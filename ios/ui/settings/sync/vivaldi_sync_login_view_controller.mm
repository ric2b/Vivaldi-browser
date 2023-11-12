// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/sync/vivaldi_sync_login_view_controller.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/settings/cells/passphrase_error_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/common/string_util.h"
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
  SectionIdentifierUsernamePassword,
  SectionIdentifierLoginButton,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypeTitle,
  ItemTypeUsername,
  ItemTypePassword,
  ItemTypeLoginButton,
  ItemTypeError,
};

@interface VivaldiSyncLoginViewController () <
    UITextFieldDelegate,
    UITextViewDelegate>

@property(nonatomic, strong) VivaldiTableViewTextEditItem* usernameItem;
@property(nonatomic, strong) VivaldiTableViewTextEditItem* passwordItem;

@end

@implementation VivaldiSyncLoginViewController
@synthesize delegate;

- (instancetype)initWithStyle:(UITableViewStyle)style {
  if ((self = [super initWithStyle:style])) {

  }
  return self;
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.delegate vivaldiSyncLoginViewControllerWasRemoved:self];
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadModel];
  self.title = l10n_util::GetNSString(IDS_PREFS_VIVALDI_SYNC);
}

- (void)loginFailed {
  if ([self hasErrorCell]) {
    [self removeErrorCell];
  }
  // Insert cell at top of section
  [self.tableViewModel insertItem:[self errorCellItemWithMessage:
      l10n_util::GetNSString(IDS_VIVALDI_ACCOUNT_LOG_IN_FAILED)]
      inSectionWithIdentifier:SectionIdentifierUsernamePassword
      atIndex:0];
  [self reloadSection:SectionIdentifierUsernamePassword];
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierHeader];
  [model addSectionWithIdentifier:SectionIdentifierUsernamePassword];
  [model addSectionWithIdentifier:SectionIdentifierLoginButton];

  VivaldiTableViewIllustratedItem* title =
      [[VivaldiTableViewIllustratedItem alloc] initWithType:ItemTypeTitle];
  title.image = [UIImage imageNamed:kVivaldiIcon];
  title.title = l10n_util::GetNSString(IDS_VIVALDI_ACCOUNT_LOG_IN_TITLE);

  NSMutableParagraphStyle* paragraphStyle =
      [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
  paragraphStyle.alignment = NSTextAlignmentCenter;
  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline],
    NSParagraphStyleAttributeName : paragraphStyle
  };
  // TODO(tomas@vivaldi.com): Use URL for create account flow
  NSDictionary* linkAttributes = @{
      NSLinkAttributeName : [NSURL URLWithString:@""],
  };

  auto subtitleText = AttributedStringFromStringWithLink(
      l10n_util::GetNSString(IDS_VIVALDI_CREATE_ACCOUNT_TEXT),
      textAttributes, linkAttributes);
  title.subtitle = subtitleText;

  [model addItem:title
      toSectionWithIdentifier:SectionIdentifierHeader];

  self.usernameItem =
      [[VivaldiTableViewTextEditItem alloc] initWithType:ItemTypeUsername];
  self.usernameItem.textFieldPlaceholder =
      l10n_util::GetNSString(IDS_VIVALDI_ACCOUNT_USERNAME);
  self.usernameItem.returnKeyType = UIReturnKeyDone;
  self.usernameItem.textFieldEnabled = YES;
  self.usernameItem.hideIcon = YES;
  self.usernameItem.autoCapitalizationType = UITextAutocapitalizationTypeNone;
  self.usernameItem.textContentType = UITextContentTypeUsername;
  self.usernameItem.keyboardType = UIKeyboardTypeEmailAddress;

  [model addItem:self.usernameItem
      toSectionWithIdentifier:SectionIdentifierUsernamePassword];

  self.passwordItem =
      [[VivaldiTableViewTextEditItem alloc] initWithType:ItemTypePassword];
  self.passwordItem.textFieldPlaceholder =
      l10n_util::GetNSString(IDS_VIVALDI_ACCOUNT_PASSWORD);
  self.passwordItem.identifyingIcon =
      [UIImage imageNamed:kShowPasswordIcon];
  self.passwordItem.identifyingIconEnabled = YES;
  self.passwordItem.hideIcon = YES;
  self.passwordItem.textFieldEnabled = YES;
  self.passwordItem.textFieldSecureTextEntry = YES;
  self.passwordItem.autoCapitalizationType = UITextAutocapitalizationTypeNone;
  self.passwordItem.textContentType = UITextContentTypePassword;
  self.passwordItem.identifyingIconAccessibilityLabel = l10n_util::GetNSString(
      IDS_VIVALDI_IOS_SETTINGS_SHOW_PASSWORD_HINT);
  [model addItem:self.passwordItem
      toSectionWithIdentifier:SectionIdentifierUsernamePassword];

  TableViewTextButtonItem* loginButton =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeLoginButton];
  loginButton.buttonText =
      l10n_util::GetNSString(IDS_VIVALDI_ACCOUNT_LOG_IN);
  loginButton.textAlignment = NSTextAlignmentNatural;
  loginButton.buttonBackgroundColor = [UIColor clearColor];
  loginButton.buttonTextColor = [UIColor colorNamed:kBlueColor];
  loginButton.disableButtonIntrinsicWidth = YES;

  [self.tableViewModel addItem:loginButton
      toSectionWithIdentifier:SectionIdentifierLoginButton];
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
    case ItemTypeLoginButton: {
      TableViewTextButtonCell* tableViewTextButtonCell =
          base::mac::ObjCCastStrict<TableViewTextButtonCell>(cell);
      [tableViewTextButtonCell.button
                 addTarget:self
                    action:@selector(logInButtonPressed:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    case ItemTypeUsername: {
      VivaldiTableViewTextEditCell* editCell =
          base::mac::ObjCCast<VivaldiTableViewTextEditCell>(cell);
      editCell.selectionStyle = UITableViewCellSelectionStyleNone;
      editCell.textField.delegate = self;
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
    case ItemTypeTitle: {
      VivaldiTableViewIllustratedCell* titleCell =
          base::mac::ObjCCast<VivaldiTableViewIllustratedCell>(cell);
      titleCell.selectionStyle = UITableViewCellSelectionStyleNone;
      titleCell.subtitleLabel.delegate = self;
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

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  if (URL) {
    // TODO(tomas@vivaldi.com): Implement create account flow
  }
  // Returns NO as the app is handling the opening of the URL.
  return NO;
}

#pragma mark - Private Methods

- (void)togglePasswordMasking {
  self.passwordItem.textFieldSecureTextEntry =
      !self.passwordItem.textFieldSecureTextEntry;
  [self reconfigureCellsForItems:@[ self.passwordItem ]];
}

- (void)logInButtonPressed:(UIButton*)sender {
  NSString* errorMessage;

  if ([self.usernameItem.textFieldValue length] == 0) {
    errorMessage = l10n_util::GetNSString(IDS_VIVALDI_ACCOUNT_USERNAME_REQUIRED);
  } else if ([self.passwordItem.textFieldValue length] == 0) {
    errorMessage = l10n_util::GetNSString(IDS_VIVALDI_ACCOUNT_PASSWORD_REQUIRED);
  }

  if (errorMessage) {
    if ([self hasErrorCell]) {
      [self removeErrorCell];
    }
    // Insert cell at top of section
    [self.tableViewModel insertItem:[self errorCellItemWithMessage:errorMessage]
        inSectionWithIdentifier:SectionIdentifierUsernamePassword
        atIndex:0];
    [self reloadSection:SectionIdentifierUsernamePassword];
    return;
  } else if ([self hasErrorCell]) {
    [self removeErrorCell];
    [self reloadSection:SectionIdentifierUsernamePassword];
  }

  [self.delegate logInButtonPressed:self.usernameItem.textFieldValue
                           password:self.passwordItem.textFieldValue
                       savePassword:YES];
}

- (NSInteger)indexOfItemWithType:(NSInteger)itemType {
  NSIndexPath* indexPath = [self.tableViewModel indexPathForItemType:itemType
      sectionIdentifier:SectionIdentifierUsernamePassword];
  return indexPath.row;
}

- (BOOL)hasErrorCell {
  return [self.tableViewModel hasItemForItemType:ItemTypeError
      sectionIdentifier:SectionIdentifierUsernamePassword];
}

- (void)removeErrorCell {
  TableViewModel* model = self.tableViewModel;
  if ([self hasErrorCell]) {
    NSIndexPath* path =
        [model indexPathForItemType:ItemTypeError
                  sectionIdentifier:SectionIdentifierUsernamePassword];
    [model removeItemWithType:ItemTypeError
        fromSectionWithIdentifier:SectionIdentifierUsernamePassword];
    [self.tableView deleteRowsAtIndexPaths:@[ path ]
                          withRowAnimation:UITableViewRowAnimationAutomatic];
  }
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
