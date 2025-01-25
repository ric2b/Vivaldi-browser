// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_MEDIATOR_H_
#define IOS_UI_SETTINGS_SYNC_VIVALDI_SYNC_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/ui/settings/sync/vivaldi_sync_settings_command_handler.h"
#import "ios/ui/settings/sync/vivaldi_sync_settings_consumer.h"
#import "ios/ui/settings/sync/vivaldi_sync_settings_view_controller_model_delegate.h"
#import "ios/ui/settings/sync/vivaldi_sync_settings_view_controller_service_delegate.h"

class PrefService;

namespace vivaldi {
  class VivaldiAccountManager;
}

namespace syncer {
  class SyncService;
}

@interface VivaldiSyncMediator
    : NSObject <VivaldiSyncSettingsViewControllerModelDelegate,
                VivaldiSyncSettingsViewControllerServiceDelegate>

- (instancetype)initWithAccountManager:
      (vivaldi::VivaldiAccountManager*)vivaldiAccountManager
      syncService:(syncer::SyncService*)syncService
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
        deviceName:(NSString*)deviceName
      save_password:(BOOL)save_password;

- (void)updateDeviceName:(NSString*)deviceName;

- (BOOL)setEncryptionPassword:(std::string)password;
- (void)importEncryptionPassword:(NSURL*)file
          completionHandler:(void (^)(NSString* errorMessage))completionHandler;

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
