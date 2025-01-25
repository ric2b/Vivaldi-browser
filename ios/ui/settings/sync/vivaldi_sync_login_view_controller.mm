// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/sync/vivaldi_sync_login_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_link_item.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/common/vivaldi_url_constants.h"
#import "ios/ui/modal_page/modal_page_commands.h"
#import "ios/ui/settings/sync/cells/vivaldi_table_view_text_edit_item.h"
#import "ios/ui/settings/sync/vivaldi_sync_settings_constants.h"
#import "ios/ui/table_view/cells/vivaldi_input_error_item.h"
#import "ios/ui/table_view/cells/vivaldi_table_view_illustrated_item.h"
#import "ios/ui/table_view/cells/vivaldi_table_view_text_spinner_button_item.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierHeader = kSectionIdentifierEnumZero,
  SectionIdentifierUsernamePassword,
  SectionIdentifierSavePassword,
  SectionIdentifierLoginButton,
  SectionIdentifierForgotUsernamePassword,
  SectionIdentifierUserDetails,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypeTitle,
  ItemTypeUsername,
  ItemTypePassword,
  ItemTypeSavePasswordSwitch,
  ItemTypeForgotUsernamePassword,
  ItemTypeDeviceName,
  ItemTypeLoginButton,
  ItemTypeError,
};

@interface VivaldiSyncLoginViewController () <
    UITextFieldDelegate,
    UITextViewDelegate,
    TableViewTextLinkCellDelegate>

@property(nonatomic, strong) VivaldiTableViewTextEditItem* usernameItem;
@property(nonatomic, strong) VivaldiTableViewTextEditItem* passwordItem;
@property(nonatomic, strong) TableViewSwitchItem* switchItemSavePassword;
@property(nonatomic, strong) VivaldiTableViewTextEditItem* deviceNameItem;
@property(nonatomic, strong) VivaldiTableViewTextSpinnerButtonItem* loginButton;

@property(nonatomic, weak) id<ModalPageCommands> modalPageHandler;

@end

@implementation VivaldiSyncLoginViewController
@synthesize delegate;

