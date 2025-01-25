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
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_observer.h"
#import "components/sync/service/sync_user_settings.h"
#import "components/sync_device_info/local_device_info_util.h"
#import "ios/chrome/browser/net/model/crurl.h"
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
#import "ios/ui/settings/sync/manager/vivaldi_account_sync_manager.h"
#import "ios/ui/settings/sync/manager/vivaldi_account_sync_manager_consumer.h"
#import "ios/ui/settings/sync/manager/vivaldi_sync_simplified_state.h"
#import "ios/ui/settings/sync/vivaldi_create_account_ui_helper.h"
#import "ios/ui/settings/sync/vivaldi_sync_settings_constants.h"
#import "ios/ui/settings/sync/vivaldi_sync_settings_view_controller.h"
#import "ios/ui/table_view/cells/vivaldi_table_view_segmented_control_item.h"
#import "ios/ui/table_view/cells/vivaldi_table_view_text_button_item.h"
#import "net/base/network_change_notifier.h"
#import "prefs/vivaldi_pref_names.h"
#import "sync/vivaldi_sync_service_impl.h"
#import "sync/vivaldi_sync_ui_helpers.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"
#import "vivaldi/prefs/vivaldi_gen_prefs.h"
#import "vivaldi_account/vivaldi_account_manager.h"

using base::SysNSStringToUTF8;
using base::SysUTF8ToNSString;
using syncer::ClientAction;
using syncer::SyncProtocolErrorType;
using syncer::SyncService;
using syncer::UserSelectableType;
using syncer::UserSelectableTypeSet;
using vivaldi::VivaldiAccountManager;
using vivaldi::sync_ui_helpers::CycleData;
using vivaldi::sync_ui_helpers::CycleStatus;
using vivaldi::sync_ui_helpers::EngineData;
using vivaldi::sync_ui_helpers::EngineState;

// Donation Support Tiers
typedef enum {
  BadgeTierNone = 0,
  BadgeTierSupporter,
  BadgeTierPatron,
  BadgeTierAdvocate
} BadgeTier;

struct PendingRegistration {
  std::string username;
  int age;
  std::string recoveryEmailAddress;
  std::string password;
};

@interface VivaldiSyncMediator () <VivaldiAccountSyncManagerConsumer> {
  PendingRegistration pendingRegistration;
}

@property(nonatomic, assign) syncer::SyncService* syncService;
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
@property(nonatomic, strong) NSString* localDeviceClientName;

@property(nonatomic, copy) NSURLSessionDataTask* task;

@end

@interface VivaldiSyncMediator () {
  EngineData engineData;
  CycleData cycleData;
  bool loggingOut;
}

@end

@implementation VivaldiSyncMediator

- (instancetype)initWithAccountManager:
                    (VivaldiAccountManager*)vivaldiAccountManager
                           syncService:(syncer::SyncService*)syncService
                           prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _vivaldiAccountManager = vivaldiAccountManager;
    _syncService = syncService;
    _prefService = prefService;
    loggingOut = false;

    VivaldiAccountSyncManager* syncManager = [[VivaldiAccountSyncManager alloc]
        initWithAccountManager:_vivaldiAccountManager
                   syncService:_syncService];
    _syncManager = syncManager;
    _syncManager.consumer = self;
    [_syncManager start];

    self.formatter = [[NSDateFormatter alloc] init];
    [self.formatter setDateFormat:@"dd/MM/yyyy HH:mm:ss"];

    // The order must match the SyncType enum
    self.segmentedControlLabels =
        [NSArray arrayWithObjects:l10n_util::GetNSString(
                                      IDS_VIVALDI_SYNC_AUTOMATIC_TITLE),
                                  l10n_util::GetNSString(
                                      IDS_VIVALDI_SYNC_DATA_CHOOSE_TITLE),
                                  nil];

    [self geLocalDeviceClientName];
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
                               base::SysNSStringToUTF16(
                                   [@(errorCode) stringValue]))
                attributes:textAttributes];
        errorMessage = [errorMsg string];
      } else if (errorCode == -1) {
        errorMessage =
            l10n_util::GetNSString(IDS_VIVALDI_ACCOUNT_ERROR_WRONG_DOMAIN);
      } else {
        NSAttributedString* errorMsg = [[NSAttributedString alloc]
            initWithString:l10n_util::GetNSStringF(
                               IDS_VIVALDI_ACCOUNT_ERROR_CREDENTIALS_REJECTED_2,
                               base::SysNSStringToUTF16(
                                   [@(errorCode) stringValue]))
                attributes:textAttributes];
        errorMessage = [errorMsg string];
      }
      [self.commandHandler loginFailed:errorMessage];
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
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
  self.localDeviceClientName = nil;
}

