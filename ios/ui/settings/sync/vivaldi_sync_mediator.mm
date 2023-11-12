// Copyright 2022-2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/sync/vivaldi_sync_mediator.h"

#import "base/base64.h"
#import "base/containers/flat_map.h"
#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/language/core/browser/pref_names.h"
#import "components/os_crypt/os_crypt.h"
#import "components/prefs/pref_service.h"
#import "components/sync/base/command_line_switches.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/driver/sync_service_observer.h"
#import "components/sync/driver/sync_service.h"
#import "components/sync/driver/sync_user_settings.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/settings/sync/cells/vivaldi_table_view_sync_status_item.h"
#import "ios/ui/settings/sync/cells/vivaldi_table_view_sync_user_info_item.h"
#import "ios/ui/settings/sync/vivaldi_create_account_ui_helper.h"
#import "ios/ui/settings/sync/vivaldi_sync_settings_constants.h"
#import "ios/ui/settings/sync/vivaldi_sync_settings_view_controller.h"
#import "ios/ui/table_view/cells/vivaldi_table_view_segmented_control_item.h"
#import "ios/ui/table_view/cells/vivaldi_table_view_text_button_item.h"
#import "prefs/vivaldi_pref_names.h"
#import "sync/vivaldi_sync_service_impl.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi_account/vivaldi_account_manager.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"
#import "vivaldi/prefs/vivaldi_gen_prefs.h"

using syncer::ClientAction;
using syncer::SyncProtocolErrorType;
using syncer::SyncService;
using syncer::UserSelectableType;
using syncer::UserSelectableTypeSet;
using vivaldi::EngineData;
using vivaldi::EngineState;
using vivaldi::VivaldiAccountManager;
using vivaldi::VivaldiSyncServiceImpl;
using vivaldi::VivaldiSyncUIHelper;
using base::SysUTF8ToNSString;
using base::SysNSStringToUTF8;

namespace {
  const std::string ERROR_ACCOUNT_NOT_ACTIVATED = "17006";
}

struct PendingRegistration {
std::string username;
int age;
std::string recoveryEmailAddress;
std::string password;
};

@protocol AccountManagerObserver
// Called by VivaldiAccountManagerObserverBridge
-(void)onVivaldiAccountUpdated;
-(void)onTokenFetchSucceeded;
-(void)onTokenFetchFailed;

@end

@protocol VivaldiSyncServiceObserver
// Called by VivaldiSyncServiceObserverBridge
-(void)onSyncStateChanged;
-(void)onSyncCycleCompleted;
@end

@interface VivaldiSyncMediator () <VivaldiSyncServiceObserver,
                                   AccountManagerObserver> {
PendingRegistration pendingRegistration;
}

@property(nonatomic, assign) VivaldiSyncServiceImpl* syncService;
@property(nonatomic, assign) VivaldiAccountManager* vivaldiAccountManager;
@property(nonatomic, assign) PrefService* prefService;

// Model
@property(nonatomic, strong) NSArray* segmentedControlLabels;
@property(nonatomic, strong) NSArray* syncAllItems;
@property(nonatomic, strong) NSArray* syncSelectedItems;
@property(nonatomic, strong)
    VivaldiTableViewSegmentedControlItem* segmentedControlItem;
@property(nonatomic, strong) VivaldiTableViewTextButtonItem* startSyncingButton;
@property(nonatomic, strong) TableViewTextButtonItem* deleteDataButton;
@property(nonatomic, strong) TableViewTextButtonItem* logOutButton;
@property(nonatomic, strong) VivaldiTableViewSyncUserInfoItem* userInfoItem;
@property(nonatomic, strong) VivaldiTableViewSyncStatusItem* syncStatusItem;
@property(nonatomic, strong) NSDateFormatter* formatter;

@property(nonatomic, copy) NSURLSessionDataTask* task;

@end

class VivaldiAccountManagerObserverBridge :
    public VivaldiAccountManager::Observer {
  public:
    VivaldiAccountManagerObserverBridge(
        VivaldiSyncMediator* consumer,
        VivaldiAccountManager* account_manager
    ) : account_manager_(account_manager),
        consumer_(consumer) {
      DCHECK(account_manager);
      DCHECK(consumer);
      account_manager_->AddObserver(this);
    }

    ~VivaldiAccountManagerObserverBridge() override {
      account_manager_->RemoveObserver(this);
    }

    // VivaldiAccountManager::Observer implementation
    void OnVivaldiAccountUpdated() override {
      [consumer_ onVivaldiAccountUpdated];
    }

    void OnTokenFetchSucceeded() override {
      [consumer_ onTokenFetchSucceeded];
    }

    void OnTokenFetchFailed() override {
      [consumer_ onTokenFetchFailed];
    }

    void OnVivaldiAccountShutdown() override {
      account_manager_->RemoveObserver(this);
    }

 private:
    VivaldiAccountManager* account_manager_;
    __weak VivaldiSyncMediator* const consumer_; // Weak, owns us
};


class VivaldiSyncServiceObserverBridge : public syncer::SyncServiceObserver {
  public:
    VivaldiSyncServiceObserverBridge(
        VivaldiSyncMediator* consumer,
        VivaldiSyncServiceImpl* sync_service
    ) : sync_service_(sync_service),
        consumer_(consumer) {
      DCHECK(sync_service);
      DCHECK(consumer);
      sync_service_->AddObserver(this);
    }

    ~VivaldiSyncServiceObserverBridge() override {
      sync_service_->RemoveObserver(this);
    }

    // syncer::SyncServiceObserver implementation.
    void OnStateChanged(SyncService* sync) override {
      [consumer_ onSyncStateChanged];
    }
    void OnSyncCycleCompleted(SyncService* sync) override {
      [consumer_ onSyncCycleCompleted];
    }

    void OnSyncShutdown(SyncService* sync) override {
      sync_service_->RemoveObserver(this);
    }

