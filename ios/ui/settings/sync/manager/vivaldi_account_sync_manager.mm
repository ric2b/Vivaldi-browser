// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/sync/manager/vivaldi_account_sync_manager.h"

#import <Foundation/Foundation.h>

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/ui/settings/sync/manager/vivaldi_account_sync_manager_consumer.h"
#import "ios/ui/settings/sync/manager/vivaldi_account_sync_manager_observer_bridge.h"
#import "ios/ui/settings/sync/manager/vivaldi_session_simplified_state.h"
#import "ios/vivaldi_account/vivaldi_account_manager_factory.h"
#import "sync/vivaldi_sync_service_impl.h"
#import "sync/vivaldi_sync_ui_helpers.h"
#import "vivaldi_account/vivaldi_account_manager.h"

using syncer::SyncService;
using syncer::UserSelectableType;
using syncer::UserSelectableTypeSet;
using vivaldi::VivaldiAccountManager;
using vivaldi::sync_ui_helpers::CycleData;
using vivaldi::sync_ui_helpers::CycleStatus;
using vivaldi::sync_ui_helpers::EngineData;
using vivaldi::sync_ui_helpers::EngineState;

namespace {
const std::string ERROR_ACCOUNT_NOT_ACTIVATED = "17006";
const double vConnectionTimeout = 60.0;
const double kAvatarSize = 30.0;
NSString* avatarPlaceholder = @"person.circle.fill";
}

@interface VivaldiAccountSyncManager () <VivaldiAccountSyncManagerConsumer> {
  std::unique_ptr<VivaldiAccountSyncManagerObserverBridge>
      _accountSyncManagerObserver;
  std::unique_ptr<syncer::SyncSetupInProgressHandle> _syncSetupInProgressHandle;
  EngineData engineData;
  CycleData cycleData;
}

@property(nonatomic, assign) Browser* browser;
@property(nonatomic, assign) ProfileIOS* profile;
@property(nonatomic, assign) syncer::SyncService* syncService;
@property(nonatomic, assign) VivaldiAccountManager* vivaldiAccountManager;
@property(nonatomic, copy) NSURLSessionDataTask* task;
@property(nonatomic, strong) UIImage* cachedAvatarImage;

@end

@implementation VivaldiAccountSyncManager
@synthesize consumer = _consumer;
@synthesize syncService = _syncService;
@synthesize vivaldiAccountManager = _vivaldiAccountManager;

#pragma mark - INITIALIZERS
- (instancetype)initWithBrowser:(Browser*)browser {
  if ((self =
        [self initWithProfile:browser->GetProfile()->GetOriginalProfile()])) {
    _browser = browser;
  }
  return self;
}

- (instancetype)initWithProfile:(ProfileIOS*)profile {
  if ((self = [super init])) {
    _profile = profile;

    _syncService = SyncServiceFactory::GetForProfile(_profile);
    _vivaldiAccountManager =
        vivaldi::VivaldiAccountManagerFactory::GetForProfile(_profile);

    _accountSyncManagerObserver.reset(
        new VivaldiAccountSyncManagerObserverBridge(
            self, _vivaldiAccountManager, _syncService));

    // Cache the account avatar at start
    [self cacheUserAvatar];
  }
  return self;
}

- (instancetype)initWithAccountManager:
                    (vivaldi::VivaldiAccountManager*)vivaldiAccountManager
                           syncService:
                               (syncer::SyncService*)syncService {
  if ((self = [super init])) {
    _vivaldiAccountManager = vivaldiAccountManager;
    _syncService = syncService;

    _accountSyncManagerObserver.reset(
        new VivaldiAccountSyncManagerObserverBridge(
            self, _vivaldiAccountManager, _syncService));

    // Cache the account avatar at start
    [self cacheUserAvatar];
  }
  return self;
}

#pragma mark - PUBLIC METHODS
- (void)start {
  [self notifyCurrentSessionState];
}

- (void)stop {
  self.consumer = nil;
  _accountSyncManagerObserver = nullptr;
}

#pragma mark - GETTERS
- (BOOL)hasSyncConsent {
  return [self getCurrentAccountState] == LOGGED_IN;
}

- (NSString*)accountUsername {
  if (![self hasSyncConsent])
    return nil;
  VivaldiAccountManager::AccountInfo account_info =
      _vivaldiAccountManager->account_info();
  return base::SysUTF8ToNSString(account_info.username);
}