- (void)requestPendingRegistrationLogin {
  [self login:pendingRegistration.username
           password:pendingRegistration.password
         deviceName:[self sessionName]
      save_password:false];
}

- (NSString*)getPendingRegistrationUsername {
  return SysUTF8ToNSString(pendingRegistration.username);
}

- (NSString*)getPendingRegistrationEmail {
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
       deviceName:(NSString*)deviceName
    save_password:(BOOL)save_password {
  // If the user has started registration on another device, and tries to log in
  // here, we store the username and password in case we get a NOT_ACTIVE
  // reply from the server. There will not be a pending registration in prefs
  pendingRegistration.username = username;
  pendingRegistration.password = password;
  pendingRegistration.recoveryEmailAddress = SysNSStringToUTF8(
      l10n_util::GetNSString(IDS_SYNC_DEFAULT_RECOVERY_EMAIL_ADDRESS));
  [self setSessionName:deviceName];
  _vivaldiAccountManager->Login(username, password, save_password);
  _syncService->SetSyncFeatureRequested();
}

- (BOOL)setEncryptionPassword:(std::string)password {
  return vivaldi::sync_ui_helpers::SetEncryptionPassword(_syncService,
                                                         password);
}

- (void)importEncryptionPassword:(NSURL*)file
               completionHandler:
                   (void (^)(NSString* errorMessage))completionHandler {
  __weak __typeof(self) weakSelf = self;
  void (^readingAccessor)(NSURL*) = ^(NSURL* newURL) {
    if (!weakSelf) {
      return;
    }
    base::ScopedBlockingCall inner_scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);
    NSFileManager* manager = [NSFileManager defaultManager];
    NSData* data = [manager contentsAtPath:[newURL path]];
    [weakSelf receivedData:data withCompletion:completionHandler];
  };
  NSError* error = nil;
  NSFileCoordinator* readingCoordinator =
      [[NSFileCoordinator alloc] initWithFilePresenter:nil];
  [readingCoordinator
      coordinateReadingItemAtURL:file
                         options:NSFileCoordinatorReadingWithoutChanges
                           error:&error
                      byAccessor:readingAccessor];

  if (error) {
    completionHandler([error localizedDescription]);
  }
}