 private:
    VivaldiSyncServiceImpl* sync_service_;
    __weak VivaldiSyncMediator* const consumer_; // Weak, owns us
};

@interface VivaldiSyncMediator () {
  EngineData engineData;
  VivaldiSyncUIHelper::CycleData cycleData;
  bool loggingOut;
  base::flat_map<int, int> background_color_map;
}

@end

@implementation VivaldiSyncMediator

- (instancetype)initWithAccountManager:
                  (VivaldiAccountManager*)vivaldiAccountManager
                syncService:(VivaldiSyncServiceImpl*)syncService
                prefService:(PrefService*)prefService {

  self = [super init];
  if (self) {
    _vivaldiAccountManager = vivaldiAccountManager;
    _syncService = syncService;
    _prefService = prefService;
    loggingOut = false;

    _accountManagerObserver =
      std::make_unique<VivaldiAccountManagerObserverBridge>(
        self, _vivaldiAccountManager);
    _syncObserver = std::make_unique<VivaldiSyncServiceObserverBridge>(
        self, _syncService);

    self.formatter = [[NSDateFormatter alloc] init];
    [self.formatter setDateFormat:@"dd/MM/yyyy HH:mm:ss"];

    background_color_map = {
        {kSyncStatusGreenColor, kSyncStatusGreenBackgroundColor},
        {kSyncStatusBlueColor, kSyncStatusBlueBackgroundColor},
        {kSyncStatusRedColor, kSyncStatusRedBackgroundColor},
        {kSyncStatusYellowColor, kSyncStatusYellowBackgroundColor}};

    // The order must match the SyncType enum
    self.segmentedControlLabels = [NSArray arrayWithObjects:
        l10n_util::GetNSString(IDS_VIVALDI_SYNC_AUTOMATIC_TITLE),
        l10n_util::GetNSString(IDS_VIVALDI_SYNC_DATA_CHOOSE_TITLE),
        nil];
  }
  return self;
}

- (void)dealloc {
  DCHECK(!self.commandHandler);
  DCHECK(!self.settingsConsumer);
  _accountManagerObserver.reset();
  _syncObserver.reset();
}

- (void)startMediating {
  DCHECK(self.commandHandler);
  // TODO(tomas@vivaldi.com): Implement missing account states
  switch ([self getSimplifiedAccountState]) {
    case LOGGED_IN: {
      [self onSyncStateChanged];
      break;
    }
    case LOGGED_OUT: {
      [self.commandHandler showSyncLoginView];
      break;
    }
    case NOT_ACTIVATED: {
      [self handleNotActivated];
      break;
    }
    case CREDENTIALS_MISSING: {
      NSString* errorMessage;
      if (_vivaldiAccountManager->has_encrypted_refresh_token()) {
        errorMessage = l10n_util::GetNSString(
          IDS_VIVALDI_ACCOUNT_ERROR_CREDENTIALS_ENCRYPTED);
      } else {
        errorMessage = l10n_util::GetNSString(
          IDS_VIVALDI_ACCOUNT_ERROR_CREDENTIALS_MISSING);
      }
      [self.commandHandler loginFailed:errorMessage];
      break;
    }
    case LOGIN_FAILED: {
      NSString* errorMessage;
      std::string serverMessage =
          _vivaldiAccountManager->last_token_fetch_error().server_message;
      int errorCode =
          _vivaldiAccountManager->last_token_fetch_error().error_code;

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
      if (!serverMessage.empty()) {
        NSAttributedString* errorMsg = [[NSAttributedString alloc]
          initWithString:l10n_util::GetNSStringF(
                IDS_VIVALDI_ACCOUNT_ERROR_CREDENTIALS_REJECTED_1,
              base::UTF8ToUTF16(serverMessage),
              base::SysNSStringToUTF16([@(errorCode) stringValue]))
          attributes:textAttributes];
        errorMessage = [errorMsg string];
      } else if (errorCode == -1) {
        errorMessage = l10n_util::GetNSString(
            IDS_VIVALDI_ACCOUNT_ERROR_WRONG_DOMAIN);
      } else {
        NSAttributedString* errorMsg = [[NSAttributedString alloc]
          initWithString:l10n_util::GetNSStringF(
                IDS_VIVALDI_ACCOUNT_ERROR_CREDENTIALS_REJECTED_2,
              base::SysNSStringToUTF16([@(errorCode) stringValue]))
          attributes:textAttributes];
        errorMessage = [errorMsg string];
      }
      [self.commandHandler loginFailed:errorMessage];
      break;
    }
    case LOGGING_IN:
      break;
    default:
      NOTREACHED();
      break;
  }
}

- (void)disconnect {
  _accountManagerObserver.reset();
  _syncObserver.reset();
  self.commandHandler = nil;
  self.settingsConsumer = nil;
}


- (void)requestPendingRegistrationLogin {
  [self login:pendingRegistration.username
      password:pendingRegistration.password
      save_password:false];
}

- (NSString*)getPendingRegistrationUsername {
  return SysUTF8ToNSString(pendingRegistration.username);
}

- (NSString*)getPendingRegistrationEmail{
  return SysUTF8ToNSString(pendingRegistration.recoveryEmailAddress);
}

- (void)clearPendingRegistration {
  _prefService->ClearPref(vivaldiprefs::kVivaldiAccountPendingRegistration);
  pendingRegistration.username = "";
  pendingRegistration.age = 0;
  pendingRegistration.recoveryEmailAddress = "";
  pendingRegistration.password = "";
}

- (void)login:(std::string)username
        password:(std::string)password
        save_password:(BOOL)save_password {
  // If the user has started registration on another device, and tries to log in
  // here, we store the username and password in case we get a NOT_ACTIVE
  // reply from the server. There will not be a pending registration in prefs
  pendingRegistration.username = username;
  pendingRegistration.password = password;
  pendingRegistration.recoveryEmailAddress = SysNSStringToUTF8(
      l10n_util::GetNSString(IDS_SYNC_DEFAULT_RECOVERY_EMAIL_ADDRESS));
  _vivaldiAccountManager->Login(username, password, save_password);
  _syncService->GetUserSettings()->SetSyncRequested(true);
}

