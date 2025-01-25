// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_SETTINGS_SYNC_MANAGER_VIVALDI_ACCOUNT_SYNC_MANAGER_OBSERVER_BRIDGE_H_
#define IOS_UI_SETTINGS_SYNC_MANAGER_VIVALDI_ACCOUNT_SYNC_MANAGER_OBSERVER_BRIDGE_H_

#import "Foundation/Foundation.h"

#import "components/sync/base/command_line_switches.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service_observer.h"
#import "components/sync/service/sync_service.h"
#import "ios/ui/settings/sync/vivaldi_sync_mediator.h"
#import "sync/vivaldi_sync_service_impl.h"
#import "vivaldi_account/vivaldi_account_manager.h"

using vivaldi::VivaldiAccountManager;
using vivaldi::VivaldiSyncServiceImpl;
using syncer::SyncService;

@protocol VivaldiAccountSyncManagerConsumer;

class VivaldiAccountSyncManagerObserverBridge:
    public VivaldiAccountManager::Observer,
    public syncer::SyncServiceObserver {
public:
  explicit VivaldiAccountSyncManagerObserverBridge(
    id<VivaldiAccountSyncManagerConsumer> consumer,
                             VivaldiAccountManager* account_manager,
                             SyncService* sync_service);
  ~VivaldiAccountSyncManagerObserverBridge() override;

private:
  // Account manager
  void OnVivaldiAccountUpdated() override;
  void OnTokenFetchSucceeded() override;
  void OnTokenFetchFailed() override;
  void OnVivaldiAccountShutdown() override;

  // Sync service
  void OnStateChanged(SyncService* sync) override;
  void OnSyncCycleCompleted(SyncService* sync) override;
  void OnSyncShutdown(SyncService* sync) override;

  VivaldiAccountManager* account_manager_ = nullptr;
  SyncService* sync_service_ = nullptr;
  __weak id<VivaldiAccountSyncManagerConsumer> consumer_; // Weak, owns us
};


#endif  // IOS_UI_SETTINGS_SYNC_MANAGER_VIVALDI_ACCOUNT_SYNC_MANAGER_OBSERVER_BRIDGE_H_
