// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_sync_auth_manager.h"

#include <string>

#include "sync/vivaldi_syncmanager.h"

namespace vivaldi {

VivaldiSyncAuthManager::VivaldiSyncAuthManager(
    syncer::SyncPrefs* sync_prefs,
    identity::IdentityManager* identity_manager,
    const AccountStateChangedCallback& account_state_changed,
    const CredentialsChangedCallback& credentials_changed,
    const NotifyTokenRequestedCallback& notify_token_requested,
    const std::string& saved_username)
    : SyncAuthManager(sync_prefs,
                      identity_manager,
                      account_state_changed,
                      credentials_changed),
      notify_token_requested_(notify_token_requested) {
  account_info_.account_id = saved_username;
  account_info_.email = saved_username;
}

VivaldiSyncAuthManager::~VivaldiSyncAuthManager() {}

void VivaldiSyncAuthManager::RegisterForAuthNotifications() {}

AccountInfo VivaldiSyncAuthManager::GetAuthenticatedAccountInfo() const {
  return account_info_;
}

void VivaldiSyncAuthManager::ConnectionStatusChanged(
    syncer::ConnectionStatus status) {
  token_status_.connection_status_update_time = base::Time::Now();
  token_status_.connection_status = status;

  switch (status) {
    case syncer::CONNECTION_AUTH_ERROR:
      access_token_.clear();

      token_status_.token_request_time = base::Time::Now();
      token_status_.token_receive_time = base::Time();
      token_status_.next_token_request_time = base::Time();

      notify_token_requested_.Run();
      break;
    case syncer::CONNECTION_OK:
      last_auth_error_ = GoogleServiceAuthError::AuthErrorNone();
      break;
    case syncer::CONNECTION_SERVER_ERROR:
      last_auth_error_ =
          GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED);
      break;
    case syncer::CONNECTION_NOT_ATTEMPTED:
      // The connection status should never change to "not attempted".
      NOTREACHED();
      break;
  }
}

void VivaldiSyncAuthManager::SetLoginInfo(const std::string& username,
                                          const std::string& access_token) {
  auto error = GoogleServiceAuthError(GoogleServiceAuthError::NONE);
  access_token_ = access_token;
  token_status_.last_get_token_error = error;
  token_status_.token_receive_time = base::Time::Now();

  account_info_.account_id = username;
  account_info_.email = username;

  sync_prefs_->SetSyncAuthError(false);
  last_auth_error_ = GoogleServiceAuthError::AuthErrorNone();
  credentials_changed_callback_.Run();
}

void VivaldiSyncAuthManager::ResetLoginInfo() {
  access_token_ = "";
  account_info_ = AccountInfo();

  token_status_.token_request_time = base::Time();
  token_status_.token_receive_time = base::Time();
  token_status_.next_token_request_time = base::Time();
}

}  // namespace vivaldi
