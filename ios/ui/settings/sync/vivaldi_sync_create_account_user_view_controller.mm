// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/sync/vivaldi_sync_create_account_user_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/common/vivaldi_url_constants.h"
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
  SectionIdentifierUserDetails,
  SectionIdentifierNextButton,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypeTitle,
  ItemTypeUsername,
  ItemTypeAge,
  ItemTypeRecoveryEmail,
  ItemTypeConfirmRecoveryEmail,
  ItemTypeNextButton,
  ItemTypeError,
};

@interface VivaldiSyncCreateAccountUserViewController () <
    UITextFieldDelegate,
    UITextViewDelegate> {

BOOL usernameIsValid;
BOOL emailIsValid;
}

@property(nonatomic, strong) VivaldiTableViewTextEditItem* usernameItem;
@property(nonatomic, strong) VivaldiTableViewTextEditItem* ageItem;
@property(nonatomic, strong) VivaldiTableViewTextEditItem* recoveryEmailItem;
@property(nonatomic, strong) VivaldiTableViewTextEditItem*
    confirmRecoveryEmailItem;
@property(nonatomic, strong) VivaldiTableViewTextSpinnerButtonItem* nextButton;

@property(nonatomic, copy) NSURLSessionDataTask* task;

@end

@implementation VivaldiSyncCreateAccountUserViewController
@synthesize delegate;

