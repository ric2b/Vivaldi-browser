// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/sync/vivaldi_sync_create_account_password_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
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
  SectionIdentifierUserDetails,
  SectionIdentifierTermsAndNewsletter,
  SectionIdentifierExternalLinks,
  SectionIdentifierCreateButton,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypeTitle,
  ItemTypePassword,
  ItemTypeConfirmPassword,
  ItemTypeDeviceName,
  ItemTypeTOSSwitch,
  ItemTypeNewsletterSwitch,
  ItemTypeExternalLinks,
  ItemTypeCreateButton,
  ItemTypeError,
};

@interface VivaldiSyncCreateAccountPasswordViewController () <
    UITextFieldDelegate,
    UITextViewDelegate> {
NSString *username;
int age;
NSString* recoveryEmailAddress;
BOOL termsOfUseAccepted;
BOOL subscribeToNewsletter;
}

@property(nonatomic, strong) VivaldiTableViewTextEditItem* passwordItem;
@property(nonatomic, strong) VivaldiTableViewTextEditItem* confirmPasswordItem;
@property(nonatomic, strong) VivaldiTableViewTextEditItem* deviceNameItem;
@property(nonatomic, strong)
    VivaldiTableViewTextSpinnerButtonItem* createButton;
@property(nonatomic, strong) TableViewSwitchItem* termsOfUseSwitch;
@property(nonatomic, strong) TableViewSwitchItem* subscribeToNewsletterSwitch;

@property(nonatomic, weak) id<ModalPageCommands> modalPageHandler;

@end

@implementation VivaldiSyncCreateAccountPasswordViewController
@synthesize delegate;

- (instancetype)initWithModalPageHandler:(id<ModalPageCommands>)modalPageHandler
                                   style:(UITableViewStyle)style {
  DCHECK(modalPageHandler);
  if ((self = [super initWithStyle:style])) {
    _modalPageHandler = modalPageHandler;
    termsOfUseAccepted = NO;
    subscribeToNewsletter = NO;
  }
  return self;
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.delegate
        vivaldiSyncCreateAccountPasswordViewControllerWasRemoved:self];
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadModel];
  self.title = l10n_util::GetNSString(IDS_VIVALDI_SYNC_CREATE_ACCOUNT_TITLE);
}