- (void)setEncryptionPassword:(std::string)password {
  bool success = _syncService->ui_helper()->SetEncryptionPassword(password);
  if (!success) {
    // TODO(tomas@vivaldi.com) implement the failure path
  }
}

- (void)storeUsername:(NSString*)username
                  age:(int)age
                email:(NSString*)recoveryEmailAddress {
  pendingRegistration.username = SysNSStringToUTF8(username);
  pendingRegistration.age = age;
  pendingRegistration.recoveryEmailAddress =
      SysNSStringToUTF8(recoveryEmailAddress);
}

- (void)createAccount:(NSString*)password
           deviceName:(NSString*)deviceName
      wantsNewsletter:(BOOL)wantsNewsletter {
  pendingRegistration.password = [password UTF8String];
  __weak __typeof__(self) weakSelf = self;
  [self sendCreateAccountRequestToServer:wantsNewsletter
    completionHandler:^(NSData* data,NSURLResponse* response, NSError* error) {
    dispatch_async(dispatch_get_main_queue(), ^{
      [weakSelf onCreateAccountResponse:data
                                response:response
                                  error:error
                             deviceName:deviceName];
    });
  }];
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  if (loggingOut) {
    return;
  }

  engineData = _syncService->ui_helper()->GetEngineData();
  cycleData = _syncService->ui_helper()->GetCycleData();

  if ([self getSimplifiedAccountState] == NOT_ACTIVATED ||
      (_vivaldiAccountManager->last_token_fetch_error().server_message.empty()
      && [self hasPendingRegistration])) {
    if ([self getSimplifiedAccountState] == NOT_ACTIVATED &&
        ![self hasPendingRegistration] &&
        !pendingRegistration.username.empty()) {
      // The user may have started registartion on another device
      [self setPendingRegistration];
    }
    [self handleNotActivated];
    return;
  }

  switch(engineData.engine_state) {
    case EngineState::STOPPED:
      if ([self getSimplifiedAccountState] != LOGGED_IN) {
        [self.commandHandler showSyncLoginView];
        return;
      }
      // TODO(tomas@vivaldi) if we get STOPPED while logged in we should show a
      // "Restart sync" button somewhere. It should call setsyncrequested(true).
      ABSL_FALLTHROUGH_INTENDED;
    case EngineState::FAILED:
      // TODO(tomas@vivaldi) Depending on the reason for the Failed state,
      // the response should be either to wait, inform the user and give up
      // or offer to restart, depending on the failure cause.
    case EngineState::STARTING:
    case EngineState::STARTING_SERVER_ERROR:
    case EngineState::CLEARING_DATA:
      [self reloadSyncStatusItem:engineData];
      break;
    case EngineState::CONFIGURATION_PENDING:
    case EngineState::STARTED:
      if (engineData.needs_decryption_password) {
        [self.commandHandler showSyncEncryptionPasswordView:NO];
      } else if (!engineData.uses_encryption_password) {
        // There is a different message displayed when creating the
        // encryption key for the first time.
        // We land here when the server has no encryption password set
        [self.commandHandler showSyncEncryptionPasswordView:YES];
      } else {
        [self.commandHandler showSyncSettingsView];
        [self reloadSyncStatusItem:engineData];
      }
      break;
    default:
      NOTREACHED();
      break;
  }
}

-(void)onSyncCycleCompleted {
  [self onSyncStateChanged];
}

#pragma mark - AccountManagerObserver

-(void)onVivaldiAccountUpdated {
  if (!loggingOut) {
    [self onSyncStateChanged];
  }
  if (loggingOut && [self getSimplifiedAccountState] == LOGGED_OUT) {
    loggingOut = false;
  }
}

-(void)onTokenFetchSucceeded {
  [self onSyncStateChanged];
}

-(void)onTokenFetchFailed {
  if ([self getSimplifiedAccountState] == LOGIN_FAILED) {
    [self.commandHandler loginFailed:l10n_util::GetNSString(
        IDS_VIVALDI_ACCOUNT_LOG_IN_FAILED)];
    return;
  }
  [self onSyncStateChanged];
}

#pragma mark - VivaldiSyncSettingsViewControllerServiceDelegate

- (void)backupEncryptionKeyButtonPressed {
  // TODO(tomas@vivaldi.com): Implement this differently. Let the user
  // pick where to save the file and show a warning, similar to
  // export passwords.

  NSString* key = SysUTF8ToNSString(
    _syncService->ui_helper()->GetBackupEncryptionToken());

  [key writeToFile:GetBackupEncryptionKeyPath()
        atomically:YES
          encoding:NSUTF8StringEncoding
             error:nil];
}

- (void)clearSyncDataWithNoWarning {
  _syncService->ClearSyncData();

  // TODO(tomas@vivaldi.com): The warning shown says that doing this action will
  // log you out, but neither android nor desktop actually does that. Should
  // it not log out?
  [self logOutButtonPressed];
}

- (void)logOutButtonPressed {
  loggingOut = true;
  _syncService->GetUserSettings()->SetSyncRequested(false);
  _syncService->StopAndClear();
  _vivaldiAccountManager->Logout();
  [self clearPendingRegistration];
  [self.commandHandler showSyncLoginView];
}

- (void)startSyncingAllButtonPressed {
  UserSelectableTypeSet selectedTypes;
  selectedTypes.Put(UserSelectableType::kBookmarks);
  selectedTypes.Put(UserSelectableType::kPasswords);
  selectedTypes.Put(UserSelectableType::kPreferences);
  selectedTypes.Put(UserSelectableType::kAutofill);
  selectedTypes.Put(UserSelectableType::kHistory);
  selectedTypes.Put(UserSelectableType::kTabs);

  [self setChosenTypes:selectedTypes syncAll:YES];
}