- (instancetype)initWithStyle:(UITableViewStyle)style {
  if ((self = [super initWithStyle:style])) {
    usernameIsValid = NO;
    emailIsValid = NO;
  }
  return self;
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.delegate vivaldiSyncCreateAccountUserViewControllerWasRemoved:self];
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadModel];
  self.title = l10n_util::GetNSString(IDS_VIVALDI_SYNC_CREATE_ACCOUNT_TITLE);
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierHeader];
  [model addSectionWithIdentifier:SectionIdentifierUserDetails];
  [model addSectionWithIdentifier:SectionIdentifierNextButton];

  VivaldiTableViewIllustratedItem* title =
      [[VivaldiTableViewIllustratedItem alloc] initWithType:ItemTypeTitle];
  title.image = [UIImage imageNamed:kVivaldiIcon];
  title.title = l10n_util::GetNSString(IDS_VIVALDI_SYNC_CREATE_ACCOUNT_TITLE);

  NSMutableParagraphStyle* paragraphStyle =
      [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
  paragraphStyle.alignment = NSTextAlignmentCenter;
  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline],
    NSParagraphStyleAttributeName : paragraphStyle
  };
  NSDictionary* linkAttributes = @{
      NSLinkAttributeName : [NSURL URLWithString:@""],
  };

  auto subtitleText = AttributedStringFromStringWithLink(
      l10n_util::GetNSString(IDS_VIVALDI_LOG_IN_TEXT),
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
  self.usernameItem.identifyingIconAccessibilityLabel = l10n_util::GetNSString(
      IDS_VIVALDI_ACCOUNT_USERNAME);
  self.usernameItem.textContentType = UITextContentTypeUsername;
  self.usernameItem.keyboardType = UIKeyboardTypeDefault;

  [model addItem:self.usernameItem
      toSectionWithIdentifier:SectionIdentifierUserDetails];

  self.ageItem =
      [[VivaldiTableViewTextEditItem alloc] initWithType:ItemTypeAge];
  self.ageItem.textFieldPlaceholder =
      l10n_util::GetNSString(IDS_VIVALDI_USER_AGE);
  self.ageItem.returnKeyType = UIReturnKeyDone;
  self.ageItem.hideIcon = YES;
  self.ageItem.textFieldEnabled = YES;
  self.ageItem.autoCapitalizationType = UITextAutocapitalizationTypeNone;
  self.ageItem.identifyingIconAccessibilityLabel = l10n_util::GetNSString(
      IDS_VIVALDI_USER_AGE);
  self.ageItem.keyboardType = UIKeyboardTypeNumberPad;
  [model addItem:self.ageItem
      toSectionWithIdentifier:SectionIdentifierUserDetails];

  self.recoveryEmailItem =
      [[VivaldiTableViewTextEditItem alloc] initWithType:ItemTypeRecoveryEmail];
  self.recoveryEmailItem.textFieldPlaceholder =
      l10n_util::GetNSString(IDS_VIVALDI_RECOVERY_EMAIL);
  self.recoveryEmailItem.hideIcon = YES;
  self.recoveryEmailItem.textFieldEnabled = YES;
  self.recoveryEmailItem.autoCapitalizationType =
      UITextAutocapitalizationTypeNone;
  self.recoveryEmailItem.identifyingIconAccessibilityLabel =
      l10n_util::GetNSString(IDS_VIVALDI_RECOVERY_EMAIL);
  self.recoveryEmailItem.textContentType = UITextContentTypeEmailAddress;
  self.recoveryEmailItem.keyboardType = UIKeyboardTypeEmailAddress;
  [model addItem:self.recoveryEmailItem
      toSectionWithIdentifier:SectionIdentifierUserDetails];

  self.confirmRecoveryEmailItem = [[VivaldiTableViewTextEditItem alloc]
      initWithType:ItemTypeConfirmRecoveryEmail];
  self.confirmRecoveryEmailItem.textFieldPlaceholder =
      l10n_util::GetNSString(IDS_VIVALDI_CONFIRM_RECOVERY_EMAIL);
  self.confirmRecoveryEmailItem.hideIcon = YES;
  self.confirmRecoveryEmailItem.textFieldEnabled = YES;
  self.confirmRecoveryEmailItem.autoCapitalizationType =
      UITextAutocapitalizationTypeNone;
  self.confirmRecoveryEmailItem.identifyingIconAccessibilityLabel =
      l10n_util::GetNSString(IDS_VIVALDI_CONFIRM_RECOVERY_EMAIL);
  self.confirmRecoveryEmailItem.textContentType = UITextContentTypeEmailAddress;
  self.confirmRecoveryEmailItem.keyboardType = UIKeyboardTypeEmailAddress;
  [model addItem:self.confirmRecoveryEmailItem
      toSectionWithIdentifier:SectionIdentifierUserDetails];

  self.nextButton =
      [[VivaldiTableViewTextSpinnerButtonItem alloc]
          initWithType:ItemTypeNextButton];
  self.nextButton.buttonText =
      l10n_util::GetNSString(IDS_VIVALDI_CREATE_ACCOUNT_NEXT);
  self.nextButton.textAlignment = NSTextAlignmentNatural;
  self.nextButton.buttonBackgroundColor = [UIColor colorNamed:kBlueColor];
  self.nextButton.buttonTextColor = [UIColor colorNamed:kSolidButtonTextColor];
  self.nextButton.cellBackgroundColor = self.nextButton.buttonBackgroundColor;
  self.nextButton.disableButtonIntrinsicWidth = YES;

  [self.tableViewModel addItem:self.nextButton
      toSectionWithIdentifier:SectionIdentifierNextButton];
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
    case ItemTypeNextButton: {
      VivaldiTableViewTextSpinnerButtonCell* tableViewTextButtonCell =
          base::apple::ObjCCastStrict<VivaldiTableViewTextSpinnerButtonCell>
                                                                        (cell);
      [tableViewTextButtonCell.button
                 addTarget:self
                    action:@selector(nextButtonPressed:)
          forControlEvents:UIControlEventTouchUpInside];
      [tableViewTextButtonCell
          setActivityIndicatorEnabled:self.nextButton.activityInProgress];
      break;
    }
    case ItemTypeUsername: {
      VivaldiTableViewTextEditCell* editCell =
          base::apple::ObjCCast<VivaldiTableViewTextEditCell>(cell);
      [editCell.textField addTarget:self
                     action:@selector(usernameChanged:)
           forControlEvents:UIControlEventEditingChanged];
      break;
    }
    case ItemTypeRecoveryEmail: {
      VivaldiTableViewTextEditCell* editCell =
          base::apple::ObjCCast<VivaldiTableViewTextEditCell>(cell);
      [editCell.textField addTarget:self
                     action:@selector(recoveryEmailChanged:)
           forControlEvents:UIControlEventEditingChanged];
      break;
    }
    case ItemTypeConfirmRecoveryEmail: {
      VivaldiTableViewTextEditCell* editCell =
          base::apple::ObjCCast<VivaldiTableViewTextEditCell>(cell);
      editCell.textField.delegate = self;
      break;
    }
    case ItemTypeTitle: {
      VivaldiTableViewIllustratedCell* titleCell =
          base::apple::ObjCCast<VivaldiTableViewIllustratedCell>(cell);
      titleCell.subtitleLabel.delegate = self;
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
  [self.delegate logInLinkPressed];

  // Returns NO as the app is handling the opening of the URL.
  return NO;
}

#pragma mark - UITextFieldDelegate

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [self nextButtonPressed:nil];
  return NO;
}