- (void)receivedData:(NSData*)data
      withCompletion:(void (^)(NSString* message))completionHandler {
  NSString* key = [[NSString alloc] initWithBytes:data.bytes
                                           length:data.length
                                         encoding:NSUTF8StringEncoding];
  BOOL success = vivaldi::sync_ui_helpers::RestoreEncryptionToken(
      _syncService, base::SysNSStringToUTF8(key));
  if (success && [key length]) {
    completionHandler(@"");
    return;
  }
  completionHandler(
      l10n_util::GetNSString(IDS_VIVALDI_ACCOUNT_ENCRYPTION_PASSPHRASE_WRONG));
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
  [self
      sendCreateAccountRequestToServer:wantsNewsletter
                     completionHandler:^(NSData* data, NSURLResponse* response,
                                         NSError* error) {
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

  engineData = vivaldi::sync_ui_helpers::GetEngineData(_syncService);
  cycleData = vivaldi::sync_ui_helpers::GetCycleData(_syncService);

  if ([self getSimplifiedAccountState] == NOT_ACTIVATED ||
      (_vivaldiAccountManager->last_token_fetch_error()
           .server_message.empty() &&
       [self hasPendingRegistration])) {
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

  switch (engineData.engine_state) {
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
      if (!engineData.disable_reasons.Has(
              SyncService::DISABLE_REASON_NOT_SIGNED_IN)) {
        // We might enter this state for a brief moment after login while
        // the credentials are being passed to sync itself.
        // Don't show the UI just yet in this case.
        if ([self getSimplifiedAccountState] == LOGGED_IN) {
          // Show the settings view so the user can see the sync status
          [self.commandHandler showSyncSettingsView];
          [self reloadSyncStatusItem];
          return;
        }
      }
      ABSL_FALLTHROUGH_INTENDED;
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
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

- (void)onVivaldiSyncCycleCompleted {
  [self onVivaldiSyncStateChanged];
}

- (void)onVivaldiAccountUpdated {
  if (!loggingOut) {
    [self onVivaldiSyncStateChanged];
  }
  if (loggingOut && [self getSimplifiedAccountState] == LOGGED_OUT) {
    loggingOut = false;
  }
}

- (void)onTokenFetchSucceeded {
  [self onVivaldiSyncStateChanged];
}

- (void)onTokenFetchFailed {
  if ([self getSimplifiedAccountState] == LOGIN_FAILED) {
    [self.commandHandler
        loginFailed:l10n_util::GetNSString(IDS_VIVALDI_ACCOUNT_LOG_IN_FAILED)];
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
      vivaldi::sync_ui_helpers::GetBackupEncryptionToken(_syncService));
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

- (void)updateDeviceName:(NSString*)deviceName {
  [self setSessionName:deviceName];

  if (self.userInfoItem) {
    self.userInfoItem.sessionName = deviceName;
    [self updateUserInfoSection];
  }
}

- (void)updateChosenTypes:(UserSelectableType)type isOn:(BOOL)isOn {
  if (!_syncManager)
    return;
  [_syncManager updateSettingsType:type isOn:isOn];
}

- (BOOL)getSyncStatusFor:(NSInteger)type {
  return [self syncStatusForItemType:type];
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

- (void)updateUserInfoSection {
  if (![self.settingsConsumer.tableViewModel
          hasSectionForSectionIdentifier:SectionIdentifierSyncUserInfo]) {
    return;
  }
  [self.settingsConsumer reloadSection:SectionIdentifierSyncUserInfo];
}

- (void)updateSyncStatusItem {
  NSString* statusText;
  int color = 0;
  NSDate* lastSyncDate = nil;

  // TODO: (tomas@vivaldi.com) or (prio@vivaldi.com):
  // Refactor this by replacing the engineData and cycleData calls with getters
  // from VivaldiAccountSyncManager. That will help avoid duplication of sync
  // and account state management in different places.

  [self shouldAddSyncStatusSectionFooter:NO];

  if (engineData.engine_state == EngineState::FAILED) {
    statusText = l10n_util::GetNSString(IDS_VIVALDI_SYNC_FAILED);
    color = kSyncStatusRedBackgroundColor;
    // TODO(tomas@vivaldi.com): Some of these strings are not used on the
    // android side, only on desktop. Should they be used on iOS?
    if (!syncer::IsSyncAllowedByFlag()) {
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
          statusText = [statusText
              stringByAppendingString:SysUTF8ToNSString(
                                          engineData
                                              .protocol_error_description)];
      }
    }
  } else if (engineData.engine_state == EngineState::CLEARING_DATA) {
    statusText = l10n_util::GetNSString(IDS_VIVALDI_SYNC_CLEARING_SERVER_DATA);
    color = kSyncStatusBlueBackgroundColor;
  } else if (engineData.engine_state == EngineState::CONFIGURATION_PENDING) {
    statusText = l10n_util::GetNSString(IDS_SYNC_ALL_SET_TEXT);
    color = kSyncStatusBlueBackgroundColor;
  } else if (engineData.engine_state == EngineState::STARTED) {
    if (cycleData.download_updates_status == CycleStatus::SUCCESS &&
        cycleData.commit_status == CycleStatus::SUCCESS) {
      statusText = l10n_util::GetNSString(
          [self getCycleStatusMessageId:cycleData.commit_status]);
      color = [self getCycleStatusColor:cycleData.commit_status];
      lastSyncDate = cycleData.cycle_start_time.ToNSDate();
    } else {
      statusText = l10n_util::GetNSStringF(IDS_VIVALDI_SYNC_ERROR_STATUS,
         base::SysNSStringToUTF16(l10n_util::GetNSString(
            [self getCycleStatusMessageId:cycleData.download_updates_status])));
      color = [self getCycleStatusColor:cycleData.download_updates_status];
      if (!net::NetworkChangeNotifier::IsOffline() &&
          (cycleData.download_updates_status == CycleStatus::NETWORK_ERROR ||
          cycleData.download_updates_status == CycleStatus::SERVER_ERROR)) {
        [self shouldAddSyncStatusSectionFooter:YES];
      }
    }
  } else if (engineData.engine_state == EngineState::STARTING_SERVER_ERROR) {
    statusText = l10n_util::GetNSString(IDS_VIVALDI_SYNC_WAITING_SERVER_ERROR);
    color = kSyncStatusYellowBackgroundColor;
    if (!net::NetworkChangeNotifier::IsOffline()) {
      [self shouldAddSyncStatusSectionFooter:YES];
    }
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
      statusText = [statusText
          stringByAppendingString:
              @"Sync was stopped following a request from the server: "];
      if (engineData.protocol_error_type ==
          SyncProtocolErrorType::NOT_MY_BIRTHDAY) {
        statusText =
            [statusText stringByAppendingString:@"Sync server data was reset"];
      } else {
        statusText = [statusText
            stringByAppendingString:SysUTF8ToNSString(
                                        engineData.protocol_error_description)];
      }
    } else {
      statusText = [statusText
          stringByAppendingString:SysUTF8ToNSString(
                                      engineData.protocol_error_description)];
    }
  } else {
    NOTREACHED();
  }

  if (lastSyncDate) {
    self.syncStatusItem.lastSyncDateString = [NSString
        stringWithFormat:@"%@ %@",
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
    case CycleStatus::NOT_SYNCED:
      return IDS_VIVALDI_SYNC_NOT_SYNCED_CYCLE;
    case CycleStatus::SUCCESS:
      return IDS_VIVALDI_SYNC_SUCCESS;
    case CycleStatus::AUTH_ERROR:
      return IDS_VIVALDI_SYNC_AUTH_ERROR;
    case CycleStatus::SERVER_ERROR:
      return IDS_VIVALDI_SYNC_SERVER_ERROR;
    case CycleStatus::NETWORK_ERROR:
      return IDS_VIVALDI_SYNC_NETWORK_ERROR;
    case CycleStatus::CONFLICT:
      return IDS_VIVALDI_SYNC_CONFLICT;
    case CycleStatus::THROTTLED:
      return IDS_VIVALDI_SYNC_THROTTLED;
    default:
      return IDS_VIVALDI_SYNC_OTHER_ERROR;
  }
}

- (int)getCycleStatusColor:(int)status {
  switch (status) {
    case CycleStatus::SUCCESS:
      return kSyncStatusGreenBackgroundColor;
    case CycleStatus::NOT_SYNCED:
    case CycleStatus::NETWORK_ERROR:
    case CycleStatus::THROTTLED:
      return kSyncStatusYellowBackgroundColor;
    case CycleStatus::AUTH_ERROR:
    case CycleStatus::SERVER_ERROR:
    case CycleStatus::CONFLICT:
    default:
      return kSyncStatusRedBackgroundColor;
  }
}

- (void)reloadSyncStatusItem {
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

- (void)addUserInfoSection {
  TableViewModel* model = self.settingsConsumer.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierSyncUserInfo];
  VivaldiAccountManager::AccountInfo account_info =
      _vivaldiAccountManager->account_info();
  self.userInfoItem = [[VivaldiTableViewSyncUserInfoItem alloc]
      initWithType:ItemTypeSyncUserInfo];

  // Checking & Setting Donation Badges
  NSString* donationTierString = SysUTF8ToNSString(account_info.donation_tier);
  if (donationTierString != nil && ![donationTierString isEqualToString:@""]) {
    NSUInteger badgeTier = [donationTierString integerValue];
    [self setBadgeWithTier:(BadgeTier)badgeTier];
  }

  // Add a default avatar image
  self.userInfoItem.userAvatar =
      [UIImage systemImageNamed:@"person.circle.fill"];
  self.userInfoItem.userName = SysUTF8ToNSString(account_info.username);
  self.userInfoItem.sessionName = self.sessionName;
  [self.settingsConsumer.tableViewModel
                     setHeader:self.userInfoItem
      forSectionWithIdentifier:SectionIdentifierSyncUserInfo];

  __weak VivaldiSyncMediator* weakSelf = self;
  NSURL* profileImageURL =
      [NSURL URLWithString:SysUTF8ToNSString(account_info.picture_url)];
  [self fetchProfilePicture:profileImageURL
          completionHandler:^(NSData* data, NSURLResponse* response,
                              NSError* error) {
            if (error) {
              return;
            }

            dispatch_async(dispatch_get_main_queue(), ^{
              weakSelf.userInfoItem.userAvatar = [UIImage imageWithData:data];
              [weakSelf.settingsConsumer
                  reloadSection:SectionIdentifierSyncUserInfo];
            });
          }];
}

- (void)fetchProfilePicture:(NSURL*)url
          completionHandler:(ServerRequestCompletionHandler)handler {
  NSMutableURLRequest* request = [NSMutableURLRequest
       requestWithURL:url
          cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
      timeoutInterval:vConnectionTimeout];
  [request setHTTPMethod:@"GET"];
  [request setHTTPBody:nil];

  NSURLSession* session = [NSURLSession sharedSession];
  _task = [session dataTaskWithRequest:request completionHandler:handler];
  [_task resume];
}

- (void)addSyncStatusSection {
  TableViewModel* model = self.settingsConsumer.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierSyncStatus];
  self.syncStatusItem =
      [[VivaldiTableViewSyncStatusItem alloc] initWithType:ItemTypeSyncStatus];

  [self updateSyncStatusItem];
  [model addItem:self.syncStatusItem
      toSectionWithIdentifier:SectionIdentifierSyncStatus];
}

// The footer exists only when the sync cycle status returns either
// NETWORK_ERROR or SERVER_ERROR to allow users to check current status of Sync.
- (void)shouldAddSyncStatusSectionFooter:(BOOL)add {
  TableViewModel* model = self.settingsConsumer.tableViewModel;

  if (![model hasSectionForSectionIdentifier: SectionIdentifierSyncStatus]) {
    return;
  }

  TableViewLinkHeaderFooterItem* footer =
      [[TableViewLinkHeaderFooterItem alloc]
          initWithType:ItemTypeSyncStatusFooter];
  footer.forceIndents = YES;
  footer.text =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_ERROR_STATUS_DESCRIPTION);
  footer.urls = @[ [[CrURL alloc] initWithGURL:GURL(vVivaldiSyncStatusUrl)] ];

  if (add) {
    [model setFooter:footer
        forSectionWithIdentifier:SectionIdentifierSyncStatus];
  } else {
    [model setFooter:nil
        forSectionWithIdentifier:SectionIdentifierSyncStatus];
  }

  [self.settingsConsumer reloadSection:SectionIdentifierSyncStatus];
}

- (void)addSyncSwitchesSection {
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
    [self addItemsToUI:self.syncAllItems toSection:SectionIdentifierSyncItems];
  } else {
    [self addItemsToUI:self.syncSelectedItems
             toSection:SectionIdentifierSyncItems];
  }
}

- (void)addStartSyncingSection {
  TableViewModel* model = self.settingsConsumer.tableViewModel;
  self.startSyncingButton = [[VivaldiTableViewTextButtonItem alloc]
      initWithType:ItemTypeStartSyncingButton];
  self.startSyncingButton.buttonText =
      l10n_util::GetNSString(IDS_VIVALDI_START_SYNCING);
  self.startSyncingButton.textAlignment = NSTextAlignmentNatural;
  self.startSyncingButton.buttonBackgroundColor =
      [UIColor colorNamed:kBlueColor];
  self.startSyncingButton.buttonTextColor =
      [UIColor colorNamed:kSolidButtonTextColor];
  self.startSyncingButton.enabled = [self getStartSyncingButtonEnabled];

  [model addSectionWithIdentifier:SectionIdentifierSyncStartSyncing];
  [self updateStartSyncingSection];
}

- (void)addEncryptionSection {
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
      [[TableViewDetailTextItem alloc]
          initWithType:ItemTypeBackupRecoveryKeyButton];
  backupRecoveryKeyItem.text =
      l10n_util::GetNSString(IDS_VIVALDI_BACKUP_ENCRYPTION_KEY);
  backupRecoveryKeyItem.accessorySymbol =
      TableViewDetailTextCellAccessorySymbolExternalLink;
  [model addItem:backupRecoveryKeyItem
      toSectionWithIdentifier:SectionIdentifierSyncEncryption];
}

- (void)addSignOutSection {
  TableViewModel* model = self.settingsConsumer.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierSyncSignOut];
  self.logOutButton =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeLogOutButton];
  self.logOutButton.buttonText =
      l10n_util::GetNSString(IDS_VIVALDI_ACCOUNT_LOG_OUT);
  self.logOutButton.textAlignment = NSTextAlignmentNatural;
  self.logOutButton.buttonTextColor = [UIColor colorNamed:kBlueColor];
  self.logOutButton.buttonBackgroundColor = [UIColor clearColor];
  self.logOutButton.disableButtonIntrinsicWidth = YES;

  [model addItem:self.logOutButton
      toSectionWithIdentifier:SectionIdentifierSyncSignOut];
}

- (void)addDeleteDataSection {
  TableViewModel* model = self.settingsConsumer.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierSyncDeleteData];
  self.deleteDataButton =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeDeleteDataButton];
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
  for (TableViewItem* item : items) {
    [self.settingsConsumer.tableViewModel addItem:item
                          toSectionWithIdentifier:sectionIdentifier];
  }
}