- (void)syncAllOptionChanged:(BOOL)syncAll {
  if (!self.settingsConsumer) {
    return;
  }
  if (syncAll) {
    [self.settingsConsumer.tableView performBatchUpdates:^{
      [self removeItemsFromUI:self.syncSelectedItems];
      self.segmentedControlItem.selectedItem = SyncAll;
      [self addItemsToUI:self.syncAllItems
          toSection:SectionIdentifierSyncItems];
      [self updateStartSyncingSection];
      [self.settingsConsumer reloadSection:SectionIdentifierSyncItems];
      [self.settingsConsumer reloadSection:SectionIdentifierSyncStartSyncing];
    }
    completion:nil];
  } else {
    [self.settingsConsumer.tableView performBatchUpdates:^{
      [self removeItemsFromUI:self.syncAllItems];
      self.segmentedControlItem.selectedItem = SyncSelected;
      [self addItemsToUI:self.syncSelectedItems
          toSection:SectionIdentifierSyncItems];
      [self updateStartSyncingSection];
      [self.settingsConsumer reloadSection:SectionIdentifierSyncItems];
      [self.settingsConsumer reloadSection:SectionIdentifierSyncStartSyncing];
    }
    completion:nil];
  }
}

- (void)updateChosenTypes:(UserSelectableType)type isOn:(BOOL)isOn {
  UserSelectableTypeSet selectedTypes = engineData.data_types;
  if (isOn && !engineData.data_types.Has(type)) {
    selectedTypes.Put(type);
  } else if (!isOn && engineData.data_types.Has(type)) {
    selectedTypes.Remove(type);
  }

  if (selectedTypes.Has(UserSelectableType::kHistory)) {
    selectedTypes.Put(UserSelectableType::kTabs);
  } else if (!selectedTypes.Has(UserSelectableType::kHistory) &&
              selectedTypes.Has(UserSelectableType::kTabs)) {
    selectedTypes.Remove(UserSelectableType::kTabs);
  }

  [self setChosenTypes:selectedTypes syncAll:NO];
}

#pragma mark - VivaldiSyncSettingsViewControllerModelDelegate

- (void)vivaldiSyncSettingsViewControllerLoadModel:
    (id<VivaldiSyncSettingsConsumer>)controller {
  if (!self.settingsConsumer) {
    return;
  }
  DCHECK_EQ(self.settingsConsumer, controller);
  [self addUserInfoSection];
  [self addSyncStatusSection];
  [self addSyncSwitchesSection];
  [self addStartSyncingSection];
  [self addEncryptionSection];
  [self addSignOutSection];
  [self addDeleteDataSection];
}

#pragma mark - Private Methods

NSString* FormattedString(int message_id) {
  return [@"  " stringByAppendingString:l10n_util::GetNSString(message_id)];
}

- (void)updateSyncStatusItem {
  NSString* statusText;
  int color = 0;
  NSDate* lastSyncDate = nil;

  if (engineData.engine_state == EngineState::FAILED) {
    statusText = FormattedString(IDS_VIVALDI_SYNC_FAILED);
    color = kSyncStatusRedColor;
    // TODO(tomas@vivaldi.com): Some of these strings are not used on the
    // android side, only on desktop. Should they be used on iOS?
    if (engineData.disable_reasons.Has(
        SyncService::DISABLE_REASON_NOT_SIGNED_IN)) {
      // We might enter this state for a brief moment after login while
      // the credentials are being passed to sync itself.
      statusText = @"Waiting for account details";
      color = kSyncStatusYellowColor;
    } else if (!syncer::IsSyncAllowedByFlag()) {
      // Disabled by command line flag
      statusText = @"Sync was disabled from the command line";
      color = kSyncStatusYellowColor;
    } else if (engineData.disable_reasons.Has(
        SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR)) {
      switch (engineData.protocol_error_client_action) {
        case ClientAction::UPGRADE_CLIENT:
          statusText = FormattedString(IDS_VIVALDI_SYNC_CLIENT_NEEDS_UPDATE);
          break;
        default:
          statusText = @"Sync must be restarted: ";
          statusText = [statusText stringByAppendingString:
              SysUTF8ToNSString(engineData.protocol_error_description)];
      }
    }
  } else if (engineData.engine_state == EngineState::CLEARING_DATA) {
    statusText = FormattedString(IDS_VIVALDI_SYNC_CLEARING_SERVER_DATA);
    color = kSyncStatusBlueColor;
  } else if (engineData.engine_state == EngineState::CONFIGURATION_PENDING) {
    statusText = FormattedString(IDS_SYNC_ALL_SET_TEXT);
    color = kSyncStatusBlueColor;
  } else if (engineData.engine_state == EngineState::STARTED) {
    if (cycleData.download_updates_status ==
            VivaldiSyncUIHelper::CycleStatus::SUCCESS &&
        cycleData.commit_status == VivaldiSyncUIHelper::CycleStatus::SUCCESS) {
      statusText = FormattedString([self getCycleStatusMessageId:
          cycleData.commit_status]);
      color = [self getCycleStatusColor:cycleData.commit_status];
      lastSyncDate = cycleData.cycle_start_time.ToNSDate();
    } else {
      statusText = FormattedString([self getCycleStatusMessageId:
          cycleData.download_updates_status]);
      color = [self getCycleStatusColor:cycleData.download_updates_status];
    }
  } else if (engineData.engine_state == EngineState::STARTING_SERVER_ERROR) {
    statusText = FormattedString(IDS_VIVALDI_SYNC_WAITING_SERVER_ERROR);
    color = kSyncStatusYellowColor;
  } else if (engineData.engine_state == EngineState::STARTING) {
    statusText = FormattedString(IDS_VIVALDI_SYNC_INITIALIZING);
    color = kSyncStatusBlueColor;
  } else if (engineData.engine_state == EngineState::STOPPED) {
    statusText = @" ";
    color = kSyncStatusYellowColor;
    if (engineData.protocol_error_client_action ==
        ClientAction::DISABLE_SYNC_ON_CLIENT) {
      // TODO(tomas@vivaldi.com): These strings are not used on the
      // android side, only on desktop. Should they be used on iOS?
      statusText = [statusText stringByAppendingString:
          @"Sync was stopped following a request from the server: "];
      if (engineData.protocol_error_type ==
          SyncProtocolErrorType::NOT_MY_BIRTHDAY) {
        statusText = [statusText stringByAppendingString:
            @"Sync server data was reset"];
      } else {
        statusText = [statusText stringByAppendingString:
            SysUTF8ToNSString(engineData.protocol_error_description)];
      }
    } else {
      statusText = [statusText stringByAppendingString:
            SysUTF8ToNSString(engineData.protocol_error_description)];
    }
  } else {
    NOTREACHED();
  }

  if (lastSyncDate) {
    self.syncStatusItem.lastSyncDateString = [NSString stringWithFormat:@"%@ %@",
        l10n_util::GetNSString(IDS_VIVALDI_IOS_SYNC_LAST_SYNC),
        [self.formatter stringFromDate:lastSyncDate]];
  } else {
    self.syncStatusItem.lastSyncDateString = nil;
  }
  self.syncStatusItem.statusText = statusText;
  if (color > 0) {
    self.syncStatusItem.statusIndicatorColor = UIColorFromRGB(color);
    self.syncStatusItem.statusBackgroundColor =
        UIColorFromRGB(background_color_map[color]);
  } else {
    self.syncStatusItem.statusIndicatorColor = [UIColor clearColor];
    self.syncStatusItem.statusBackgroundColor = [UIColor clearColor];
  }
}

