// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_address_mediator.h"

#import "base/i18n/message_formatter.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_profile.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/browser/profile_requirement_utils.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/address_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/address_list_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_action_cell.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_address+AutofillProfile.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_address.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_address_cell.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_content_injector.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

using autofill::AutofillProfile;

namespace manual_fill {
NSString* const ManageAddressAccessibilityIdentifier =
    @"kManualFillManageAddressAccessibilityIdentifier";
}  // namespace manual_fill

@interface ManualFillAddressMediator () <PersonalDataManagerObserver>

// All available addresses.
@property(nonatomic, assign) std::vector<const AutofillProfile*> addresses;

@end

@implementation ManualFillAddressMediator {
  // Personal data manager to be observed.
  raw_ptr<autofill::PersonalDataManager> _personalDataManager;

  // C++ to ObjC bridge for PersonalDataManagerObserver.
  std::unique_ptr<autofill::PersonalDataManagerObserverBridge>
      _personalDataManagerObserver;

  // Indicates whether to show the autofill button for the items.
  BOOL _showAutofillFormButton;

  // Service used to get the user's identity email address.
  raw_ptr<AuthenticationService> _authenticationService;
}

- (instancetype)initWithPersonalDataManager:
                    (autofill::PersonalDataManager*)personalDataManager
                     showAutofillFormButton:(BOOL)showAutofillFormButton
                      authenticationService:
                          (AuthenticationService*)authenticationService {
  self = [super init];
  if (self) {
    _personalDataManager = personalDataManager;
    _personalDataManagerObserver =
        std::make_unique<autofill::PersonalDataManagerObserverBridge>(self);
    _personalDataManager->AddObserver(_personalDataManagerObserver.get());
    _authenticationService = authenticationService;
    _addresses =
        _personalDataManager->address_data_manager().GetProfilesToSuggest();
    _showAutofillFormButton = showAutofillFormButton;
  }
  return self;
}

- (void)setConsumer:(id<ManualFillAddressConsumer>)consumer {
  if (consumer == _consumer) {
    return;
  }
  _consumer = consumer;
  [self postAddressesToConsumer];
  [self postActionsToConsumer];
}

- (void)disconnect {
  if (_personalDataManager && _personalDataManagerObserver.get()) {
    _personalDataManager->RemoveObserver(_personalDataManagerObserver.get());
    _personalDataManagerObserver.reset();
  }
  _personalDataManager = nullptr;
  _authenticationService = nullptr;
}

#pragma mark - PersonalDataManagerObserver

- (void)onPersonalDataChanged {
  self.addresses =
      _personalDataManager->address_data_manager().GetProfilesToSuggest();
  if (self.consumer) {
    [self postAddressesToConsumer];
    [self postActionsToConsumer];
  }
}

#pragma mark - Private

// Posts the addresses to the consumer.
- (void)postAddressesToConsumer {
  if (!self.consumer) {
    return;
  }

  int addressCount = self.addresses.size();
  NSMutableArray* items =
      [[NSMutableArray alloc] initWithCapacity:addressCount];
  for (int i = 0; i < addressCount; i++) {
    ManualFillAddress* manualFillAddress =
        [[ManualFillAddress alloc] initWithProfile:*self.addresses[i]];

    NSArray<UIAction*>* menuActions =
        IsKeyboardAccessoryUpgradeEnabled()
            ? @[ [self createMenuEditActionForAddress:self.addresses[i]] ]
            : @[];

    NSString* cellIndexAccessibilityLabel = base::SysUTF16ToNSString(
        base::i18n::MessageFormatter::FormatWithNamedArgs(
            l10n_util::GetStringUTF16(
                IDS_IOS_MANUAL_FALLBACK_ADDRESS_CELL_INDEX),
            "count", addressCount, "position", i + 1));

    auto item = [[ManualFillAddressItem alloc]
                    initWithAddress:manualFillAddress
                    contentInjector:self.contentInjector
                        menuActions:menuActions
        cellIndexAccessibilityLabel:cellIndexAccessibilityLabel
             showAutofillFormButton:_showAutofillFormButton];
    [items addObject:item];
  }

  [self.consumer presentAddresses:items];
}

- (void)postActionsToConsumer {
  if (!self.consumer) {
    return;
  }

  NSString* manageAddressesTitle =
      l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_MANAGE_ADDRESSES);
  __weak __typeof(self) weakSelf = self;
  ManualFillActionItem* manageAddressesItem = [[ManualFillActionItem alloc]
      initWithTitle:manageAddressesTitle
             action:^{
               base::RecordAction(base::UserMetricsAction(
                   "ManualFallback_Profiles_OpenManageProfiles"));
               [weakSelf.navigationDelegate openAddressSettings];
             }];
  manageAddressesItem.accessibilityIdentifier =
      manual_fill::ManageAddressAccessibilityIdentifier;
  [self.consumer presentActions:@[ manageAddressesItem ]];
}

// Creates a UIAction to edit an address from a UIMenu.
- (UIAction*)createMenuEditActionForAddress:(const AutofillProfile*)address {
  ActionFactory* actionFactory = [[ActionFactory alloc]
      initWithScenario:
          kMenuScenarioHistogramAutofillManualFallbackAddressEntry];

  __weak __typeof(self) weakSelf = self;
  UIAction* editAction = [actionFactory actionToEditWithBlock:^{
    BOOL offerMigrateToAccount =
        [weakSelf offerMigrateToAccountForAddress:address];
    [weakSelf.navigationDelegate
        openAddressDetailsInEditMode:address
               offerMigrateToAccount:offerMigrateToAccount];
  }];

  return editAction;
}

// Evaluates whether or not the option to move the address to the account should
// be available when navigating to the details page of the given address.
- (BOOL)offerMigrateToAccountForAddress:(const AutofillProfile*)address {
  BOOL syncIsEnabled = _personalDataManager->address_data_manager()
                           .IsSyncFeatureEnabledForAutofill();
  BOOL addressIsLocalOrSyncable =
      address->source() == autofill::AutofillProfile::Source::kLocalOrSyncable;
  BOOL addressIsEligibleForAccountMigration =
      addressIsLocalOrSyncable &&
      IsEligibleForMigrationToAccount(
          _personalDataManager->address_data_manager(), *address);
  BOOL userEmailIsValid = [self userEmail] != nil;

  return !syncIsEnabled && addressIsEligibleForAccountMigration &&
         userEmailIsValid;
}

// Returns the user's identity email address.
- (NSString*)userEmail {
  id<SystemIdentity> identity =
      _authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (identity) {
    return identity.userEmail;
  } else {
    return nil;
  }
}

@end
