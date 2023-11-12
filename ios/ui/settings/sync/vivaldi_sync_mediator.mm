// Copyright 2022-2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/sync/vivaldi_sync_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/base64.h"
#import "base/containers/flat_map.h"
#import "base/files/file_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "base/values.h"
#import "components/language/core/browser/pref_names.h"
#import "components/os_crypt/sync/os_crypt.h"
#import "components/prefs/pref_service.h"
#import "components/sync/base/command_line_switches.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service_observer.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/ui/common/vivaldi_url_constants.h"
#import "ios/ui/settings/sync/cells/vivaldi_table_view_sync_status_item.h"
#import "ios/ui/settings/sync/cells/vivaldi_table_view_sync_user_info_item.h"
#import "ios/ui/settings/sync/manager/vivaldi_account_simplified_state.h"
#import "ios/ui/settings/sync/manager/vivaldi_account_sync_manager_consumer.h"
#import "ios/ui/settings/sync/manager/vivaldi_account_sync_manager.h"
#import "ios/ui/settings/sync/manager/vivaldi_sync_simplified_state.h"
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

struct PendingRegistration {
std::string username;
int age;
std::string recoveryEmailAddress;
std::string password;
};

@interface VivaldiSyncMediator () <VivaldiAccountSyncManagerConsumer> {
PendingRegistration pendingRegistration;
}

@property(nonatomic, assign) VivaldiSyncServiceImpl* syncService;
@property(nonatomic, assign) VivaldiAccountManager* vivaldiAccountManager;
@property(nonatomic, assign) PrefService* prefService;
@property(nonatomic, strong) VivaldiAccountSyncManager* syncManager;

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

@interface VivaldiSyncMediator () {
  EngineData engineData;
  VivaldiSyncUIHelper::CycleData cycleData;
  bool loggingOut;
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

    VivaldiAccountSyncManager* syncManager =
        [[VivaldiAccountSyncManager alloc]
            initWithAccountManager:_vivaldiAccountManager
                       syncService:_syncService];
    _syncManager = syncManager;
    _syncManager.consumer = self;
    [_syncManager start];

    self.formatter = [[NSDateFormatter alloc] init];
    [self.formatter setDateFormat:@"dd/MM/yyyy HH:mm:ss"];

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
  if (_syncManager) {
    [_syncManager stop];
    _syncManager = nil;
  }
}

- (void)startMediating {
  DCHECK(self.commandHandler);
  switch ([self getSimplifiedAccountState]) {
    case LOGGING_IN:
    case LOGGED_IN: {
      [self onVivaldiSyncStateChanged];
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
    default:
      NOTREACHED();
      break;
  }
}

- (void)disconnect {
  if (_syncManager) {
    [_syncManager stop];
    _syncManager = nil;
  }
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
  _syncService->SetSyncFeatureRequested();
}

- (BOOL)setEncryptionPassword:(std::string)password {
  return _syncService->ui_helper()->SetEncryptionPassword(password);
}

- (BOOL)importEncryptionPassword:(NSURL*)file {
  NSError* error = nil;
  NSString* key = [NSString stringWithContentsOfFile:[file path]
                                            encoding:NSUTF8StringEncoding
                                               error:&error];
  if (!error && [key length]) {
    return _syncService->ui_helper()->RestoreEncryptionToken(
      base::SysNSStringToUTF8(key));
  }
  return NO;
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

#pragma mark - VivaldiAccountSyncManagerConsumer

- (void)onVivaldiSyncStateChanged {
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

  // TODO: (tomas@vivaldi.com) or (prio@vivaldi.com):
  // Refactor this by replacing the engineData and cycleData calls with getters
  // from VivaldiAccountSyncManager. That will help avoid duplication of sync
  // and account state management in different places.

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
      [self reloadSyncStatusItem];
      break;
    case EngineState::CONFIGURATION_PENDING:
    case EngineState::STARTED:
      if (engineData.needs_decryption_password) {
        [self.commandHandler showSyncEncryptionPasswordView];
      } else if (!engineData.uses_encryption_password) {
        // There is a different message displayed when creating the
        // encryption key for the first time.
        // We land here when the server has no encryption password set
        [self.commandHandler showSyncCreateEncryptionPasswordView];
      } else {
        [self.commandHandler showSyncSettingsView];
        [self reloadSyncStatusItem];
      }
      break;
    default:
      NOTREACHED();
      break;
  }
}

-(void)onVivaldiSyncCycleCompleted {
  [self onVivaldiSyncStateChanged];
}

-(void)onVivaldiAccountUpdated {
  if (!loggingOut) {
    [self onVivaldiSyncStateChanged];
  }
  if (loggingOut && [self getSimplifiedAccountState] == LOGGED_OUT) {
    loggingOut = false;
  }
}

-(void)onTokenFetchSucceeded {
  [self onVivaldiSyncStateChanged];
}

-(void)onTokenFetchFailed {
  if ([self getSimplifiedAccountState] == LOGIN_FAILED) {
    [self.commandHandler loginFailed:l10n_util::GetNSString(
        IDS_VIVALDI_ACCOUNT_LOG_IN_FAILED)];
    return;
  }
  [self onVivaldiSyncStateChanged];
}

#pragma mark - VivaldiSyncSettingsViewControllerServiceDelegate

- (void)removeTempBackupEncryptionKeyFile:(NSString*)filePath {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(^{
        NSFileManager* fileManager = [NSFileManager defaultManager];
        base::ScopedBlockingCall scoped_blocking_call(
            FROM_HERE, base::BlockingType::WILL_BLOCK);
        [fileManager removeItemAtPath:filePath error:nil];
      }));
}