- (instancetype)initWithModalPageHandler:(id<ModalPageCommands>)modalPageHandler
                                   style:(UITableViewStyle)style {
  if ((self = [super initWithStyle:style])) {
    _modalPageHandler = modalPageHandler;
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
  self.title = l10n_util::GetNSString(IDS_VIVALDI_ACCOUNT_LOG_IN);
}

- (void)loginFailed:(NSString*)errorMessage {
  [self showErrorCellWithMessage:errorMessage
                         section:SectionIdentifierUsernamePassword
                        itemType:ItemTypeError];

  self.loginButton.activityInProgress = NO;
  [self reloadCellsForItems:@[ self.loginButton ]
           withRowAnimation:UITableViewRowAnimationNone];
}

- (void)setupLeftCancelButton {
  UIBarButtonItem* cancelButton =
      [[UIBarButtonItem alloc]
          initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                               target:self
                               action:@selector(dismissView)];
  self.customLeftBarButtonItem = cancelButton;
  self.navigationItem.leftBarButtonItem = self.customLeftBarButtonItem;
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierHeader];
  [model addSectionWithIdentifier:SectionIdentifierUsernamePassword];
  [model addSectionWithIdentifier:SectionIdentifierUserDetails];
  [model addSectionWithIdentifier:SectionIdentifierSavePassword];
  [model addSectionWithIdentifier:SectionIdentifierLoginButton];
  [model addSectionWithIdentifier:SectionIdentifierForgotUsernamePassword];

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
  // Using empty string since this is handled in the function
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
      [UIImage systemImageNamed:kShowPasswordIcon];
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

  self.switchItemSavePassword =
    [[TableViewSwitchItem alloc] initWithType:ItemTypeSavePasswordSwitch];
  self.switchItemSavePassword.text = l10n_util::GetNSString(
      IDS_VIVALDI_ACCOUNT_SAVE_CREDENTIALS);
  self.switchItemSavePassword.on = NO;
  [model addItem:self.switchItemSavePassword
      toSectionWithIdentifier:SectionIdentifierSavePassword];

  self.deviceNameItem =
      [[VivaldiTableViewTextEditItem alloc] initWithType:ItemTypeDeviceName];
  self.deviceNameItem.textFieldPlaceholder =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_DEVICE_NAME);
  self.deviceNameItem.hideIcon = YES;
  self.deviceNameItem.textFieldEnabled = YES;
  self.deviceNameItem.autoCapitalizationType = UITextAutocapitalizationTypeNone;
  self.deviceNameItem.identifyingIconAccessibilityLabel =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_DEVICE_NAME);
  self.deviceNameItem.keyboardType = UIKeyboardTypeDefault;
  [model addItem:self.deviceNameItem
      toSectionWithIdentifier:SectionIdentifierUserDetails];

  self.loginButton =
      [[VivaldiTableViewTextSpinnerButtonItem alloc]
          initWithType:ItemTypeLoginButton];
  self.loginButton.activityInProgress = NO;
  self.loginButton.buttonText =
      l10n_util::GetNSString(IDS_VIVALDI_ACCOUNT_LOG_IN);
  self.loginButton.textAlignment = NSTextAlignmentNatural;
  self.loginButton.buttonBackgroundColor = [UIColor colorNamed:kBlueColor];
  self.loginButton.buttonTextColor = [UIColor colorNamed:kSolidButtonTextColor];
  self.loginButton.cellBackgroundColor = _loginButton.buttonBackgroundColor;
  self.loginButton.disableButtonIntrinsicWidth = YES;

  [self.tableViewModel addItem:self.loginButton
      toSectionWithIdentifier:SectionIdentifierLoginButton];

  [self addForgotUsernamePasswordSection];
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  return 0;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:section];

  if (sectionIdentifier == SectionIdentifierForgotUsernamePassword) {
    return kPasswordSectionHeaderHeight;
  }

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
    case ItemTypeLoginButton: {
      VivaldiTableViewTextSpinnerButtonCell* tableViewTextButtonCell =
          base::apple::ObjCCastStrict<VivaldiTableViewTextSpinnerButtonCell>
              (cell);
      [tableViewTextButtonCell.button
                 addTarget:self
                    action:@selector(logInButtonPressed:)
          forControlEvents:UIControlEventTouchUpInside];
      [tableViewTextButtonCell
          setActivityIndicatorEnabled:self.loginButton.activityInProgress];
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
    case ItemTypeTitle: {
      VivaldiTableViewIllustratedCell* titleCell =
          base::apple::ObjCCast<VivaldiTableViewIllustratedCell>(cell);
      titleCell.subtitleLabel.delegate = self;
      break;
    }
    case ItemTypeSavePasswordSwitch: {
      TableViewSwitchCell* switchCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(savePasswordSwitchToggled)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeForgotUsernamePassword: {
      TableViewTextLinkCell* forgotUsernamePasswordCell =
          base::apple::ObjCCast<TableViewTextLinkCell>(cell);
      forgotUsernamePasswordCell.backgroundColor = nil;
      forgotUsernamePasswordCell.textView.font =
          [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
      forgotUsernamePasswordCell.textView.textAlignment = NSTextAlignmentCenter;
      forgotUsernamePasswordCell.delegate = self;
      break;
    }
    default:
      break;
  }

  return cell;
}

#pragma mark - UITextFieldDelegate

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [self logInButtonPressed:nil];
  return NO;
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  [self.delegate createAccountLinkPressed];

  // Return NO, we don't want to try to open the URL
  return NO;
}