- (int)getCycleStatusMessageId:(int)status {
  switch (status) {
    case VivaldiSyncUIHelper::CycleStatus::NOT_SYNCED:
      return IDS_VIVALDI_SYNC_NOT_SYNCED_CYCLE;
    case VivaldiSyncUIHelper::CycleStatus::SUCCESS:
      return IDS_VIVALDI_SYNC_SUCCESS;
    case VivaldiSyncUIHelper::CycleStatus::IN_PROGRESS:
      return IDS_VIVALDI_SYNC_APPLYING_CHANGES;
    case VivaldiSyncUIHelper::CycleStatus::AUTH_ERROR:
      return IDS_VIVALDI_SYNC_AUTH_ERROR;
    case VivaldiSyncUIHelper::CycleStatus::SERVER_ERROR:
      return IDS_VIVALDI_SYNC_SERVER_ERROR;
    case VivaldiSyncUIHelper::CycleStatus::NETWORK_ERROR:
      return IDS_VIVALDI_SYNC_NETWORK_ERROR;
    case VivaldiSyncUIHelper::CycleStatus::CONFLICT:
      return IDS_VIVALDI_SYNC_CONFLICT;
    case VivaldiSyncUIHelper::CycleStatus::THROTTLED:
      return IDS_VIVALDI_SYNC_THROTTLED;
    default:
      return IDS_VIVALDI_SYNC_OTHER_ERROR;
  }
}

- (int)getCycleStatusColor:(int)status {
  switch (status) {
    case VivaldiSyncUIHelper::CycleStatus::SUCCESS:
      return kSyncStatusGreenColor;
    case VivaldiSyncUIHelper::CycleStatus::IN_PROGRESS:
      return kSyncStatusBlueColor;
    case VivaldiSyncUIHelper::CycleStatus::NOT_SYNCED:
    case VivaldiSyncUIHelper::CycleStatus::NETWORK_ERROR:
    case VivaldiSyncUIHelper::CycleStatus::THROTTLED:
      return kSyncStatusYellowColor;
    case VivaldiSyncUIHelper::CycleStatus::AUTH_ERROR:
    case VivaldiSyncUIHelper::CycleStatus::SERVER_ERROR:
    case VivaldiSyncUIHelper::CycleStatus::CONFLICT:
    default:
      return kSyncStatusRedColor;
  }
}

- (void) reloadSyncStatusItem:(EngineData)engineData {
  if (!self.settingsConsumer) {
    return;
  }

  [self updateSyncStatusItem];
  [self.settingsConsumer reloadItem:self.syncStatusItem];

  if ([self.settingsConsumer.tableViewModel
      hasSectionForSectionIdentifier:SectionIdentifierSyncStartSyncing]) {
    self.startSyncingButton.enabled = [self getStartSyncingButtonEnabled];
    [self.settingsConsumer reloadSection:SectionIdentifierSyncStartSyncing];
  }
}

- (BOOL)getStartSyncingButtonEnabled {
    BOOL isPending =
      engineData.engine_state == EngineState::CONFIGURATION_PENDING ? YES : NO;
    return isPending || !engineData.sync_everything;
}

- (void) addUserInfoSection {
  TableViewModel* model = self.settingsConsumer.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierSyncUserInfo];
  VivaldiAccountManager::AccountInfo account_info =
      _vivaldiAccountManager->account_info();
  self.userInfoItem = [[VivaldiTableViewSyncUserInfoItem alloc]
      initWithType:ItemTypeSyncUserInfo];
  self.userInfoItem.userName = SysUTF8ToNSString(account_info.username);
  [self.settingsConsumer.tableViewModel setHeader:self.userInfoItem
      forSectionWithIdentifier:SectionIdentifierSyncUserInfo];

  __weak VivaldiSyncMediator* weakSelf = self;
  NSURL* profileImageURL =
      [NSURL URLWithString:SysUTF8ToNSString(account_info.picture_url)];
  dispatch_async(dispatch_get_main_queue(), ^{
    NSData * imageData = [[NSData alloc] initWithContentsOfURL:profileImageURL];
    weakSelf.userInfoItem.userAvatar = [UIImage imageWithData:imageData];
    [weakSelf.settingsConsumer reloadSection:SectionIdentifierSyncUserInfo];
  });
}

