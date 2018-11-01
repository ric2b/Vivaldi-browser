// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_sync_model.h"

#include <string>

#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "sync/vivaldi_sync_model_observer.h"
#include "sync/vivaldi_syncmanager.h"

using vivaldi::VivaldiSyncModelObserver;

VivaldiSyncModel::VivaldiSyncModel(vivaldi::VivaldiSyncManager* client)
    : observers_(base::ObserverList<
                 vivaldi::VivaldiSyncModelObserver>::NOTIFY_EXISTING_ONLY) {}

VivaldiSyncModel::~VivaldiSyncModel() {
  for (auto& observer : observers_)
    observer.SyncModelBeingDeleted(this);
}

void VivaldiSyncModel::newMessage(const std::string& param1,
                                  const std::string& param2) {
  for (auto& observer : observers_)
    observer.SyncOnMessage(param1, param2);
}

void VivaldiSyncModel::Shutdown() {}

void VivaldiSyncModel::AddObserver(
    vivaldi::VivaldiSyncModelObserver* observer) {
  observers_.AddObserver(observer);
}

void VivaldiSyncModel::RemoveObserver(
    vivaldi::VivaldiSyncModelObserver* observer) {
  observers_.RemoveObserver(observer);
}
