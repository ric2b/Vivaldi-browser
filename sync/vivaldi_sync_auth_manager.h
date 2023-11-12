// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_VIVALDI_SYNC_AUTH_MANAGER_H_
#define SYNC_VIVALDI_SYNC_AUTH_MANAGER_H_

#include <string>

#include "components/sync/service/sync_auth_manager.h"
#include "vivaldi_account/vivaldi_account_manager.h"

namespace vivaldi {

class VivaldiAccountManager;

class VivaldiSyncAuthManager : public syncer::SyncAuthManager,
                               public VivaldiAccountManager::Observer {
 public:
  using NotifyTokenRequestedCallback = base::RepeatingClosure;

  VivaldiSyncAuthManager(
      signin::IdentityManager* identity_manager,
      const AccountStateChangedCallback& account_state_changed,
      const CredentialsChangedCallback& credentials_changed,
      VivaldiAccountManager* account_manager);

  ~VivaldiSyncAuthManager() override;
  VivaldiSyncAuthManager(const VivaldiSyncAuthManager&) = delete;
  VivaldiSyncAuthManager& operator=(const VivaldiSyncAuthManager&) = delete;

  void RegisterForAuthNotifications() override;
  syncer::SyncTokenStatus GetSyncTokenStatus() const override;
  void ConnectionOpened() override;
  void ConnectionStatusChanged(syncer::ConnectionStatus status) override;
  void ConnectionClosed() override;

  // VivaldiAccountManager::Observer implementation
  void OnVivaldiAccountUpdated() override;
  void OnTokenFetchSucceeded() override;
  void OnTokenFetchFailed() override;
  void OnVivaldiAccountShutdown() override;

 private:
  const raw_ptr<VivaldiAccountManager> account_manager_;  // Not owning.
  bool registered_for_account_notifications_ = false;
};

}  // namespace vivaldi

#endif  // SYNC_VIVALDI_SYNC_AUTH_MANAGER_H_