- (void) addSyncStatusSection {
  TableViewModel* model = self.settingsConsumer.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierSyncStatus];
  self.syncStatusItem =
      [[VivaldiTableViewSyncStatusItem alloc]
          initWithType:ItemTypeSyncStatus];

  [self updateSyncStatusItem];
  [model addItem:self.syncStatusItem
       toSectionWithIdentifier:SectionIdentifierSyncStatus];
}

- (void) addSyncSwitchesSection {
  TableViewModel* model = self.settingsConsumer.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierSyncItems];
  self.segmentedControlItem = [[VivaldiTableViewSegmentedControlItem alloc]
        initWithType:ItemTypeSyncWhatSegmentedControl];

  bool syncAllData = _syncService->GetUserSettings()->IsSyncEverythingEnabled();
  self.segmentedControlItem.labels = self.segmentedControlLabels;
  [model addItem:self.segmentedControlItem
      toSectionWithIdentifier:SectionIdentifierSyncItems];
  [self initSyncItemArrays];
  [self syncAllOptionChanged:syncAllData];
}

- (void) addStartSyncingSection {
  TableViewModel* model = self.settingsConsumer.tableViewModel;
  self.startSyncingButton = [[VivaldiTableViewTextButtonItem alloc]
      initWithType:ItemTypeStartSyncingButton];
  self.startSyncingButton.buttonText =
      l10n_util::GetNSString(IDS_VIVALDI_START_SYNCING);
  self.startSyncingButton.textAlignment = NSTextAlignmentNatural;
  self.startSyncingButton.buttonTextColor = [UIColor colorNamed:kBlueColor];
  self.startSyncingButton.buttonBackgroundColor = [UIColor clearColor];
  self.startSyncingButton.disableButtonIntrinsicWidth = YES;
  self.startSyncingButton.enabled = [self getStartSyncingButtonEnabled];

  [model addSectionWithIdentifier:SectionIdentifierSyncStartSyncing];
  [self updateStartSyncingSection];
}

- (void) addEncryptionSection {
  TableViewModel* model = self.settingsConsumer.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierSyncEncryption];
  TableViewLinkHeaderFooterItem* encryptionHeaderItem =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeHeaderItem];
  encryptionHeaderItem.text =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_ENCRYPTION_PASSWORD_TITLE);
  [model setHeader:encryptionHeaderItem
        forSectionWithIdentifier:SectionIdentifierSyncEncryption];

  TableViewDetailTextItem* encryptionPasswordItem =
      [[TableViewDetailTextItem alloc]
          initWithType:ItemTypeEncryptionPasswordButton];
  encryptionPasswordItem.text =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_ENCRYPTION_PASSWORD_SECTION);
  encryptionPasswordItem.accessorySymbol =
      TableViewDetailTextCellAccessorySymbolExternalLink;
  [model addItem:encryptionPasswordItem
      toSectionWithIdentifier:SectionIdentifierSyncEncryption];

  TableViewDetailTextItem* backupRecoveryKeyItem =
      [[TableViewDetailTextItem alloc] initWithType:
          ItemTypeBackupRecoveryKeyButton];
  backupRecoveryKeyItem.text =
      l10n_util::GetNSString(IDS_VIVALDI_BACKUP_ENCRYPTION_KEY);
  backupRecoveryKeyItem.accessorySymbol =
      TableViewDetailTextCellAccessorySymbolExternalLink;
  [model addItem:backupRecoveryKeyItem
      toSectionWithIdentifier:SectionIdentifierSyncEncryption];
}

- (void) addSignOutSection {
  TableViewModel* model = self.settingsConsumer.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierSyncSignOut];
  self.logOutButton = [[TableViewTextButtonItem alloc]
      initWithType:ItemTypeLogOutButton];
  self.logOutButton.buttonText =
      l10n_util::GetNSString(IDS_VIVALDI_ACCOUNT_LOG_OUT);
  self.logOutButton.textAlignment = NSTextAlignmentNatural;
  self.logOutButton.buttonTextColor = [UIColor colorNamed:kBlueColor];
  self.logOutButton.buttonBackgroundColor = [UIColor clearColor];
  self.logOutButton.disableButtonIntrinsicWidth = YES;

  [model addItem:self.logOutButton
      toSectionWithIdentifier:SectionIdentifierSyncSignOut];
}

- (void) addDeleteDataSection {
  TableViewModel* model = self.settingsConsumer.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierSyncDeleteData];
  self.deleteDataButton = [[TableViewTextButtonItem alloc]
      initWithType:ItemTypeDeleteDataButton];
  self.deleteDataButton.buttonText =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_CONFIRM_CLEAR_SERVER_DATA_TITLE);
  self.deleteDataButton.textAlignment = NSTextAlignmentNatural;
  self.deleteDataButton.disableButtonIntrinsicWidth = YES;
  self.deleteDataButton.buttonTextColor = [UIColor colorNamed:kRedColor];
  self.deleteDataButton.buttonBackgroundColor = [UIColor clearColor];

  [model addItem:self.deleteDataButton
      toSectionWithIdentifier:SectionIdentifierSyncDeleteData];
}

- (void)addItemsToUI:(NSArray*)items toSection:(NSInteger)sectionIdentifier {
  for (TableViewItem* item: items) {
    [self.settingsConsumer.tableViewModel addItem:item
        toSectionWithIdentifier:sectionIdentifier];
  }
}