#pragma mark - For UITextField

- (void)usernameChanged:(UITextField*)textField {
  usernameIsValid = NO;
}

- (void)recoveryEmailChanged:(UITextField*)textField {
  emailIsValid = NO;
}

#pragma mark - Private Methods

- (void)validateFieldWithServer:(std::string)field
                          value:(std::string)value
              completionHandler:(ServerRequestCompletionHandler)handler {
  base::Value::Dict dict;
  dict.Set(vParamField, field);
  dict.Set(vParamValue, value);

  NSURL* url = [NSURL URLWithString:vVivaldiSyncValidateFieldUrl];
  sendRequestToServer(std::move(dict), url, handler, self.task);
}

- (void)onEmailValidationResponse:(NSData*)data
                         response:(NSURLResponse*)response
                            error:(NSError*)error {
  std::optional<base::Value> readResult = NSDataToDict(data);
  if (error || !readResult.has_value()) {
    emailIsValid = NO;
    [self showErrorCellWithMessage:
        l10n_util::GetNSString(IDS_SYNC_SERVER_OTHER_ERROR)];
    return;
  }

  [self removeErrorCell:SectionIdentifierUserDetails itemType:ItemTypeError];

  base::Value val = std::move(readResult).value();
  const base::Value::Dict& dict = val.GetDict();
  if (dict.FindString(vSuccessKey)) {
    emailIsValid = YES;
    [self runUserInfoValidation];
  } else {
    NSString* errorMessage;
    emailIsValid = NO;
    NSString* e = base::SysUTF8ToNSString(*(dict.FindString(vErrorKey)));
    if ([e containsString:vErrorCode1004]) {
      errorMessage =
        l10n_util::GetNSString(IDS_SYNC_EMAIL_ALREADY_REGISTERED);
    } else if ([e containsString:vErrorCode2002]) {
      errorMessage =
          l10n_util::GetNSString(IDS_EMAIL_NOT_ACCEPTED);
    } else {
      errorMessage =
          l10n_util::GetNSString(IDS_SYNC_NOT_VALID_EMAIL_ERROR);
    }

    if(errorMessage) {
      [self showErrorCellWithMessage:errorMessage];
    }
  }
}

- (void)onUserNameValidationResponse:(NSData*)data
                            response:(NSURLResponse*)response
                               error:(NSError*)error {
  std::optional<base::Value> readResult = NSDataToDict(data);
  if (error || !readResult.has_value()) {
    usernameIsValid = NO;
    [self showErrorCellWithMessage:
        l10n_util::GetNSString(IDS_SYNC_SERVER_OTHER_ERROR)];
    return;
  }

  [self removeErrorCell:SectionIdentifierUserDetails itemType:ItemTypeError];

  base::Value val = std::move(readResult).value();
  const base::Value::Dict& dict = val.GetDict();
  if (dict.FindString(vSuccessKey)) {
    usernameIsValid = YES;
    [self runUserInfoValidation];
  } else {
    usernameIsValid = NO;
    [self showErrorCellWithMessage:
        l10n_util::GetNSString(IDS_SYNC_USER_EXISTS)];
  }
}

// Returns YES if the username is valid
- (BOOL)validUsername:(NSString*)candidate {
  NSString* regex = @"[a-zA-Z0-9]{3,30}";
  return [self checkRegex:regex candidate:candidate];
}

// Returns YES if the email is valid
- (BOOL)validEmail:(NSString*)candidate {
  NSString* regex = @".+@[^\\.]+?\\.\\w[\\w\\.]+?";
  return [self checkRegex:regex candidate:candidate];
}

