// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/sync/vivaldi_sync_activate_account_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/language/core/browser/pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/common/vivaldi_url_constants.h"
#import "ios/ui/settings/sync/cells/vivaldi_table_view_link_and_button_item.h"
#import "ios/ui/settings/sync/cells/vivaldi_table_view_text_edit_item.h"
#import "ios/ui/settings/sync/vivaldi_create_account_ui_helper.h"
#import "ios/ui/settings/sync/vivaldi_sync_settings_constants.h"
#import "ios/ui/table_view/cells/vivaldi_input_error_item.h"
#import "ios/ui/table_view/cells/vivaldi_table_view_illustrated_item.h"
#import "ios/ui/table_view/cells/vivaldi_table_view_text_spinner_button_item.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierHeader = kSectionIdentifierEnumZero,
  SectionIdentifierActivationCode,
  SectionIdentifierSaveButton,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypeActivationCode,
  ItemTypeSaveButton,
  ItemTypeError,
};

@interface VivaldiSyncActivateAccountViewController () <
    UITextFieldDelegate,
    UITextViewDelegate>

@property(nonatomic, strong) VivaldiTableViewTextEditItem* activationCodeItem;
@property(nonatomic, copy) NSURLSessionDataTask* task;

@end

@implementation VivaldiSyncActivateAccountViewController
@synthesize delegate;

- (instancetype)initWithStyle:(UITableViewStyle)style {
  if ((self = [super initWithStyle:style])) {

  }
  return self;
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.delegate vivaldiSyncActivateAccountViewControllerWasRemoved:self];
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadModel];
  self.title =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_ENCRYPTION_PASSWORD_TITLE);
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierHeader];
  [model addSectionWithIdentifier:SectionIdentifierActivationCode];
  [model addSectionWithIdentifier:SectionIdentifierSaveButton];

  VivaldiTableViewIllustratedItem* header = [
    [VivaldiTableViewIllustratedItem alloc] initWithType:ItemTypeHeader
  ];
  header.image = [UIImage imageNamed:kVivaldiIcon];
  header.title =
      l10n_util::GetNSString(IDS_VIVALDI_ACTIVATE_ACCOUNT_TITLE);
  NSMutableParagraphStyle* paragraphStyle =
      [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
  paragraphStyle.alignment = NSTextAlignmentCenter;
  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline],
    NSParagraphStyleAttributeName : paragraphStyle
  };
  NSString* recoveryEmailAddress = [self.delegate getPendingRegistrationEmail];
  header.subtitle = [[NSAttributedString alloc]
      initWithString:l10n_util::GetNSStringF(IDS_VIVALDI_VERIFICATION_CODE_TEXT,
                      base::SysNSStringToUTF16(recoveryEmailAddress))
      attributes:textAttributes];

  [model addItem:header
      toSectionWithIdentifier:SectionIdentifierHeader];

  self.activationCodeItem =
      [[VivaldiTableViewTextEditItem alloc] initWithType:ItemTypeActivationCode];
  self.activationCodeItem.textFieldPlaceholder =
      l10n_util::GetNSString(IDS_VIVALDI_VERIFICATION_CODE_HINT);
  self.activationCodeItem.hideIcon = YES;
  self.activationCodeItem.textFieldEnabled = YES;
  self.activationCodeItem.textFieldSecureTextEntry = YES;
  self.activationCodeItem.autoCapitalizationType =
      UITextAutocapitalizationTypeNone;
  self.activationCodeItem.identifyingIconAccessibilityLabel =
    l10n_util::GetNSString(IDS_VIVALDI_VERIFICATION_CODE_HINT);
  [model addItem:self.activationCodeItem
      toSectionWithIdentifier:SectionIdentifierActivationCode];

  VivaldiTableViewLinkAndButtonItem* linkAndButton =
    [[VivaldiTableViewLinkAndButtonItem alloc] initWithType:ItemTypeSaveButton];
  linkAndButton.buttonText =
      l10n_util::GetNSString(IDS_VIVALDI_ACTIVATE_ACCOUNT_BUTTON);
  linkAndButton.textAlignment = NSTextAlignmentCenter;

  NSDictionary* linkAttributes = @{
      NSLinkAttributeName : [NSURL URLWithString:@""],
  };

  auto linkText = AttributedStringFromStringWithLink(
      l10n_util::GetNSString(IDS_VIVALDI_RESEND_CODE),
      textAttributes, linkAttributes);

  linkAndButton.linkText = linkText;

  [self.tableViewModel addItem:linkAndButton
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
  if (itemType == ItemTypeSaveButton) {
    VivaldiTableViewLinkAndButtonCell* editCell =
        base::apple::ObjCCastStrict<VivaldiTableViewLinkAndButtonCell>(cell);
    [editCell.button
                addTarget:self
                  action:@selector(activateButtonPressed:)
        forControlEvents:UIControlEventTouchUpInside];
    editCell.label.delegate = self;
  }
  if (itemType == ItemTypeActivationCode) {
    VivaldiTableViewTextEditCell* editCell =
        base::apple::ObjCCastStrict<VivaldiTableViewTextEditCell>(cell);
    editCell.textField.delegate = self;
  }
  return cell;
}

