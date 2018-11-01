// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_syncmanager.h"

#include <utility>

#include "app/vivaldi_apptools.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/sync/base/invalidation_helper.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "content/public/browser/browser_thread.h"
#include "sync/vivaldi_invalidation_service.h"
#include "sync/vivaldi_profile_oauth2_token_service.h"
#include "sync/vivaldi_profile_oauth2_token_service_factory.h"
#include "sync/vivaldi_sync_urls.h"

using syncer::ModelType;
using syncer::ModelTypeSet;
using syncer::JsBackend;
using syncer::SyncCredentials;
using syncer::WeakHandle;

namespace {
// TODO(julienp): We need to switch away from polling and use notifications as
// our primary way of refreshing sync data. When that is done, we might still
// want to do some occasional polling, but it won't be on a fixed interval.
const int kPollingInterval = 5;  // minutes
}  // anonymous namespace

namespace vivaldi {

VivaldiSyncManager::VivaldiSyncManager(
    ProfileSyncService::InitParams* init_params,
    std::shared_ptr<VivaldiInvalidationService> invalidation_service)
    : ProfileSyncService(std::move(*init_params)),
      polling_posted_(false),
      invalidation_service_(invalidation_service),
      weak_factory_(this) {}

VivaldiSyncManager::~VivaldiSyncManager() {
  for (auto& observer : vivaldi_observers_) {
    observer.OnDeletingSyncManager();
  }
}

void VivaldiSyncManager::AddVivaldiObserver(
    VivaldiSyncManagerObserver* observer) {
  vivaldi_observers_.AddObserver(observer);
}

void VivaldiSyncManager::RemoveVivaldiObserver(
    VivaldiSyncManagerObserver* observer) {
  vivaldi_observers_.RemoveObserver(observer);
}

void VivaldiSyncManager::ClearSyncData(base::Closure callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  data_type_manager_->Stop();
  engine_->StartConfiguration();
  engine_->ClearServerData(base::Bind(&VivaldiSyncManager::OnSyncDataCleared,
                                      weak_factory_.GetWeakPtr(), callback));
}

void VivaldiSyncManager::OnSyncDataCleared(base::Closure callback) {
  Logout();
  callback.Run();
}

void VivaldiSyncManager::Logout() {
  signin()->SignOut(signin_metrics::USER_CLICKED_SIGNOUT_SETTINGS,
                    signin_metrics::SignoutDelete::IGNORE_METRIC);
  sync_client_->GetPrefService()->ClearPref(prefs::kGoogleServicesAccountId);
  sync_client_->GetPrefService()->ClearPref(prefs::kGoogleServicesUsername);
  sync_client_->GetPrefService()->ClearPref(
      prefs::kGoogleServicesUserAccountId);
  RequestStop(CLEAR_DATA);
}

void VivaldiSyncManager::ConfigureTypes(bool sync_everything,
                                        syncer::ModelTypeSet chosen_types) {
  OnUserChoseDatatypes(sync_everything, chosen_types);

  if (!IsFirstSetupComplete()) {
    SetFirstSetupComplete();
    sync_blocker_.reset();
  }

  NotifySyncConfigured();
}

void VivaldiSyncManager::StartPollingServer() {
  if (polling_posted_)
    return;

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&VivaldiSyncManager::PerformPollServer,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMinutes(kPollingInterval));

  polling_posted_ = true;
}

void VivaldiSyncManager::PerformPollServer() {
  polling_posted_ = false;

  PollServer();
  StartPollingServer();
}

void VivaldiSyncManager::PollServer() {
  if (engine_) {
    syncer::ObjectIdInvalidationMap invalidation_map =
        syncer::ObjectIdInvalidationMap::InvalidateAll(
            syncer::ModelTypeSetToObjectIdSet(syncer::ProtocolTypes()));
    invalidation_service_->PerformInvalidation(invalidation_map);
    NotifySyncStarted();
  }
}

void VivaldiSyncManager::NotifyLoginDone() {
  for (auto& observer : vivaldi_observers_) {
    observer.OnLoginDone();
  }
}

void VivaldiSyncManager::NotifySyncConfigured() {
  for (auto& observer : vivaldi_observers_) {
    observer.OnSyncConfigured();
  }
}

void VivaldiSyncManager::NotifySyncStarted() {
  for (auto& observer : vivaldi_observers_) {
    observer.OnBeginSyncing();
  }
}

void VivaldiSyncManager::NotifySyncCompleted() {
  for (auto& observer : vivaldi_observers_) {
    observer.OnEndSyncing();
  }
}