- (void)createAccountFailed:(NSString*)errorCode {
  NSString* errorMessage;
  if ([errorCode containsString:vErrorCode1004]) {
    errorMessage = l10n_util::GetNSString(IDS_SYNC_EMAIL_ALREADY_REGISTERED);
  } else if ([errorCode containsString:vErrorCode1006]) {
    errorMessage = l10n_util::GetNSString(IDS_PASSWORD_INVALID_ERROR);
  } else if ([errorCode containsString:vErrorCode1007] ||
             [errorCode containsString:vErrorCode2001]) {
    errorMessage = l10n_util::GetNSString(IDS_SYNC_USER_EXISTS);
  } else if ([errorCode containsString:vErrorCode2002]) {
    errorMessage = l10n_util::GetNSString(IDS_EMAIL_NOT_ACCEPTED);
  } else if ([errorCode containsString:vErrorCode2003]) {
    errorMessage = l10n_util::GetNSString(IDS_SYNC_IP_BLOCKED);
  } else {
    errorMessage = l10n_util::GetNSString(IDS_SYNC_REGISTRATION_OTHER_ERROR);
  }

  if (errorMessage) {
    [self showErrorCellWithMessage:errorMessage
                           section:SectionIdentifierUserDetails
                          itemType:ItemTypeError];
  }
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierHeader];
  [model addSectionWithIdentifier:SectionIdentifierUserDetails];
  [model addSectionWithIdentifier:SectionIdentifierTermsAndNewsletter];
  [model addSectionWithIdentifier:SectionIdentifierExternalLinks];
  [model addSectionWithIdentifier:SectionIdentifierCreateButton];

  VivaldiTableViewIllustratedItem* title =
      [[VivaldiTableViewIllustratedItem alloc] initWithType:ItemTypeTitle];
  title.image = [UIImage imageNamed:kVivaldiIcon];
  title.title = l10n_util::GetNSString(IDS_VIVALDI_SYNC_CREATE_ACCOUNT_TITLE);

  [model addItem:title
      toSectionWithIdentifier:SectionIdentifierHeader];

  self.passwordItem =
      [[VivaldiTableViewTextEditItem alloc] initWithType:ItemTypePassword];
  self.passwordItem.textFieldPlaceholder =
      l10n_util::GetNSString(IDS_VIVALDI_ACCOUNT_PASSWORD);
  self.passwordItem.returnKeyType = UIReturnKeyDone;
  self.passwordItem.textFieldEnabled = YES;
  self.passwordItem.identifyingIcon =
      [UIImage systemImageNamed:kShowPasswordIcon];
  self.passwordItem.identifyingIconEnabled = YES;
  self.passwordItem.hideIcon = YES;
  self.passwordItem.textFieldEnabled = YES;
  self.passwordItem.textFieldSecureTextEntry = YES;
  self.passwordItem.autoCapitalizationType = UITextAutocapitalizationTypeNone;
  self.passwordItem.identifyingIconAccessibilityLabel = l10n_util::GetNSString(
      IDS_VIVALDI_ACCOUNT_PASSWORD);
  self.passwordItem.textContentType = UITextContentTypePassword;
  self.passwordItem.keyboardType = UIKeyboardTypeDefault;

  [model addItem:self.passwordItem
      toSectionWithIdentifier:SectionIdentifierUserDetails];

  self.confirmPasswordItem = [[VivaldiTableViewTextEditItem alloc]
      initWithType:ItemTypeConfirmPassword];
  self.confirmPasswordItem.textFieldPlaceholder =
      l10n_util::GetNSString(IDS_VIVALDI_CONFIRM_PASSWORD);
  self.confirmPasswordItem.returnKeyType = UIReturnKeyDone;
  self.confirmPasswordItem.textFieldEnabled = YES;
  self.confirmPasswordItem.identifyingIcon =
      [UIImage systemImageNamed:kShowPasswordIcon];
  self.confirmPasswordItem.identifyingIconEnabled = YES;
  self.confirmPasswordItem.hideIcon = YES;
  self.confirmPasswordItem.textFieldEnabled = YES;
  self.confirmPasswordItem.textFieldSecureTextEntry = YES;
  self.confirmPasswordItem.autoCapitalizationType =
      UITextAutocapitalizationTypeNone;
  self.confirmPasswordItem.identifyingIconAccessibilityLabel =
      l10n_util::GetNSString(IDS_VIVALDI_CONFIRM_PASSWORD);
  self.confirmPasswordItem.textContentType = UITextContentTypePassword;
  self.confirmPasswordItem.keyboardType = UIKeyboardTypeDefault;
  [model addItem:self.confirmPasswordItem
      toSectionWithIdentifier:SectionIdentifierUserDetails];

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

  self.termsOfUseSwitch =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeTOSSwitch];
  self.termsOfUseSwitch.on = NO;
  self.termsOfUseSwitch.text =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_TC);
  self.termsOfUseSwitch.accessibilityIdentifier =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_TC);
  [model addItem:self.termsOfUseSwitch
      toSectionWithIdentifier:SectionIdentifierTermsAndNewsletter];

  self.subscribeToNewsletterSwitch =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeNewsletterSwitch];
  self.subscribeToNewsletterSwitch.on = NO;
  self.subscribeToNewsletterSwitch.text =
      l10n_util::GetNSString(IDS_VIVALDI_SUBSCRIBE_NEWSLETTER);
  self.subscribeToNewsletterSwitch.accessibilityIdentifier =
      l10n_util::GetNSString(IDS_VIVALDI_SUBSCRIBE_NEWSLETTER);
  [model addItem:self.subscribeToNewsletterSwitch
      toSectionWithIdentifier:SectionIdentifierTermsAndNewsletter];


  VivaldiTableViewIllustratedItem* externalLinks =
    [[VivaldiTableViewIllustratedItem alloc] initWithType:ItemTypeExternalLinks];

  NSString* text = l10n_util::GetNSString(IDS_VIVALDI_TOS_AND_PP_EXTERNAL_LINKS);
  StringWithTags parsedString = ParseStringWithLinks(text);

  NSMutableParagraphStyle* paragraphStyle =
      [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
  paragraphStyle.alignment = NSTextAlignmentCenter;
  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline],
    NSParagraphStyleAttributeName : paragraphStyle,
  };

  NSMutableAttributedString* attributedText =
      [[NSMutableAttributedString alloc] initWithString:parsedString.string
                                             attributes:textAttributes];
  NSArray* urls = @[
    [NSURL URLWithString:vVivaldiTermsOfServiceUrl],
    [NSURL URLWithString:vVivaldiCommunityPrivacyUrl]
  ];
  DCHECK_EQ(parsedString.ranges.size(), [urls count]);
  size_t index = 0;
  for (CrURL* url in urls) {
    [attributedText addAttribute:NSLinkAttributeName
                           value:url
                           range:parsedString.ranges[index]];
    index += 1;
  }

  externalLinks.subtitle = attributedText;
  [model addItem:externalLinks
      toSectionWithIdentifier:SectionIdentifierExternalLinks];

  self.createButton =
      [[VivaldiTableViewTextSpinnerButtonItem alloc]
          initWithType:ItemTypeCreateButton];
  self.createButton.buttonText =
      l10n_util::GetNSString(IDS_VIVALDI_CREATE_ACCOUNT_CREATE);
  self.createButton.textAlignment = NSTextAlignmentNatural;
  self.createButton.buttonBackgroundColor = [UIColor colorNamed:kBlueColor];
  self.createButton.buttonTextColor =
      [UIColor colorNamed:kSolidButtonTextColor];
  self.createButton.cellBackgroundColor =
      self.createButton.buttonBackgroundColor;
  self.createButton.disableButtonIntrinsicWidth = YES;

  [self.tableViewModel addItem:self.createButton
      toSectionWithIdentifier:SectionIdentifierCreateButton];
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
    case ItemTypeCreateButton: {
      VivaldiTableViewTextSpinnerButtonCell* tableViewTextButtonCell =
          base::apple::ObjCCastStrict<VivaldiTableViewTextSpinnerButtonCell>
                                                                        (cell);
      [tableViewTextButtonCell.button
                 addTarget:self
                    action:@selector(createButtonPressed:)
          forControlEvents:UIControlEventTouchUpInside];
      [tableViewTextButtonCell
          setActivityIndicatorEnabled:self.createButton.activityInProgress];
      break;
    }
    case ItemTypePassword: {
      VivaldiTableViewTextEditCell* editCell =
          base::apple::ObjCCast<VivaldiTableViewTextEditCell>(cell);
      [editCell.identifyingIconButton addTarget:self
                                         action:@selector(togglePasswordMasking)
                               forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    case ItemTypeConfirmPassword: {
      VivaldiTableViewTextEditCell* editCell =
          base::apple::ObjCCast<VivaldiTableViewTextEditCell>(cell);
      [editCell.identifyingIconButton addTarget:self
                                  action:@selector(toggleConfirmPasswordMasking)
                        forControlEvents:UIControlEventTouchUpInside];
      editCell.textField.delegate = self;
      break;
    }
    case ItemTypeDeviceName: {
      VivaldiTableViewTextEditCell* editCell =
          base::apple::ObjCCast<VivaldiTableViewTextEditCell>(cell);
      editCell.textField.delegate = self;
      break;
    }
    case ItemTypeTOSSwitch: {
      TableViewSwitchCell* editCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
      [editCell.switchView addTarget:self
                              action:@selector(switchAction:)
                    forControlEvents:UIControlEventValueChanged];
      editCell.switchView.tag = itemType;
      break;
    }
    case ItemTypeNewsletterSwitch: {
      TableViewSwitchCell* editCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
      [editCell.switchView addTarget:self
                              action:@selector(switchAction:)
                    forControlEvents:UIControlEventValueChanged];
      editCell.switchView.tag = itemType;
      break;
    }
    case ItemTypeExternalLinks: {
      VivaldiTableViewIllustratedCell* editCell =
          base::apple::ObjCCast<VivaldiTableViewIllustratedCell>(cell);
      editCell.subtitleLabel.delegate = self;
      break;
    }
    default:
      break;
  }

  return cell;
}