#pragma mark - UITextFieldDelegate

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [self activateButtonPressed:nil];
  return NO;
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  std::string username =
      base::SysNSStringToUTF8([self.delegate getPendingRegistrationUsername]);

  PrefService* pref_service = GetApplicationContext()->GetLocalState();
  std::string locale =
    pref_service->HasPrefPath(language::prefs::kApplicationLocale)
        ? pref_service->GetString(language::prefs::kApplicationLocale)
        : GetApplicationContext()->GetApplicationLocale();

  base::Value::Dict dict;
  dict.Set(vParamUsername, username);
  dict.Set(vParamLanguage, locale);

  NSURL* url = [NSURL URLWithString:vVivaldiSyncReSendActivationCodeUrl];
  __weak __typeof__(self) weakSelf = self;
  sendRequestToServer(std::move(dict), url,
      ^(NSData* data, NSURLResponse* response,
                                NSError* error) {
        dispatch_async(dispatch_get_main_queue(), ^{
          [weakSelf onResendCodeResponse:data
                              response:response
                                  error:error];
        });
      }, self.task);

  // Returns NO as the app is handling the opening of the URL.
  return NO;
}

#pragma mark - Private Methods

- (void)onResendCodeResponse:(NSData*)data
                    response:(NSURLResponse*)response
                       error:(NSError*)error {
  std::optional<base::Value> readResult = NSDataToDict(data);
  if (error || !readResult.has_value()) {
    [self showErrorCellWithMessage:
        l10n_util::GetNSString(IDS_SYNC_SERVER_OTHER_ERROR)];
    return;
  }

  [self removeErrorCell:SectionIdentifierActivationCode itemType:ItemTypeError];

  base::Value val = std::move(readResult).value();
  const base::Value::Dict& dict = val.GetDict();
  if (dict.FindString(vSuccessKey)) {
    [self showErrorCellWithMessage:
        l10n_util::GetNSString(IDS_VIVALDI_SYNC_RESEND_CODE_SUCCESS)
                           section:SectionIdentifierActivationCode
                          itemType:ItemTypeError
                         textColor:[UIColor colorNamed:kTextPrimaryColor]];
  } else {
    [self showErrorCellWithMessage:
        l10n_util::GetNSString(IDS_VIVALDI_SYNC_RESEND_CODE_FAILED)];
  }
}