- (void)removeItemsFromUI:(NSArray*)items {
  if (!self.settingsConsumer.tableViewModel || items.count == 0) {
    return;
  }
  NSMutableArray* indexPaths = [[NSMutableArray alloc] init];
  for (TableViewItem* item: items) {
    NSIndexPath* itemIndexPath =
        [self.settingsConsumer.tableViewModel indexPathForItem:item];
    if (itemIndexPath) {
      [indexPaths addObject:itemIndexPath];
    }
  }
  if (indexPaths.count == 0) {
    return;
  }
  ChromeTableViewController* tableViewController =
    base::mac::ObjCCast<ChromeTableViewController>(self.settingsConsumer);
  [tableViewController removeFromModelItemAtIndexPaths:indexPaths];
  [self.settingsConsumer.tableView deleteRowsAtIndexPaths:indexPaths
                      withRowAnimation:UITableViewRowAnimationAutomatic];
}

- (void)updateStartSyncingSection {
  if (!self.settingsConsumer.tableViewModel) {
    return;
  }
  if (![self.settingsConsumer.tableViewModel
          hasSectionForSectionIdentifier:SectionIdentifierSyncStartSyncing]) {
    return;
  }
  NSIndexPath* itemIndexPath = [self.settingsConsumer.tableViewModel
      indexPathForItem:self.startSyncingButton];
  if (self.segmentedControlItem.selectedItem == SyncAll) {
    if (!itemIndexPath) {
      [self.settingsConsumer.tableViewModel addItem:self.startSyncingButton
          toSectionWithIdentifier:SectionIdentifierSyncStartSyncing];
    }
  } else {
    if (itemIndexPath) {
      [self.settingsConsumer.tableViewModel
          removeItemWithType:ItemTypeStartSyncingButton
          fromSectionWithIdentifier:SectionIdentifierSyncStartSyncing];
    }
  }
}

- (void)initSyncItemArrays {
  TableViewTextItem* syncAllInfoTextbox =
      [[TableViewTextItem alloc] initWithType:ItemTypeSyncAllInfoTextbox];
  syncAllInfoTextbox.textAlignment = NSTextAlignmentLeft;
  syncAllInfoTextbox.text = l10n_util::GetNSString(IDS_VIVALDI_SYNC_EVERYTHING);

  _syncAllItems = @[syncAllInfoTextbox];

  TableViewSwitchItem* switchItemBookmarks =
    [[TableViewSwitchItem alloc] initWithType:ItemTypeSyncBookmarksSwitch];
  switchItemBookmarks.text = l10n_util::GetNSString(IDS_VIVALDI_SYNC_BOOKMARKS);
  switchItemBookmarks.detailText =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_BOOKMARKS_SUBTITLE);
  switchItemBookmarks.on = engineData.data_types.Has(
      UserSelectableType::kBookmarks);
  switchItemBookmarks.iconImage = [UIImage imageNamed:@"sync_bookmarks"];

  TableViewSwitchItem* switchItemSettings =
    [[TableViewSwitchItem alloc] initWithType:ItemTypeSyncSettingsSwitch];
  switchItemSettings.text = l10n_util::GetNSString(IDS_VIVALDI_SYNC_PREFERENCES);
  switchItemSettings.detailText =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_PREFERENCES_SUBTITLE);
  switchItemSettings.on = engineData.data_types.Has(
      UserSelectableType::kPreferences);
  switchItemSettings.iconImage = [UIImage imageNamed:@"sync_settings"];

  TableViewSwitchItem* switchItemPasswords =
    [[TableViewSwitchItem alloc] initWithType:ItemTypeSyncPasswordsSwitch];
  switchItemPasswords.text = l10n_util::GetNSString(IDS_VIVALDI_SYNC_PASSWORDS);
  switchItemPasswords.detailText =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_PASSWORDS_SUBTITLE);
  switchItemPasswords.on = engineData.data_types.Has(
      UserSelectableType::kPasswords);
  switchItemPasswords.iconImage = [UIImage imageNamed:@"sync_passwords"];

  TableViewSwitchItem* switchItemAutofill =
    [[TableViewSwitchItem alloc] initWithType:ItemTypeSyncAutofillSwitch];
  switchItemAutofill.text = l10n_util::GetNSString(IDS_VIVALDI_SYNC_AUTOFILL);
  switchItemAutofill.detailText =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_AUTOFILL_SUBTITLE);
  switchItemAutofill.on = engineData.data_types.Has(
      UserSelectableType::kAutofill);
  switchItemAutofill.iconImage = [UIImage imageNamed:@"sync_autofill"];

  TableViewSwitchItem* switchItemHistory =
    [[TableViewSwitchItem alloc] initWithType:ItemTypeSyncHistorySwitch];
  switchItemHistory.text = l10n_util::GetNSString(IDS_VIVALDI_SYNC_TYPED_URLS);
  switchItemHistory.detailText =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_TYPED_URLS_SUBTITLE);
  switchItemHistory.on = engineData.data_types.Has(
      UserSelectableType::kHistory);
  switchItemHistory.iconImage = [UIImage imageNamed:@"sync_history"];

  _syncSelectedItems = @[
    switchItemBookmarks,
    switchItemSettings,
    switchItemPasswords,
    switchItemAutofill,
    switchItemHistory,
  ];
}

-(NSInteger)getSimplifiedAccountState {
  if (_vivaldiAccountManager->has_refresh_token()) {
    return LOGGED_IN;
  } else if (_vivaldiAccountManager->last_token_fetch_error().
      server_message.find(ERROR_ACCOUNT_NOT_ACTIVATED) != std::string::npos) {
    return NOT_ACTIVATED;
  } else if (_vivaldiAccountManager->last_token_fetch_error().type ==
      VivaldiAccountManager::FetchErrorType::INVALID_CREDENTIALS) {
    return LOGIN_FAILED;
  } else if (!_vivaldiAccountManager->GetTokenRequestTime().is_null()) {
    return LOGGING_IN;
  } else if (!_vivaldiAccountManager->account_info().account_id.empty()) {
    return CREDENTIALS_MISSING;
  } else {
    return LOGGED_OUT;
  }
}