- (NSString*)createTempBackupEncryptionKeyFile {
  NSString* filePath = [NSTemporaryDirectory()
      stringByAppendingPathComponent:@"BackupEncryptionKey.txt"];

  NSString* key = SysUTF8ToNSString(
    _syncService->ui_helper()->GetBackupEncryptionToken());
  [key writeToFile:filePath
        atomically:YES
          encoding:NSUTF8StringEncoding
             error:nil];
  return filePath;
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
  _vivaldiAccountManager->Logout();
  [self clearPendingRegistration];
  [self.commandHandler resetViewControllers];
  [self.commandHandler showSyncLoginView];
}

- (void)startSyncingAllButtonPressed {
  if (!_syncManager)
    return;
  [_syncManager enableAllSync];
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
      [self refreshSyncSelectedItems];
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
  if (!_syncManager)
    return;
  [_syncManager updateSettingsType:type isOn:isOn];
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

- (void)updateSyncStatusItem {
  NSString* statusText;
  int color = 0;
  NSDate* lastSyncDate = nil;

  // TODO: (tomas@vivaldi.com) or (prio@vivaldi.com):
  // Refactor this by replacing the engineData and cycleData calls with getters
  // from VivaldiAccountSyncManager. That will help avoid duplication of sync
  // and account state management in different places.

  if (engineData.engine_state == EngineState::FAILED) {
    statusText = l10n_util::GetNSString(IDS_VIVALDI_SYNC_FAILED);
    color = kSyncStatusRedBackgroundColor;
    // TODO(tomas@vivaldi.com): Some of these strings are not used on the
    // android side, only on desktop. Should they be used on iOS?
    if (engineData.disable_reasons.Has(
        SyncService::DISABLE_REASON_NOT_SIGNED_IN)) {
      // We might enter this state for a brief moment after login while
      // the credentials are being passed to sync itself.
      statusText = @"Waiting for account details";
      color = kSyncStatusYellowBackgroundColor;
    } else if (!syncer::IsSyncAllowedByFlag()) {
      // Disabled by command line flag
      statusText = @"Sync was disabled from the command line";
      color = kSyncStatusYellowBackgroundColor;
    } else if (engineData.disable_reasons.Has(
        SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR)) {
      switch (engineData.protocol_error_client_action) {
        case ClientAction::UPGRADE_CLIENT:
          statusText =
              l10n_util::GetNSString(IDS_VIVALDI_SYNC_CLIENT_NEEDS_UPDATE);
          break;
        default:
          statusText = @"Sync must be restarted: ";
          statusText = [statusText stringByAppendingString:
              SysUTF8ToNSString(engineData.protocol_error_description)];
      }
    }
  } else if (engineData.engine_state == EngineState::CLEARING_DATA) {
    statusText = l10n_util::GetNSString(IDS_VIVALDI_SYNC_CLEARING_SERVER_DATA);
    color = kSyncStatusBlueBackgroundColor;
  } else if (engineData.engine_state == EngineState::CONFIGURATION_PENDING) {
    statusText = l10n_util::GetNSString(IDS_SYNC_ALL_SET_TEXT);;
    color = kSyncStatusBlueBackgroundColor;
  } else if (engineData.engine_state == EngineState::STARTED) {
    if (cycleData.download_updates_status ==
            VivaldiSyncUIHelper::CycleStatus::SUCCESS &&
        cycleData.commit_status == VivaldiSyncUIHelper::CycleStatus::SUCCESS) {
      statusText = l10n_util::GetNSString([self getCycleStatusMessageId:
          cycleData.commit_status]);
      color = [self getCycleStatusColor:cycleData.commit_status];
      lastSyncDate = cycleData.cycle_start_time.ToNSDate();
    } else {
      statusText = l10n_util::GetNSString([self getCycleStatusMessageId:
          cycleData.download_updates_status]);
      color = [self getCycleStatusColor:cycleData.download_updates_status];
    }
  } else if (engineData.engine_state == EngineState::STARTING_SERVER_ERROR) {
    statusText = l10n_util::GetNSString(IDS_VIVALDI_SYNC_WAITING_SERVER_ERROR);
    color = kSyncStatusYellowBackgroundColor;
  } else if (engineData.engine_state == EngineState::STARTING) {
    statusText = l10n_util::GetNSString(IDS_VIVALDI_SYNC_INITIALIZING);
    color = kSyncStatusBlueBackgroundColor;
  } else if (engineData.engine_state == EngineState::STOPPED) {
    statusText = @" ";
    color = kSyncStatusYellowBackgroundColor;
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
    self.syncStatusItem.statusBackgroundColor = UIColorFromRGB(color);
  } else {
    self.syncStatusItem.statusBackgroundColor = [UIColor clearColor];
  }
}

- (int)getCycleStatusMessageId:(int)status {
  switch (status) {
    case VivaldiSyncUIHelper::CycleStatus::NOT_SYNCED:
      return IDS_VIVALDI_SYNC_NOT_SYNCED_CYCLE;
    case VivaldiSyncUIHelper::CycleStatus::SUCCESS:
      return IDS_VIVALDI_SYNC_SUCCESS;
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
      return kSyncStatusGreenBackgroundColor;
    case VivaldiSyncUIHelper::CycleStatus::NOT_SYNCED:
    case VivaldiSyncUIHelper::CycleStatus::NETWORK_ERROR:
    case VivaldiSyncUIHelper::CycleStatus::THROTTLED:
      return kSyncStatusYellowBackgroundColor;
    case VivaldiSyncUIHelper::CycleStatus::AUTH_ERROR:
    case VivaldiSyncUIHelper::CycleStatus::SERVER_ERROR:
    case VivaldiSyncUIHelper::CycleStatus::CONFLICT:
    default:
      return kSyncStatusRedBackgroundColor;
  }
}

- (void) reloadSyncStatusItem {
  if (!self.settingsConsumer) {
    return;
  }

  [self updateSyncStatusItem];
  [self.settingsConsumer reloadItem:self.syncStatusItem];

  if ([self.settingsConsumer.tableViewModel
      hasSectionForSectionIdentifier:SectionIdentifierSyncStartSyncing] &&
      self.startSyncingButton.enabled != [self getStartSyncingButtonEnabled]) {
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
  // Add a default avatar image
  self.userInfoItem.userAvatar =
      [UIImage systemImageNamed:@"person.circle.fill"];
  self.userInfoItem.userName = SysUTF8ToNSString(account_info.username);
  [self.settingsConsumer.tableViewModel setHeader:self.userInfoItem
      forSectionWithIdentifier:SectionIdentifierSyncUserInfo];

  __weak VivaldiSyncMediator* weakSelf = self;
  NSURL* profileImageURL =
      [NSURL URLWithString:SysUTF8ToNSString(account_info.picture_url)];
  [self fetchProfilePicture:profileImageURL
    completionHandler:^(NSData* data, NSURLResponse* response, NSError* error) {
    if (error) {
      return;
    }

    dispatch_async(dispatch_get_main_queue(), ^{
      weakSelf.userInfoItem.userAvatar = [UIImage imageWithData:data];
      [weakSelf.settingsConsumer reloadSection:SectionIdentifierSyncUserInfo];
    });
  }];
}

-(void) fetchProfilePicture:(NSURL*)url
          completionHandler:(ServerRequestCompletionHandler)handler {
  NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:url
                          cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
                                      timeoutInterval:vConnectionTimeout];
  [request setHTTPMethod:@"GET"];
  [request setHTTPBody:nil];

  NSURLSession* session = [NSURLSession sharedSession];
  _task =
      [session dataTaskWithRequest:request
                completionHandler:handler];
  [_task resume];
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
  self.segmentedControlItem.selectedItem = syncAllData ? SyncAll : SyncSelected;
  [model addItem:self.segmentedControlItem
      toSectionWithIdentifier:SectionIdentifierSyncItems];
  [self initSyncItemArrays];

  if (syncAllData) {
    [self addItemsToUI:self.syncAllItems
        toSection:SectionIdentifierSyncItems];
  } else {
    [self addItemsToUI:self.syncSelectedItems
        toSection:SectionIdentifierSyncItems];
  }
}

- (void) addStartSyncingSection {
  TableViewModel* model = self.settingsConsumer.tableViewModel;
  self.startSyncingButton = [[VivaldiTableViewTextButtonItem alloc]
      initWithType:ItemTypeStartSyncingButton];
  self.startSyncingButton.buttonText =
      l10n_util::GetNSString(IDS_VIVALDI_START_SYNCING);
  self.startSyncingButton.textAlignment = NSTextAlignmentNatural;
  self.startSyncingButton.buttonTextColor = [UIColor colorNamed:kBlueColor];
  self.startSyncingButton.buttonBackgroundColor =
      [UIColor colorNamed:kBackgroundColor];
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
      base::apple::ObjCCast<ChromeTableViewController>(self.settingsConsumer);
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
  if (!_syncManager)
    return;

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
  switchItemBookmarks.on = [_syncManager isSyncBookmarksEnabled];
  switchItemBookmarks.iconImage = [UIImage imageNamed:@"sync_bookmarks"];

  TableViewSwitchItem* switchItemSettings =
    [[TableViewSwitchItem alloc] initWithType:ItemTypeSyncSettingsSwitch];
  switchItemSettings.text = l10n_util::GetNSString(IDS_VIVALDI_SYNC_PREFERENCES);
  switchItemSettings.detailText =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_PREFERENCES_SUBTITLE);
  switchItemSettings.on = [_syncManager isSyncSettingsEnabled];
  switchItemSettings.iconImage = [UIImage imageNamed:@"sync_settings"];

  TableViewSwitchItem* switchItemPasswords =
    [[TableViewSwitchItem alloc] initWithType:ItemTypeSyncPasswordsSwitch];
  switchItemPasswords.text = l10n_util::GetNSString(IDS_VIVALDI_SYNC_PASSWORDS);
  switchItemPasswords.detailText =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_PASSWORDS_SUBTITLE);
  switchItemPasswords.on = [_syncManager isSyncPasswordsEnabled];
  switchItemPasswords.iconImage = [UIImage imageNamed:@"sync_passwords"];

  TableViewSwitchItem* switchItemAutofill =
    [[TableViewSwitchItem alloc] initWithType:ItemTypeSyncAutofillSwitch];
  switchItemAutofill.text = l10n_util::GetNSString(IDS_VIVALDI_SYNC_AUTOFILL);
  switchItemAutofill.detailText =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_AUTOFILL_SUBTITLE);
  switchItemAutofill.on = [_syncManager isSyncAutofillEnabled];
  switchItemAutofill.iconImage = [UIImage imageNamed:@"sync_autofill"];

  TableViewSwitchItem* switchItemTabs =
    [[TableViewSwitchItem alloc] initWithType:ItemTypeSyncTabsSwitch];
  switchItemTabs.text = l10n_util::GetNSString(IDS_VIVALDI_SYNC_TABS);
  switchItemTabs.detailText =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_TABS_SUBTITLE);
  switchItemTabs.on = [_syncManager isSyncTabsEnabled];
  switchItemTabs.iconImage = [UIImage imageNamed:@"sync_tabs"];

  TableViewSwitchItem* switchItemHistory =
    [[TableViewSwitchItem alloc] initWithType:ItemTypeSyncHistorySwitch];
  switchItemHistory.text = l10n_util::GetNSString(IDS_VIVALDI_SYNC_TYPED_URLS);
  switchItemHistory.detailText =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_TYPED_URLS_SUBTITLE);
  switchItemHistory.on = [_syncManager isSyncHistoryEnabled];
  switchItemHistory.iconImage = [UIImage imageNamed:@"sync_history"];

  TableViewSwitchItem* switchItemReadingList =
    [[TableViewSwitchItem alloc] initWithType:ItemTypeSyncReadingListSwitch];
  switchItemReadingList.text =
      l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_READING_LIST);
  switchItemReadingList.detailText =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_READING_LIST_SUBTITLE);
  switchItemReadingList.on = [_syncManager isSyncReadingListEnabled];
  switchItemReadingList.iconImage = [UIImage imageNamed:@"sync_notes"];

  TableViewSwitchItem* switchItemNotes =
    [[TableViewSwitchItem alloc] initWithType:ItemTypeSyncNotesSwitch];
  switchItemNotes.text = l10n_util::GetNSString(IDS_VIVALDI_TOOLS_MENU_NOTES);
  switchItemNotes.detailText =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_NOTES_LIST_SUBTITLE);
  switchItemNotes.on = [_syncManager isSyncNotesEnabled];
  switchItemNotes.iconImage = [UIImage imageNamed:@"sync_readinglist"];

  _syncSelectedItems = @[
    switchItemBookmarks,
    switchItemSettings,
    switchItemPasswords,
    switchItemAutofill,
    switchItemTabs,
    switchItemHistory,
    switchItemReadingList,
    switchItemNotes,
  ];
}