- (UIImage*)accountUserAvatar {
  if (![self hasSyncConsent])
    return nil;

  UIImage* avatar = [UIImage systemImageNamed:avatarPlaceholder];

  if (self.cachedAvatarImage) {
    avatar = self.cachedAvatarImage;
  }

  return ResizeImage(avatar,
                  CGSize(kAvatarSize, kAvatarSize),
                  ProjectionMode::kAspectFit);
}

- (BOOL)isSyncBookmarksEnabled {
  return [self hasSyncConsent] &&
         engineData.data_types.Has(UserSelectableType::kBookmarks);
}

- (BOOL)isSyncSettingsEnabled {
  return [self hasSyncConsent] &&
         engineData.data_types.Has(UserSelectableType::kPreferences);
}

- (BOOL)isSyncPasswordsEnabled {
  return [self hasSyncConsent] &&
         engineData.data_types.Has(UserSelectableType::kPasswords);
}

- (BOOL)isSyncAutofillEnabled {
  return [self hasSyncConsent] &&
         engineData.data_types.Has(UserSelectableType::kAutofill);
}

- (BOOL)isSyncHistoryEnabled {
  return [self hasSyncConsent] &&
         engineData.data_types.Has(UserSelectableType::kHistory);
}

- (BOOL)isSyncTabsEnabled {
  return [self hasSyncConsent] &&
         engineData.data_types.Has(UserSelectableType::kTabs);
}

- (BOOL)isSyncReadingListEnabled {
  return [self hasSyncConsent] &&
         engineData.data_types.Has(UserSelectableType::kReadingList);
}

- (BOOL)isSyncNotesEnabled {
  return [self hasSyncConsent] &&
         engineData.data_types.Has(UserSelectableType::kNotes);
}