- (void)setChosenTypes:(UserSelectableTypeSet)types syncAll:(BOOL)syncAll {
  if (!_syncSetupInProgressHandle.get()) {
    _syncSetupInProgressHandle = _syncService->GetSetupInProgressHandle();
  }

  _syncService->GetUserSettings()->SetSelectedTypes(syncAll, types);
  _syncService->GetUserSettings()->SetFirstSetupComplete(
        syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);

  _syncSetupInProgressHandle.reset();
}

NSString* GetBackupEncryptionKeyPath() {
  NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                       NSUserDomainMask, YES);
  if ([paths count] < 1)
    return nil;

  NSString* documents_directory_path = [paths objectAtIndex:0];
  return [documents_directory_path
      stringByAppendingPathComponent:@"BackupEncryptionKey.txt"];
}

#pragma mark Private - Create Account Server Request Handling

- (void)sendCreateAccountRequestToServer:(BOOL)wantsNewsletter
                    completionHandler:(ServerRequestCompletionHandler)handler {
  PrefService* pref_service = GetApplicationContext()->GetLocalState();
  std::string locale =
    pref_service->HasPrefPath(language::prefs::kApplicationLocale)
        ? pref_service->GetString(language::prefs::kApplicationLocale)
        : GetApplicationContext()->GetApplicationLocale();

  base::Value::Dict dict;
  dict.Set(vParamUsername, pendingRegistration.username);
  dict.Set(vParamPassword, pendingRegistration.password);
  dict.Set(vParamEmailAddress, pendingRegistration.recoveryEmailAddress);
  dict.Set(vParamAge, pendingRegistration.age);
  dict.Set(vParamLanguage, locale);
  dict.Set(vParamDisableNonce, vDisableNonceValue);
  dict.Set(vParamSubscribeNewletter, wantsNewsletter);

  NSURL* url = [NSURL URLWithString:vVivaldiSyncRegistrationUrl];
  sendRequestToServer(std::move(dict), url, handler, self.task);
}

- (void)onCreateAccountResponse:(NSData*)data
                       response:(NSURLResponse*)response
                          error:(NSError*)error
                     deviceName:(NSString*)deviceName {
  absl::optional<base::Value> readResult = NSDataToDict(data);
  if (!readResult.has_value()) {
    [self.commandHandler createAccountFailed:vErrorCodeOther];
    return;
  }

  base::Value val = std::move(readResult).value();
  const base::Value::Dict& dict = val.GetDict();
  if (!error && dict.FindString(vSuccessKey)) {
    [self setPendingRegistration];
    [self setSessionName:deviceName];
    // NOTE(tomas@vivaldi.com): The login request fails since the account is
    // not verified yet. This will trigger the activation process
    // and be handled accordingly
    _vivaldiAccountManager->Login(
        pendingRegistration.username, pendingRegistration.password, false);
    _syncService->GetUserSettings()->SetSyncRequested(true);
  } else {
    const std::string* err = dict.FindString(vErrorKey);
    [self.commandHandler createAccountFailed:SysUTF8ToNSString(*err)];
  }

}

- (void)handleNotActivated {
  auto pr = [self getPendingRegistration];
  if (pr) {
    pendingRegistration.username = *pr->FindString(kUsernameKey);
    pendingRegistration.password = *pr->FindString(kPasswordKey);
    pendingRegistration.recoveryEmailAddress = *pr->FindString(kRecoveryEmailKey);
  }
  [self.commandHandler showActivateAccountView];
}

- (void)setPendingRegistration {
  std::string encrypted_password;
  // iOS uses the posix implementation, which is non-blocking.
  if (!OSCrypt::EncryptString(
        pendingRegistration.password, &encrypted_password)) {
    return;
  }

  std::string encoded_password;
  base::Base64Encode(encrypted_password, &encoded_password);
  base::Value pending_registration(base::Value::Type::DICT);

  pending_registration.SetStringKey(kRecoveryEmailKey,
      pendingRegistration.recoveryEmailAddress);
  pending_registration.SetStringKey(kUsernameKey, pendingRegistration.username);
  pending_registration.SetStringKey(kPasswordKey, encoded_password);

  _prefService->Set(vivaldiprefs::kVivaldiAccountPendingRegistration,
              pending_registration);
}

- (std::unique_ptr<base::Value::Dict>)getPendingRegistration {
  const base::Value& pref_value = _prefService->GetValue(
      vivaldiprefs::kVivaldiAccountPendingRegistration);

  const std::string* username =
      pref_value.FindStringKey(kUsernameKey);
  const std::string* encoded_password =
      pref_value.FindStringKey(kPasswordKey);
  const std::string* recovery_email =
      pref_value.FindStringKey(kRecoveryEmailKey);

  if (!username || !encoded_password || !recovery_email)
    return nullptr;

  std::string encrypted_password;
  if (!base::Base64Decode(*encoded_password, &encrypted_password) ||
      encrypted_password.empty()) {
    return nullptr;
  }
  std::string password;
  // iOS uses the posix implementation, which is non-blocking.
  if (!OSCrypt::DecryptString(encrypted_password, &password)) {
    return nullptr;
  }
  auto pending_registration = std::make_unique<base::Value::Dict>();
  pending_registration->Set(kUsernameKey, *username);
  pending_registration->Set(kPasswordKey, password);
  pending_registration->Set(kRecoveryEmailKey, *recovery_email);
  return pending_registration;
}

- (bool)hasPendingRegistration {
  return _prefService->HasPrefPath(
      vivaldiprefs::kVivaldiAccountPendingRegistration);
}

- (void)setSessionName:(NSString*)name {
  _prefService->SetString(
      vivaldiprefs::kSyncSessionName, SysNSStringToUTF8(name));
}

@end