- (void)refreshSyncSelectedItems {
  if (!_syncManager)
    return;

  for (TableViewSwitchItem* item in _syncSelectedItems) {
    switch (item.type) {
      case ItemTypeSyncBookmarksSwitch:
        item.on = [_syncManager isSyncBookmarksEnabled];
        break;
      case ItemTypeSyncSettingsSwitch:
        item.on = [_syncManager isSyncSettingsEnabled];
        break;
      case ItemTypeSyncPasswordsSwitch:
        item.on = [_syncManager isSyncPasswordsEnabled];
        break;
      case ItemTypeSyncAutofillSwitch:
        item.on = [_syncManager isSyncAutofillEnabled];
        break;
      case ItemTypeSyncTabsSwitch:
        item.on = [_syncManager isSyncTabsEnabled];
        break;
      case ItemTypeSyncHistorySwitch:
        item.on = [_syncManager isSyncHistoryEnabled];
        break;
      case ItemTypeSyncReadingListSwitch:
        item.on = [_syncManager isSyncReadingListEnabled];
        break;
      case ItemTypeSyncNotesSwitch:
        item.on = [_syncManager isSyncNotesEnabled];
        break;
      default:
        break;
    }
  }
}

-(NSInteger)getSimplifiedAccountState {
  if (!_syncManager)
    return LOGGED_OUT;

  return [_syncManager getCurrentAccountState];
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
    _syncService->SetSyncFeatureRequested();
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

  pending_registration.GetDict().Set(kRecoveryEmailKey,
      pendingRegistration.recoveryEmailAddress);
  pending_registration.GetDict().Set(kUsernameKey, pendingRegistration.username);
  pending_registration.GetDict().Set(kPasswordKey, encoded_password);

  _prefService->Set(vivaldiprefs::kVivaldiAccountPendingRegistration,
              pending_registration);
}

- (std::unique_ptr<base::Value::Dict>)getPendingRegistration {
  const base::Value& pref_value = _prefService->GetValue(
      vivaldiprefs::kVivaldiAccountPendingRegistration);

  const std::string* username =
      pref_value.GetDict().FindString(kUsernameKey);
  const std::string* encoded_password =
      pref_value.GetDict().FindString(kPasswordKey);
  const std::string* recovery_email =
      pref_value.GetDict().FindString(kRecoveryEmailKey);

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
