// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_MEDIATOR_H_
#define IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/ui/settings/sync/vivaldi_sync_settings_command_handler.h"
#import "ios/ui/settings/sync/vivaldi_sync_settings_consumer.h"
#import "ios/ui/settings/sync/vivaldi_sync_settings_view_controller_model_delegate.h"
#import "ios/ui/settings/sync/vivaldi_sync_settings_view_controller_service_delegate.h"

typedef NS_ENUM(NSInteger, SimplifiedState) {
  LOGGED_OUT = 0,
  LOGGING_IN,
  LOGGED_IN,
  CREDENTIALS_MISSING,
  LOGIN_FAILED,
  NOT_ACTIVATED
};

class VivaldiAccountManagerObserverBridge;
class VivaldiSyncServiceObserverBridge;
class PrefService;

namespace vivaldi {
  class VivaldiAccountManager;
  class VivaldiSyncServiceImpl;
}

namespace syncer {
  class SyncSetupInProgressHandle;
}

@interface VivaldiSyncMediator
    : NSObject <VivaldiSyncSettingsViewControllerModelDelegate,
                VivaldiSyncSettingsViewControllerServiceDelegate> {
  @private
  std::unique_ptr<VivaldiAccountManagerObserverBridge> _accountManagerObserver;
  std::unique_ptr<VivaldiSyncServiceObserverBridge> _syncObserver;
  std::unique_ptr<syncer::SyncSetupInProgressHandle> _syncSetupInProgressHandle;
}

- (instancetype)initWithAccountManager:
      (vivaldi::VivaldiAccountManager*)vivaldiAccountManager
      syncService:(vivaldi::VivaldiSyncServiceImpl*)syncService
      prefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (void)startMediating;
- (void)disconnect;

- (void)requestPendingRegistrationLogin;
- (NSString*)getPendingRegistrationUsername;
- (NSString*)getPendingRegistrationEmail;
- (void)clearPendingRegistration;

- (void)login:(std::string)username
        password:(std::string)password
        save_password:(BOOL)save_password;

- (void)setEncryptionPassword:(std::string)password;

- (void)storeUsername:(NSString*)username
                  age:(int)age
                email:(NSString*)recoveryEmailAddress;

- (void)createAccount:(NSString*)password
           deviceName:(NSString*)deviceName
      wantsNewsletter:(BOOL)wantsNewsletter;

@property(nonatomic, weak) id<VivaldiSyncSettingsCommandHandler> commandHandler;

@property(nonatomic, weak) id<VivaldiSyncSettingsConsumer> settingsConsumer;

@end

#endif  // IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_MEDIATOR_H_