void VivaldiSyncManager::NotifySyncEngineInitFailed() {
  for (auto& observer : vivaldi_observers_) {
    observer.OnSyncEngineInitFailed();
  }
}

void VivaldiSyncManager::NotifyAccessTokenRequested() {
  for (auto& observer : vivaldi_observers_) {
    observer.OnAccessTokenRequested();
  }
}

void VivaldiSyncManager::NotifyEncryptionPasswordRequested() {
  for (auto& observer : vivaldi_observers_) {
    observer.OnEncryptionPasswordRequested();
  }
}

void VivaldiSyncManager::OnSyncCycleCompleted(
    const syncer::SyncCycleSnapshot& snapshot) {
  ProfileSyncService::OnSyncCycleCompleted(snapshot);
  NotifySyncCompleted();

  StartPollingServer();
}

void VivaldiSyncManager::VivaldiTokenSuccess() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&VivaldiSyncManager::VivaldiDoTokenSuccess,
                            weak_factory_.GetWeakPtr()));
}

void VivaldiSyncManager::VivaldiDoTokenSuccess() {
  ProfileSyncService::OnGetTokenSuccess(nullptr, vivaldi_access_token_,
                                        expiration_time_);
}

SyncCredentials VivaldiSyncManager::GetCredentials() {
  if (!vivaldi::ForcedVivaldiRunning())
    access_token_ = vivaldi_access_token_;
  return ProfileSyncService::GetCredentials();
}

void VivaldiSyncManager::RequestAccessToken() {
  if (!vivaldi::ForcedVivaldiRunning())
    NotifyAccessTokenRequested();
  else
    ProfileSyncService::RequestAccessToken();
}

bool VivaldiSyncManager::DisableNotifications() const {
  return !vivaldi::ForcedVivaldiRunning();
}

void VivaldiSyncManager::SetToken(bool has_login_details,
                                  std::string username,
                                  std::string password,
                                  std::string token,
                                  std::string expire,
                                  std::string account_id) {
  if (token.empty()) {
    Logout();
    return;
  }

  if (expire.empty()) {
    expiration_time_ = base::Time::Now() + base::TimeDelta::FromHours(1);
  } else {
    if (!base::Time::FromUTCString(expire.c_str(), &expiration_time_))
      expiration_time_ = base::Time::Now() + base::TimeDelta::FromHours(1);
  }

  vivaldi_access_token_ = token;

  VivaldiProfileOAuth2TokenService* token_service =
      VivaldiProfileOAuth2TokenServiceFactory::GetForProfile(
          sync_client_->GetProfile());
  token_service->SetConsumer(this);

  if (has_login_details) {
    signin()->SetAuthenticatedAccountInfo(account_id, username);
  }

  if (!IsEngineInitialized()) {
    sync_blocker_ = GetSetupInProgressHandle();
    RequestStart();
  }

  sync_startup_tracker_.reset(
      new SyncStartupTracker(sync_client_->GetProfile(), this));

  if (has_login_details) {
    GoogleSigninSucceeded(account_id, username, password);
  }

  token_service->UpdateCredentials(account_id, token);
}

bool VivaldiSyncManager::SetEncryptionPassword(const std::string& password) {
  if (!IsEngineInitialized()) {
    password_ = password;
    return true;
  }

  if (IsPassphraseRequired()) {
    return SetDecryptionPassphrase(password_);
  } else if (!IsUsingSecondaryPassphrase()) {
    SetEncryptionPassphrase(password_, ProfileSyncService::EXPLICIT);
    return true;
  }
  return false;
}

void VivaldiSyncManager::SyncStartupCompleted() {
  if (sync_blocker_) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&VivaldiSyncManager::SetupConfiguration,
                   weak_factory_.GetWeakPtr()));
  }
  sync_startup_tracker_.reset();
}

void VivaldiSyncManager::SyncStartupFailed() {
  NotifySyncEngineInitFailed();
}

void VivaldiSyncManager::SetupConfiguration() {
  EnableEncryptEverything();

  if (!password_.empty()) {
    if (!SetEncryptionPassword(password_) && IsPassphraseRequired())
      NotifyEncryptionPasswordRequested();
    password_.empty();
  } else {
    if (IsPassphraseRequired())
      NotifyEncryptionPasswordRequested();
  }

  NotifyLoginDone();

  if (IsSyncActive())
    SetFirstSetupComplete();

  if (IsFirstSetupComplete())
    sync_blocker_.reset();
}

}  // namespace vivaldi