#pragma mark - TableViewTextLinkCellDelegate
- (void)tableViewTextLinkCell:(TableViewTextLinkCell*)cell
            didRequestOpenURL:(CrURL*)URL {
  NSURL* recoverUsernameURL = [NSURL URLWithString:vVivaldiRecoverUsernameUrl];
  NSURL* recoverPasswordURL = [NSURL URLWithString:vVivaldiRecoverPasswordUrl];
  if ([URL.nsurl isEqual:recoverUsernameURL]) {
    [self.modalPageHandler
      showModalPage:recoverUsernameURL
              title:l10n_util::GetNSString(IDS_VIVALDI_RECOVER_USERNAME_TEXT)];
  } else if ([URL.nsurl isEqual:recoverPasswordURL]) {
    [self.modalPageHandler
      showModalPage:recoverPasswordURL
              title:l10n_util::GetNSString(IDS_VIVALDI_RECOVER_PASSOWRD_TEXT)];
  }
}

#pragma mark - Private Methods
- (void)addForgotUsernamePasswordSection {
  TableViewTextLinkItem* forgotUsernamePasswordItem =
    [[TableViewTextLinkItem alloc]
      initWithType:ItemTypeForgotUsernamePassword];

  std::vector<GURL> linkURLs;

  GURL gurlUsername([vVivaldiRecoverUsernameUrl UTF8String]);
  if (gurlUsername.is_valid()) {
    linkURLs.push_back(gurlUsername);
  }

  GURL gurlPassword([vVivaldiRecoverPasswordUrl UTF8String]);
  if (gurlPassword.is_valid()) {
    linkURLs.push_back(gurlPassword);
  }

  StringWithTags itemStringWithTags = ParseStringWithLinks(
       l10n_util::GetNSString(IDS_VIVALDI_FORGOT_USERNAME_PASSWORD_TEXT));
  forgotUsernamePasswordItem.text = itemStringWithTags.string;

  if (linkURLs.size() == 2 && itemStringWithTags.ranges.size() == 2) {
    forgotUsernamePasswordItem.linkURLs = linkURLs;
    forgotUsernamePasswordItem.linkRanges =
    @[ [NSValue valueWithRange:itemStringWithTags.ranges[0]],
       [NSValue valueWithRange:itemStringWithTags.ranges[1]] ];
    [self.tableViewModel addItem:forgotUsernamePasswordItem
         toSectionWithIdentifier:SectionIdentifierForgotUsernamePassword];
  }
}

- (void)togglePasswordMasking {
  self.passwordItem.textFieldSecureTextEntry =
      !self.passwordItem.textFieldSecureTextEntry;
  self.passwordItem.identifyingIcon =
      self.passwordItem.textFieldSecureTextEntry ?
        [UIImage systemImageNamed:kShowPasswordIcon] :
        [UIImage systemImageNamed:kHidePasswordIcon];
  [self reconfigureCellsForItems:@[ self.passwordItem ]];
}

- (void)savePasswordSwitchToggled {
  self.switchItemSavePassword.on = !self.switchItemSavePassword.on;
}

- (void)logInButtonPressed:(UIButton*)sender {
  NSString* errorMessage;

  if ([self.usernameItem.textFieldValue length] == 0) {
    errorMessage = l10n_util::GetNSString(IDS_VIVALDI_ACCOUNT_USERNAME_REQUIRED);
  } else if ([self.passwordItem.textFieldValue length] == 0) {
    errorMessage = l10n_util::GetNSString(IDS_VIVALDI_ACCOUNT_PASSWORD_REQUIRED);
  }

  [self removeErrorCell:SectionIdentifierUsernamePassword
               itemType:ItemTypeError];
  if (errorMessage) {
    [self showErrorCellWithMessage:errorMessage
                           section:SectionIdentifierUsernamePassword
                          itemType:ItemTypeError];
    return;
  }

  self.loginButton.activityInProgress = YES;
  [self reloadSection:SectionIdentifierLoginButton];

  [self.delegate logInButtonPressed:self.usernameItem.textFieldValue
                           password:self.passwordItem.textFieldValue
                         deviceName:self.deviceNameItem.textFieldValue
                       savePassword:self.switchItemSavePassword.on];
}

- (void)dismissView {
  [self.delegate dismissVivaldiSyncLoginViewController];
}

@end
