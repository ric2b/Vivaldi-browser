// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_VIVALDI_SYNC_AUTH_MANAGER_H_
#define SYNC_VIVALDI_SYNC_AUTH_MANAGER_H_

#include <string>

#include "components/browser_sync/sync_auth_manager.h"

namespace vivaldi {

class VivaldiSyncManager;

class VivaldiSyncAuthManager : public browser_sync::SyncAuthManager {
 public:
  using NotifyTokenRequestedCallback = base::RepeatingClosure;

  VivaldiSyncAuthManager(
      syncer::SyncPrefs* sync_prefs,
      identity::IdentityManager* identity_manager,
      const AccountStateChangedCallback& account_state_changed,
      const CredentialsChangedCallback& credentials_changed,
      const NotifyTokenRequestedCallback& notify_token_requested,
      const std::string& saved_username);

  ~VivaldiSyncAuthManager() override;

  void RegisterForAuthNotifications() override;
  AccountInfo GetAuthenticatedAccountInfo() const override;
  void ConnectionStatusChanged(syncer::ConnectionStatus status) override;

  void SetLoginInfo(const std::string& username,
                    const std::string& access_token);
  void ResetLoginInfo();

 private:
  const NotifyTokenRequestedCallback notify_token_requested_;

  AccountInfo account_info_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiSyncAuthManager);
};

}  // namespace vivaldi

#endif  // SYNC_VIVALDI_SYNC_AUTH_MANAGER_H_
