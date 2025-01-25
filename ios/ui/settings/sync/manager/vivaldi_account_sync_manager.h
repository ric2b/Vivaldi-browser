// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SYNC_MANAGER_VIVALDI_ACCOUNT_SYNC_MANAGER_H_
#define IOS_UI_SETTINGS_SYNC_MANAGER_VIVALDI_ACCOUNT_SYNC_MANAGER_H_

#import "Foundation/Foundation.h"

#import "components/sync/base/user_selectable_type.h"
#import "ios/ui/settings/sync/manager/vivaldi_account_simplified_state.h"
#import "ios/ui/settings/sync/manager/vivaldi_sync_simplified_state.h"
#import "sync/vivaldi_sync_service_impl.h"
#import "vivaldi_account/vivaldi_account_manager.h"

class Browser;
class ProfileIOS;
@protocol VivaldiAccountSyncManagerConsumer;

using syncer::UserSelectableType;
using syncer::UserSelectableTypeSet;

typedef void (^ServerRequestCompletionHandler)
    (NSData* data, NSURLResponse* response, NSError* error);

// VivaldiAccountSyncManager handles the communication between UI
// and the Sync backend.
@interface VivaldiAccountSyncManager: NSObject

@property(nonatomic, weak)
    id<VivaldiAccountSyncManagerConsumer> consumer;

- (instancetype)initWithBrowser:(Browser*)browser;
- (instancetype)initWithProfile:(ProfileIOS*)profile;
- (instancetype)initWithAccountManager:
      (vivaldi::VivaldiAccountManager*)vivaldiAccountManager
      syncService:(syncer::SyncService*)syncService;
- (instancetype)init NS_UNAVAILABLE;

- (void)start;
- (void)stop;

#pragma mark - GETTERS
- (BOOL)hasSyncConsent;
- (NSString*)accountUsername;
- (UIImage*)accountUserAvatar;
- (BOOL)isSyncBookmarksEnabled;
- (BOOL)isSyncSettingsEnabled;
- (BOOL)isSyncPasswordsEnabled;
- (BOOL)isSyncAutofillEnabled;
- (BOOL)isSyncHistoryEnabled;
- (BOOL)isSyncReadingListEnabled;
- (BOOL)isSyncNotesEnabled;
- (BOOL)isSyncTabsEnabled;
- (VivaldiAccountSimplifiedState)getCurrentAccountState;

#pragma mark - SETTERS
- (void)enableTabsSync;
- (void)enableAllSync;
- (void)updateSettingsType:(UserSelectableType)type isOn:(BOOL)isOn;
- (void)updateSettingsTypes:(UserSelectableTypeSet)types syncAll:(BOOL)syncAll;

@end

#endif  // IOS_UI_SETTINGS_SYNC_MANAGER_VIVALDI_ACCOUNT_SYNC_MANAGER_H_