#pragma mark - UITextFieldDelegate

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [self createButtonPressed:nil];
  return NO;
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  NSString* URLString = URL.absoluteString;
  if (URLString == vVivaldiTermsOfServiceUrl) {
    [self.modalPageHandler showModalPage:URL
          title:l10n_util::GetNSString(IDS_VIVALDI_TOS_TITLE)];
  } else if (URLString == vVivaldiCommunityPrivacyUrl) {
    [self.modalPageHandler showModalPage:URL
          title:l10n_util::GetNSString(IDS_VIVALDI_PRIVACY_TITLE)];
  }
  // Return NO, we don't want to try to open the URL
  return NO;
}

#pragma mark - Private Methods

- (void)switchAction:(UISwitch*)sender {
  if (sender.tag == ItemTypeNewsletterSwitch) {
    subscribeToNewsletter = sender.isOn;
  } else if (sender.tag == ItemTypeTOSSwitch) {
    termsOfUseAccepted = sender.isOn;
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

- (void)toggleConfirmPasswordMasking {
  self.confirmPasswordItem.textFieldSecureTextEntry =
      !self.confirmPasswordItem.textFieldSecureTextEntry;
  self.confirmPasswordItem.identifyingIcon =
      self.confirmPasswordItem.textFieldSecureTextEntry ?
        [UIImage systemImageNamed:kShowPasswordIcon] :
        [UIImage systemImageNamed:kHidePasswordIcon];
  [self reconfigureCellsForItems:@[ self.confirmPasswordItem ]];
}

- (void)createButtonPressed:(UIButton*)sender {
  NSString* errorMessage;

  if ([self.passwordItem.textFieldValue length] < vUserMinimumPasswordLength) {
    errorMessage = l10n_util::GetNSString(IDS_SYNC_PASSWORD_LENGTH_ERROR);
  } else if (![self.confirmPasswordItem.textFieldValue
                          isEqualToString:self.passwordItem.textFieldValue]) {
    errorMessage = l10n_util::GetNSString(IDS_SYNC_PASSWORD_MATCH_ERROR);
  } else if (!termsOfUseAccepted) {
    errorMessage = l10n_util::GetNSString(IDS_SYNC_TC_NOT_ACCEPTED_ERROR);
  }

  [self removeErrorCell:SectionIdentifierUserDetails
               itemType:ItemTypeError];
  if (errorMessage) {
    [self showErrorCellWithMessage:errorMessage
                          section:SectionIdentifierUserDetails
                          itemType:ItemTypeError];
    return;
  }

  self.createButton.activityInProgress = YES;
  [self reloadSection:SectionIdentifierCreateButton];

  [self.delegate createAccountButtonPressed:self.passwordItem.textFieldValue
                                 deviceName:self.deviceNameItem.textFieldValue
                            wantsNewsletter:subscribeToNewsletter];
}

@end