- (VivaldiAccountSimplifiedState)getCurrentAccountState {
  if (_vivaldiAccountManager->has_refresh_token()) {
    return LOGGED_IN;
  } else if (_vivaldiAccountManager->last_token_fetch_error()
                 .server_message.find(ERROR_ACCOUNT_NOT_ACTIVATED) !=
             std::string::npos) {
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

#pragma mark - SETTERS
- (void)enableTabsSync {
  if ([self isSyncTabsEnabled])
    return;

  [self updateSettingsType:UserSelectableType::kTabs isOn:YES];
}

- (void)enableAllSync {
  UserSelectableTypeSet selectedTypes;

  selectedTypes.Put(UserSelectableType::kBookmarks);
  selectedTypes.Put(UserSelectableType::kPasswords);
  selectedTypes.Put(UserSelectableType::kPreferences);
  selectedTypes.Put(UserSelectableType::kAutofill);
  selectedTypes.Put(UserSelectableType::kHistory);
  selectedTypes.Put(UserSelectableType::kTabs);
  selectedTypes.Put(UserSelectableType::kReadingList);
  selectedTypes.Put(UserSelectableType::kNotes);

  [self updateSettingsTypes:selectedTypes syncAll:YES];
}

- (void)updateSettingsType:(UserSelectableType)type isOn:(BOOL)isOn {
  UserSelectableTypeSet selectedTypes = engineData.data_types;
  if (isOn && !engineData.data_types.Has(type)) {
    selectedTypes.Put(type);
  } else if (!isOn && engineData.data_types.Has(type)) {
    selectedTypes.Remove(type);
  }

  [self updateSettingsTypes:selectedTypes syncAll:NO];
}

- (void)updateSettingsTypes:(UserSelectableTypeSet)types syncAll:(BOOL)syncAll {
  if (!_syncSetupInProgressHandle.get()) {
    _syncSetupInProgressHandle = _syncService->GetSetupInProgressHandle();
  }

  _syncService->GetUserSettings()->SetSelectedTypes(syncAll, types);
  _syncService->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);

  _syncSetupInProgressHandle.reset();
}

#pragma mark - PRIVATE

- (void)notifyCurrentSessionState {
  VivaldiSessionSimplifiedState sessionState = [self getCurrentSessionState];

  SEL selector = @selector(onVivaldiSessionUpdated:);
  if ([self.consumer respondsToSelector:selector]) {
    [self.consumer onVivaldiSessionUpdated:sessionState];
  }
}

- (VivaldiSyncSimplifiedState)getCurrentSyncStatusState {
  switch (engineData.engine_state) {
    case EngineState::FAILED:
      return SYNC_STATE_FAILED;
    case EngineState::CLEARING_DATA:
      return SYNC_STATE_CLEARING_DATA;
    case EngineState::CONFIGURATION_PENDING:
      return SYNC_STATE_CONFIG_PENDING;
    case EngineState::STARTED:
      if (cycleData.download_updates_status == CycleStatus::SUCCESS &&
          cycleData.commit_status == CycleStatus::SUCCESS) {
        return SYNC_STATE_STARTED_SUCCESS;
      } else {
        return SYNC_STATE_STARTED_ERROR;
      }
    case EngineState::STARTING_SERVER_ERROR:
      return SYNC_STATE_STARTING_SERVER_ERROR;
    case EngineState::STARTING:
      return SYNC_STATE_STARTING;
    case EngineState::STOPPED:
      return SYNC_STATE_STOPPED;
    default:
      return SYNC_STATE_DEFAULT;
  }
}

- (VivaldiSessionSimplifiedState)getCurrentSessionState {
  engineData = vivaldi::sync_ui_helpers::GetEngineData(_syncService);
  cycleData = vivaldi::sync_ui_helpers::GetCycleData(_syncService);

  switch ([self getCurrentAccountState]) {
    case LOGGING_IN:
      return VivaldiSessionSimplifiedState::LOGGING_IN;
    case LOGGED_IN: {
      switch (engineData.engine_state) {
        case EngineState::STARTED:
          if (cycleData.download_updates_status == CycleStatus::SUCCESS &&
              cycleData.commit_status == CycleStatus::SUCCESS) {
            if (![self isSyncTabsEnabled]) {
              return VivaldiSessionSimplifiedState::
                  LOGGED_IN_SYNC_SUCCESS_NO_TABS;
            } else {
              return VivaldiSessionSimplifiedState::LOGGED_IN_SYNC_SUCCESS;
            }
          } else {
            return VivaldiSessionSimplifiedState::LOGGED_IN_SYNC_OFF;
          }
        default:
          return VivaldiSessionSimplifiedState::LOGGED_IN_SYNC_STOPPED;
      }
    }
    case LOGGED_OUT:
      return VivaldiSessionSimplifiedState::LOGGED_OUT;
    case NOT_ACTIVATED:
      return VivaldiSessionSimplifiedState::NOT_ACTIVATED;
    case LOGIN_FAILED:
      return VivaldiSessionSimplifiedState::LOGIN_FAILED;
    default:
      return VivaldiSessionSimplifiedState::LOGGED_OUT;
  }
}

- (void)cacheUserAvatar {
  VivaldiAccountManager::AccountInfo account_info =
  _vivaldiAccountManager->account_info();
  NSURL* avatarURL =
      [NSURL URLWithString:base::SysUTF8ToNSString(account_info.picture_url)];

  __weak __typeof(self) weakSelf = self;
  [self fetchAccountAvatar:avatarURL
         completionHandler:^(NSData* data, NSURLResponse* response,
                              NSError* error) {
    if (error) {
      return;
    }

    dispatch_async(dispatch_get_main_queue(), ^{
      weakSelf.cachedAvatarImage = [UIImage imageWithData:data];
    });
  }];
}

- (void)fetchAccountAvatar:(NSURL*)url
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

#pragma mark : - OBSERVERS
#pragma mark : - VivaldiAccountSyncManagerConsumer
- (void)onVivaldiAccountUpdated {
  [self notifyCurrentSessionState];
  SEL selector = @selector(onVivaldiAccountUpdated);
  if ([self.consumer respondsToSelector:selector]) {
    [self.consumer onVivaldiAccountUpdated];
  }
}

- (void)onTokenFetchSucceeded {
  [self notifyCurrentSessionState];
  SEL selector = @selector(onTokenFetchSucceeded);
  if ([self.consumer respondsToSelector:selector]) {
    [self.consumer onTokenFetchSucceeded];
  }
}

- (void)onTokenFetchFailed {
  [self notifyCurrentSessionState];
  SEL selector = @selector(onTokenFetchFailed);
  if ([self.consumer respondsToSelector:selector]) {
    [self.consumer onTokenFetchFailed];
  }
}

- (void)onVivaldiSyncStateChanged {
  [self notifyCurrentSessionState];
  SEL selector = @selector(onVivaldiSyncStateChanged);
  if ([self.consumer respondsToSelector:selector]) {
    [self.consumer onVivaldiSyncStateChanged];
  }
}

- (void)onVivaldiSyncCycleCompleted {
  [self notifyCurrentSessionState];
  SEL selector = @selector(onVivaldiSyncCycleCompleted);
  if ([self.consumer respondsToSelector:selector]) {
    [self.consumer onVivaldiSyncCycleCompleted];
  }
}

@end