- (void)removeItemsFromUI:(NSArray*)items {
  if (!self.settingsConsumer.tableViewModel || items.count == 0) {
    return;
  }
  NSMutableArray* indexPaths = [[NSMutableArray alloc] init];
  for (TableViewItem* item : items) {
    NSIndexPath* itemIndexPath =
        [self.settingsConsumer.tableViewModel indexPathForItem:item];
    if (itemIndexPath) {
      [indexPaths addObject:itemIndexPath];
    }
  }
  if (indexPaths.count == 0) {
    return;
  }
  LegacyChromeTableViewController* tableViewController =
      base::apple::ObjCCast<LegacyChromeTableViewController>(
          self.settingsConsumer);
  [tableViewController removeFromModelItemAtIndexPaths:indexPaths];
  [self.settingsConsumer.tableView
      deleteRowsAtIndexPaths:indexPaths
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

  BOOL hasButton = [self.settingsConsumer.tableViewModel
                      hasItem:self.startSyncingButton
      inSectionWithIdentifier:SectionIdentifierSyncStartSyncing];
  if (self.segmentedControlItem.selectedItem == SyncAll) {
    if (!hasButton) {
      [self.settingsConsumer.tableViewModel
                          addItem:self.startSyncingButton
          toSectionWithIdentifier:SectionIdentifierSyncStartSyncing];
    }
  } else {
    if (hasButton) {
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

  _syncAllItems = @[ syncAllInfoTextbox ];

  TableViewSwitchItem* switchItemBookmarks =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeSyncBookmarksSwitch];
  switchItemBookmarks.text = l10n_util::GetNSString(IDS_VIVALDI_SYNC_BOOKMARKS);
  switchItemBookmarks.detailText =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_BOOKMARKS_SUBTITLE);
  switchItemBookmarks.on = [_syncManager isSyncBookmarksEnabled];
  switchItemBookmarks.iconImage = [UIImage imageNamed:@"sync_bookmarks"];

  TableViewSwitchItem* switchItemSettings =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeSyncSettingsSwitch];
  switchItemSettings.text =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_PREFERENCES);
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
  switchItemReadingList.iconImage = [UIImage imageNamed:@"sync_readinglist"];

  TableViewSwitchItem* switchItemNotes =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeSyncNotesSwitch];
  switchItemNotes.text = l10n_util::GetNSString(IDS_VIVALDI_TOOLS_MENU_NOTES);
  switchItemNotes.detailText =
      l10n_util::GetNSString(IDS_VIVALDI_SYNC_NOTES_LIST_SUBTITLE);
  switchItemNotes.on = [_syncManager isSyncNotesEnabled];
  switchItemNotes.iconImage = [UIImage imageNamed:@"sync_notes"];

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
    BOOL syncStatus = [self syncStatusForItemType:item.type];
    item.on = syncStatus;
  }
}

