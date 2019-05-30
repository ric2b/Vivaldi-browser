// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_VIVALDI_SYNC_AUTH_MANAGER_H_
#define SYNC_VIVALDI_SYNC_AUTH_MANAGER_H_

#include <string>

#include "components/browser_sync/sync_auth_manager.h"
#include "components/signin/core/browser/account_info.h"
#include "vivaldi_account/vivaldi_account_manager.h"

namespace vivaldi {

class VivaldiAccountManager;

class VivaldiSyncAuthManager : public browser_sync::SyncAuthManager,
                               public VivaldiAccountManager::Observer {
 public:
  using NotifyTokenRequestedCallback = base::RepeatingClosure;

  VivaldiSyncAuthManager(
      identity::IdentityManager* identity_manager,
      const AccountStateChangedCallback& account_state_changed,
      const CredentialsChangedCallback& credentials_changed,
      VivaldiAccountManager* account_manager);

  ~VivaldiSyncAuthManager() override;

  void RegisterForAuthNotifications() override;
  syncer::SyncTokenStatus GetSyncTokenStatus() const override;
  void ConnectionStatusChanged(syncer::ConnectionStatus status) override;

  // VivaldiAccountManager::Observer implementation
  void OnVivaldiAccountUpdated() override;
  void OnTokenFetchSucceeded() override;
  void OnTokenFetchFailed() override;
  void OnVivaldiAccountShutdown() override;

 private:
  VivaldiAccountManager* account_manager_;  // Not owning.
  bool registered_for_account_notifications_ = false;

  DISALLOW_COPY_AND_ASSIGN(VivaldiSyncAuthManager);
};

}  // namespace vivaldi

#endif  // SYNC_VIVALDI_SYNC_AUTH_MANAGER_H_