- (void)onActivateResponse:(NSData*)data
                  response:(NSURLResponse*)response
                     error:(NSError*)error {
  std::optional<base::Value> readResult = NSDataToDict(data);
  if (error || !readResult.has_value()) {
    [self showErrorCellWithMessage:
        l10n_util::GetNSString(IDS_SYNC_SERVER_OTHER_ERROR)];
    return;
  }

  [self removeErrorCell:SectionIdentifierActivationCode itemType:ItemTypeError];

  base::Value val = std::move(readResult).value();
  const base::Value::Dict& dict = val.GetDict();
  if (dict.FindString(vSuccessKey)) {
        [self.delegate requestPendingRegistrationLogin];
        [self.delegate clearPendingRegistration];
  } else {
    NSString* errorMessage;
    NSString* e = base::SysUTF8ToNSString(*(dict.FindString(vErrorKey)));
    if ([e containsString:vErrorCode4001]) {
      int attempts = dict.FindInt(vAttemptsRemainingKey).value_or(0);

      NSMutableParagraphStyle* paragraphStyle =
          [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
      paragraphStyle.alignment = NSTextAlignmentCenter;
      NSDictionary* textAttributes = @{
        NSForegroundColorAttributeName :
            [UIColor colorNamed:kTextSecondaryColor],
        NSFontAttributeName :
            [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline],
        NSParagraphStyleAttributeName : paragraphStyle
      };
      NSAttributedString* errorMsg = [[NSAttributedString alloc]
        initWithString:
            l10n_util::GetNSStringF(IDS_VIVALDI_SYNC_CODE_DOESNT_MATCH,
            base::SysNSStringToUTF16([@(attempts) stringValue]))
        attributes:textAttributes];
      errorMessage = [errorMsg string];
      if (attempts == 0) {
        errorMessage =
          l10n_util::GetNSString(IDS_SYNC_ACTIVATION_CODE_MAX_LIMIT_REACHED);
      }
    } else if ([e containsString:vErrorCode4002]) {
      errorMessage =
          l10n_util::GetNSString(IDS_VIVALDI_SYNC_CODE_EXPIRED);
    } else if ([e containsString:vErrorCode4003]) {
      errorMessage =
          l10n_util::GetNSString(IDS_VIVALDI_SYNC_ACCOUNT_LOCKED);
    } else {
      errorMessage =
          l10n_util::GetNSString(IDS_SYNC_SERVER_OTHER_ERROR);
    }
    if (errorMessage)
      [self showErrorCellWithMessage:errorMessage];
  }
}

- (void)activateButtonPressed:(UIButton*)sender {
  NSString* errorMessage;

  if ([self.activationCodeItem.textFieldValue length] == 0) {
    errorMessage = l10n_util::GetNSString(IDS_VIVALDI_ACTIVATION_CODE_REQUIRED);
  } else if (
    [self.activationCodeItem.textFieldValue length] != vActivationCodeLength) {
    errorMessage = l10n_util::GetNSString(IDS_SYNC_ACTIVATION_CODE_LENGTH_ERROR);
  }

  [self removeErrorCell:SectionIdentifierActivationCode
               itemType:ItemTypeError];
  if (errorMessage) {
    [self showErrorCellWithMessage:errorMessage];
    return;
  }

  std::string username =
      base::SysNSStringToUTF8([self.delegate getPendingRegistrationUsername]);
  int code;
  base::StringToInt(
      base::SysNSStringToUTF8(self.activationCodeItem.textFieldValue), &code);

  PrefService* pref_service = GetApplicationContext()->GetLocalState();
  std::string locale =
    pref_service->HasPrefPath(language::prefs::kApplicationLocale)
        ? pref_service->GetString(language::prefs::kApplicationLocale)
        : GetApplicationContext()->GetApplicationLocale();

  base::Value::Dict dict;
  dict.Set(vParamUsername, username);
  dict.Set(vParamActivationCode, code);
  dict.Set(vParamLanguage, locale);

  NSURL* url = [NSURL URLWithString:vVivaldiSyncActivateAccountUrl];
  __weak __typeof__(self) weakSelf = self;
  sendRequestToServer(std::move(dict), url,
      ^(NSData* data, NSURLResponse* response,
                                NSError* error) {
        dispatch_async(dispatch_get_main_queue(), ^{
          [weakSelf onActivateResponse:data
                              response:response
                                  error:error];
        });
      }, self.task);
}

- (void)showErrorCellWithMessage:(NSString*)message {
  [self showErrorCellWithMessage:message
                         section:SectionIdentifierActivationCode
                        itemType:ItemTypeError];
}

@end