- (BOOL)syncStatusForItemType:(NSInteger)type {
  switch (type) {
    case ItemTypeSyncBookmarksSwitch:
      return [_syncManager isSyncBookmarksEnabled];
    case ItemTypeSyncSettingsSwitch:
      return [_syncManager isSyncSettingsEnabled];
    case ItemTypeSyncPasswordsSwitch:
      return [_syncManager isSyncPasswordsEnabled];
    case ItemTypeSyncAutofillSwitch:
      return [_syncManager isSyncAutofillEnabled];
    case ItemTypeSyncTabsSwitch:
      return [_syncManager isSyncTabsEnabled];
    case ItemTypeSyncHistorySwitch:
      return [_syncManager isSyncHistoryEnabled];
    case ItemTypeSyncReadingListSwitch:
      return [_syncManager isSyncReadingListEnabled];
    case ItemTypeSyncNotesSwitch:
      return [_syncManager isSyncNotesEnabled];
    default:
      return NO;
  }
}

- (NSInteger)getSimplifiedAccountState {
  if (!_syncManager)
    return LOGGED_OUT;

  return [_syncManager getCurrentAccountState];
}

#pragma mark Private - Create Account Server Request Handling
- (void)sendCreateAccountRequestToServer:(BOOL)wantsNewsletter
                       completionHandler:
                           (ServerRequestCompletionHandler)handler {
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
  std::optional<base::Value> readResult = NSDataToDict(data);
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
    _vivaldiAccountManager->Login(pendingRegistration.username,
                                  pendingRegistration.password, false);
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
    pendingRegistration.recoveryEmailAddress =
        *pr->FindString(kRecoveryEmailKey);
  }
  [self.commandHandler showActivateAccountView];
}

