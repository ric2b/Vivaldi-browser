// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/sync/manager/vivaldi_account_sync_manager_observer_bridge.h"

#import "ios/ui/settings/sync/manager/vivaldi_account_sync_manager_consumer.h"

VivaldiAccountSyncManagerObserverBridge::
  VivaldiAccountSyncManagerObserverBridge(
      id<VivaldiAccountSyncManagerConsumer> consumer,
      VivaldiAccountManager* account_manager,
      SyncService* sync_service)
: account_manager_(account_manager),
  sync_service_(sync_service),
  consumer_(consumer) {

  DCHECK(account_manager);
  DCHECK(sync_service);
  DCHECK(consumer);
  account_manager_->AddObserver(this);
  sync_service->AddObserver(this);
}

VivaldiAccountSyncManagerObserverBridge::
  ~VivaldiAccountSyncManagerObserverBridge() {
  account_manager_->RemoveObserver(this);
  sync_service_->RemoveObserver(this);
}

void VivaldiAccountSyncManagerObserverBridge::OnVivaldiAccountUpdated() {
  id<VivaldiAccountSyncManagerConsumer> consumer = consumer_;
  if (!consumer)
    return;
  [consumer_ onVivaldiAccountUpdated];
}

void VivaldiAccountSyncManagerObserverBridge::OnTokenFetchSucceeded() {
  id<VivaldiAccountSyncManagerConsumer> consumer = consumer_;
  if (!consumer)
    return;
  [consumer_ onTokenFetchSucceeded];
}

void VivaldiAccountSyncManagerObserverBridge::OnTokenFetchFailed() {
  id<VivaldiAccountSyncManagerConsumer> consumer = consumer_;
  if (!consumer)
    return;
  [consumer_ onTokenFetchFailed];
}

void VivaldiAccountSyncManagerObserverBridge::OnVivaldiAccountShutdown() {
  account_manager_->RemoveObserver(this);
}

void VivaldiAccountSyncManagerObserverBridge::
  OnStateChanged(SyncService* sync) {
  id<VivaldiAccountSyncManagerConsumer> consumer = consumer_;
  if (!consumer)
    return;
  [consumer_ onVivaldiSyncStateChanged];
}

void VivaldiAccountSyncManagerObserverBridge::
  OnSyncCycleCompleted(SyncService* sync) {
  id<VivaldiAccountSyncManagerConsumer> consumer = consumer_;
  if (!consumer)
    return;
  [consumer_ onVivaldiSyncCycleCompleted];
}

void VivaldiAccountSyncManagerObserverBridge::
  OnSyncShutdown(SyncService* sync) {
  sync_service_->RemoveObserver(this);
}