// Returns YES if candidate matches regex
- (BOOL)checkRegex:(NSString*)regex candidate:(NSString*)candidate {
  NSPredicate* test =
      [NSPredicate predicateWithFormat:@"SELF MATCHES %@", regex];
  return [test evaluateWithObject:candidate];
}

// Returns YES if the string is valid number
- (BOOL)validAge:(NSString*)candidate {
  if ([candidate length] == 0) {
    return NO;
  }
  NSCharacterSet* numeric_set = [NSCharacterSet decimalDigitCharacterSet];
  if (![numeric_set
          isSupersetOfSet:[NSCharacterSet characterSetWithCharactersInString:
                                              candidate]]) {
    return NO;
  }
  return YES;
}

- (void)nextButtonPressed:(UIButton*)sender {
  [self runUserInfoValidation];
}

- (void)runUserInfoValidation {
  NSString* errorMessage;

  int age = 0;
  if ([self validAge:self.ageItem.textFieldValue]) {
    base::StringToInt(
      base::SysNSStringToUTF8(self.ageItem.textFieldValue), &age);
  }
  if (![self validUsername:self.usernameItem.textFieldValue]) {
    errorMessage = l10n_util::GetNSString(IDS_SYNC_USER_NAME_LENGTH_ERROR);
  } else if (!usernameIsValid) {
    __weak __typeof__(self) weakSelf = self;
    [self setNextButtonActivityEnabled:YES];
    [self validateFieldWithServer:vParamUsername
                value:base::SysNSStringToUTF8(self.usernameItem.textFieldValue)
                completionHandler:^(NSData* data, NSURLResponse* response,
                                      NSError* error) {
      dispatch_async(dispatch_get_main_queue(), ^{
        [weakSelf setNextButtonActivityEnabled:NO];
        [weakSelf onUserNameValidationResponse:data
                                      response:response
                                        error:error];
      });
    }];
    return;
  } else if ([self.ageItem.textFieldValue length] == 0) {
    errorMessage = l10n_util::GetNSString(IDS_SYNC_USER_AGE_EMPTY_ERROR);
  } else if (age == 0 || age > vUserMaximumValidAge) {
    errorMessage = l10n_util::GetNSString(IDS_SYNC_USER_INVALID_AGE);
  } else if (age < vUserMinimumValidAge) {
    errorMessage = l10n_util::GetNSString(IDS_SYNC_USER_UNDER_AGE);
  } else if (![self validEmail:self.recoveryEmailItem.textFieldValue]) {
    errorMessage = l10n_util::GetNSString(IDS_SYNC_NOT_VALID_EMAIL_ERROR);
  } else if (!emailIsValid) {
    __weak __typeof__(self) weakSelf = self;
    [self setNextButtonActivityEnabled:YES];
    [self validateFieldWithServer:vParamEmailFieldName
            value:base::SysNSStringToUTF8(self.recoveryEmailItem.textFieldValue)
            completionHandler:^(NSData* data, NSURLResponse* response,
                                      NSError* error) {
      dispatch_async(dispatch_get_main_queue(), ^{
        [weakSelf setNextButtonActivityEnabled:NO];
        [weakSelf onEmailValidationResponse:data
                                      response:response
                                        error:error];
      });
    }];
    return;
  } else if (![self.recoveryEmailItem.textFieldValue
      isEqualToString:self.confirmRecoveryEmailItem.textFieldValue]) {
    errorMessage = l10n_util::GetNSString(IDS_SYNC_EMAIL_MATCH_ERROR);
  }

  [self removeErrorCell:SectionIdentifierUserDetails itemType:ItemTypeError];
  if (errorMessage) {
    [self showErrorCellWithMessage:errorMessage];
    return;
  }

  [self.delegate nextButtonPressed:self.usernameItem.textFieldValue
                               age:age
              recoveryEmailAddress:self.recoveryEmailItem.textFieldValue];
}

- (void)showErrorCellWithMessage:(NSString*)message {
  [self showErrorCellWithMessage:message
                         section:SectionIdentifierUserDetails
                        itemType:ItemTypeError];
}

- (void)setNextButtonActivityEnabled:(BOOL)enable {
  self.nextButton.activityInProgress = enable;
  [self reloadSection:SectionIdentifierNextButton];
}

@end