- (void)setPendingRegistration {
  std::string encrypted_password;
  // iOS uses the posix implementation, which is non-blocking.
  if (!OSCrypt::EncryptString(pendingRegistration.password,
                              &encrypted_password)) {
    return;
  }

  std::string encoded_password;
  base::Base64Encode(encrypted_password);
  base::Value pending_registration(base::Value::Type::DICT);

  pending_registration.GetDict().Set(kRecoveryEmailKey,
                                     pendingRegistration.recoveryEmailAddress);
  pending_registration.GetDict().Set(kUsernameKey,
                                     pendingRegistration.username);
  pending_registration.GetDict().Set(kPasswordKey, encoded_password);

  _prefService->Set(vivaldiprefs::kVivaldiAccountPendingRegistration,
                    pending_registration);
}

- (std::unique_ptr<base::Value::Dict>)getPendingRegistration {
  const base::Value& pref_value =
      _prefService->GetValue(vivaldiprefs::kVivaldiAccountPendingRegistration);

  const std::string* username = pref_value.GetDict().FindString(kUsernameKey);
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
  NSString* sessionName = name;

  // If sessionName is nil or empty, save default client name.
  if (name == nil || [name isEqualToString:@""]) {
    sessionName = [self localDeviceClientName];
  }

  _prefService->SetString(vivaldiprefs::kSyncSessionName,
                          SysNSStringToUTF8(sessionName));
}

