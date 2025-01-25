// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_sync_auth_manager.h"

#include <string>

#include "vivaldi_account/vivaldi_account_manager.h"

#if !BUILDFLAG(IS_IOS)
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
using content::GetUIThreadTaskRunner;
#endif

#if BUILDFLAG(IS_IOS)
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"

using web::GetUIThreadTaskRunner;
#endif

namespace vivaldi {

namespace {
const char kEmailSuffix[] = "@vivaldi.net";

GoogleServiceAuthError ToGoogleServiceAuthError(
    VivaldiAccountManager::FetchError error) {
  switch (error.type) {
    case VivaldiAccountManager::NONE:
      return GoogleServiceAuthError(GoogleServiceAuthError::NONE);
    case VivaldiAccountManager::NETWORK_ERROR:
      return GoogleServiceAuthError::FromConnectionError(error.error_code);
    case VivaldiAccountManager::SERVER_ERROR:
      return GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_ERROR);
    case VivaldiAccountManager::INVALID_CREDENTIALS:
      return GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER);
    default:
      NOTREACHED();
  }
}

syncer::SyncAccountInfo ToSyncAccountInfo(
    VivaldiAccountManager::AccountInfo account_info) {
  CoreAccountInfo chromium_account_info;
  // Email is the closest thing to a username that the chromium account info
  // takes. It isn't really used for anything else than disply purposes anyway.
  chromium_account_info.email = account_info.username + kEmailSuffix;
  chromium_account_info.gaia = account_info.username;
  chromium_account_info.account_id =
      CoreAccountId::FromString(account_info.account_id);

  return syncer::SyncAccountInfo(chromium_account_info, true);
}
}  // anonymous namespace

VivaldiSyncAuthManager::VivaldiSyncAuthManager(
    signin::IdentityManager* identity_manager,
    const AccountStateChangedCallback& account_state_changed,
    const CredentialsChangedCallback& credentials_changed,
    VivaldiAccountManager* account_manager)
    : SyncAuthManager(identity_manager,
                      account_state_changed,
                      credentials_changed),
      account_manager_(account_manager) {}

VivaldiSyncAuthManager::~VivaldiSyncAuthManager() {
  if (registered_for_account_notifications_)
    account_manager_->RemoveObserver(this);
}

void VivaldiSyncAuthManager::RegisterForAuthNotifications() {
  account_manager_->AddObserver(this);
  registered_for_account_notifications_ = true;

  sync_account_ = ToSyncAccountInfo(account_manager_->account_info());
}

syncer::SyncTokenStatus VivaldiSyncAuthManager::GetSyncTokenStatus() const {
  syncer::SyncTokenStatus token_status;
  token_status.connection_status_update_time =
      partial_token_status_.connection_status_update_time;
  token_status.connection_status = partial_token_status_.connection_status;
  token_status.token_request_time = account_manager_->GetTokenRequestTime();
  token_status.token_response_time = account_manager_->token_received_time();
  token_status.has_token = !account_manager_->access_token().empty();
  token_status.next_token_request_time =
      account_manager_->GetNextTokenRequestTime();
  token_status.last_get_token_error =
      ToGoogleServiceAuthError(account_manager_->last_token_fetch_error());

  return token_status;
}

void VivaldiSyncAuthManager::ConnectionOpened() {
  connection_open_ = true;
  if (account_manager_->has_refresh_token()) {
    access_token_ = account_manager_->access_token();
    GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                                 credentials_changed_callback_);
  }
}

void VivaldiSyncAuthManager::ConnectionStatusChanged(
    syncer::ConnectionStatus status) {
  partial_token_status_.connection_status_update_time = base::Time::Now();
  partial_token_status_.connection_status = status;

  switch (status) {
    case syncer::CONNECTION_AUTH_ERROR:
      access_token_.clear();
      account_manager_->RequestNewToken();
      break;
    case syncer::CONNECTION_OK:
      break;
    case syncer::CONNECTION_SERVER_ERROR:
      break;
    case syncer::CONNECTION_NOT_ATTEMPTED:
      // The connection status should never change to "not attempted".
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void VivaldiSyncAuthManager::ConnectionClosed() {
  connection_open_ = false;

  partial_token_status_ = syncer::SyncTokenStatus();
  ClearAccessTokenAndRequest();
}

void VivaldiSyncAuthManager::OnVivaldiAccountUpdated() {
  syncer::SyncAccountInfo new_account =
      ToSyncAccountInfo(account_manager_->account_info());
  if (new_account.account_info.account_id ==
      sync_account_.account_info.account_id)
    return;

  if (!sync_account_.account_info.account_id.empty()) {
    sync_account_ = syncer::SyncAccountInfo();
    ConnectionClosed();
    SetLastAuthError(GoogleServiceAuthError::AuthErrorNone());
    account_state_changed_callback_.Run();
  }

  if (!new_account.account_info.account_id.empty()) {
    sync_account_ = new_account;
    account_state_changed_callback_.Run();
  }
}

void VivaldiSyncAuthManager::OnTokenFetchSucceeded() {
  SetLastAuthError(GoogleServiceAuthError::AuthErrorNone());
  if (connection_open_) {
    access_token_ = account_manager_->access_token();
    credentials_changed_callback_.Run();
  }
}

void VivaldiSyncAuthManager::OnTokenFetchFailed() {
  if (account_manager_->last_token_fetch_error().type !=
      VivaldiAccountManager::INVALID_CREDENTIALS)
    return;

  SetLastAuthError(
      ToGoogleServiceAuthError(account_manager_->last_token_fetch_error()));
  credentials_changed_callback_.Run();
}

void VivaldiSyncAuthManager::OnVivaldiAccountShutdown() {}

}  // namespace vivaldi