- (NSString*)sessionName {
  NSString* sessionName = base::SysUTF8ToNSString(
      _prefService->GetString(vivaldiprefs::kSyncSessionName));

  // If sessionName is empty try checking if local device name is already
  // available. If thats also not available force calling the method to load
  // the name and update userInfoItem.
  if ([sessionName length] == 0) {
    if ([[self localDeviceClientName] length] > 0) {
      return [self localDeviceClientName];
    } else {
      [self geLocalDeviceClientName];
    }
  }

  return sessionName;
}

- (void)geLocalDeviceClientName {
  __weak __typeof(self) weakSelf = self;
  dispatch_async(dispatch_get_global_queue(
    DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),^{

    std::string deviceName = syncer::GetPersonalizableDeviceNameBlocking();
    NSString *deviceNameNSString = base::SysUTF8ToNSString(deviceName);

    dispatch_async(dispatch_get_main_queue(), ^{
      weakSelf.localDeviceClientName = deviceNameNSString;

      // Update only if needed
      if (weakSelf.userInfoItem &&
          [weakSelf.userInfoItem.sessionName length] == 0) {
        weakSelf.userInfoItem.sessionName = deviceNameNSString;
        [weakSelf updateUserInfoSection];
      }
    });
  });
}

#pragma mark Private - Donation Tier Methods

- (void)setBadgeWithTier:(BadgeTier)tier {
  UIImage* badgeImage = nil;
  switch (tier) {
    case BadgeTierNone:
      badgeImage = nil;
      break;
    case BadgeTierSupporter:
      badgeImage = [UIImage imageNamed:kVivaldiSupporterBadge];
      break;
    case BadgeTierPatron:
      badgeImage = [UIImage imageNamed:kVivaldiPatronBadge];
      break;
    case BadgeTierAdvocate:
      badgeImage = [UIImage imageNamed:kVivaldiAdvocateBadge];
      break;
    default:
      badgeImage = nil;
      break;
  }
  // Set donation support Badge to header model
  self.userInfoItem.supporterBadge = badgeImage;
}

@end
