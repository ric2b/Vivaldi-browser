// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_syncmanager.h"

#include <utility>

#include "app/vivaldi_apptools.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "content/public/browser/browser_thread.h"
#if !defined(OS_ANDROID)
#include "extensions/api/runtime/runtime_api.h"
#endif
#include "prefs/vivaldi_gen_pref_enums.h"
#include "prefs/vivaldi_gen_prefs.h"
#include "sync/vivaldi_invalidation_service.h"
#include "sync/vivaldi_profile_oauth2_token_service.h"
#include "sync/vivaldi_profile_oauth2_token_service_factory.h"
#include "sync/vivaldi_sync_urls.h"

using syncer::JsBackend;
using syncer::ModelType;
using syncer::ModelTypeSet;
using syncer::SyncCredentials;
using syncer::WeakHandle;

namespace vivaldi {

VivaldiSyncManager::VivaldiSyncManager(
    ProfileSyncService::InitParams* init_params,
    std::shared_ptr<VivaldiInvalidationService> invalidation_service)
    : ProfileSyncService(std::move(*init_params)),
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

void VivaldiSyncManager::ClearSyncData() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!engine_)
    return;

  engine_->StartConfiguration();
  engine_->ClearServerData(
      base::Bind(&VivaldiSyncManager::Logout, weak_factory_.GetWeakPtr()));
}

void VivaldiSyncManager::Logout() {
  // If the engine wasn't running, we need to clear the local data manually.
  if (!engine_)
    RequestStop(CLEAR_DATA);

  signin()->SignOut(signin_metrics::USER_CLICKED_SIGNOUT_SETTINGS,
                    signin_metrics::SignoutDelete::IGNORE_METRIC);
}

void VivaldiSyncManager::SetupComplete() {
  if (!IsFirstSetupComplete()) {
    SetFirstSetupComplete();
    sync_blocker_.reset();
  }
}

void VivaldiSyncManager::ConfigureTypes(bool sync_everything,
                                        syncer::ModelTypeSet chosen_types) {
  OnUserChoseDatatypes(sync_everything, chosen_types);
}

void VivaldiSyncManager::NotifyLoginDone() {
  for (auto& observer : vivaldi_observers_) {
    observer.OnLoginDone();
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

void VivaldiSyncManager::NotifyLogoutDone() {
  for (auto& observer : vivaldi_observers_) {
    observer.OnLogoutDone();
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
}

void VivaldiSyncManager::OnConfigureDone(
    const syncer::DataTypeManager::ConfigureResult& result) {
  if (IsFirstSetupComplete()) {
    // Extra paranoia, except for non-official builds where we might need
    // encyption off for debugging.
    if (!IsEncryptEverythingEnabled() && version_info::IsOfficialBuild()) {
      Logout();
      return;
    }
    ProfileSyncService::OnConfigureDone(result);
  }
}

void VivaldiSyncManager::VivaldiTokenSuccess() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&VivaldiSyncManager::VivaldiDoTokenSuccess,
                            weak_factory_.GetWeakPtr()));
}

void VivaldiSyncManager::VivaldiDoTokenSuccess() {
  if (!vivaldi_access_token_.empty())
    ProfileSyncService::OnGetTokenSuccess(nullptr, vivaldi_access_token_,
                                          expiration_time_);
  vivaldi_access_token_.clear();
}

SyncCredentials VivaldiSyncManager::GetCredentials() {
  if (!vivaldi::ForcedVivaldiRunning())
    access_token_ = vivaldi_access_token_;
  return ProfileSyncService::GetCredentials();
}

void VivaldiSyncManager::RequestAccessToken() {
  if (vivaldi::ForcedVivaldiRunning())
    ProfileSyncService::RequestAccessToken();
  else if (vivaldi_access_token_.empty())
    NotifyAccessTokenRequested();
}

void VivaldiSyncManager::ShutdownImpl(syncer::ShutdownReason reason) {
  if (reason == syncer::DISABLE_SYNC) {
    sync_client_->GetPrefService()->ClearPref(
        vivaldiprefs::kSyncIsUsingSeparateEncryptionPassword);
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&VivaldiSyncManager::NotifyLogoutDone,
                   weak_factory_.GetWeakPtr()));
  }
  ProfileSyncService::ShutdownImpl(reason);
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
  if (token.empty()
// TODO(jarle): Remove the !Android check when we have extensions running
// on Android.
#if !defined(OS_ANDROID)
    || !extensions::VivaldiRuntimeFeatures::IsEnabled(
            sync_client_->GetProfile(), "sync")
#endif
) {
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
    password_ = password;
    signin()->SetAuthenticatedAccountInfo(account_id, username);
  }

  if (!IsEngineInitialized()) {
    sync_blocker_ = GetSetupInProgressHandle();
    RequestStart();
  }

  if (!IsSyncActive()) {
    sync_startup_tracker_.reset(
        new SyncStartupTracker(sync_client_->GetProfile(), this));
  } else if (has_login_details) {
    NotifyLoginDone();
  }

  if (has_login_details) {
    // Avoid passing an implicit password here, so that we can detect later on
    // if the account password needs to be provided for decryption.
    GoogleSigninSucceeded(account_id, username);
  }

  token_service->UpdateCredentials(account_id, token);
}

bool VivaldiSyncManager::SetEncryptionPassword(const std::string& password) {
  if (!IsEngineInitialized()) {
    return false;
  }

  std::string password_used = password;
  bool separate_password = !password_used.empty();
  if (!separate_password) {
    password_used = password_;
  }
  password_.clear();

  bool result = false;

  if (IsPassphraseRequired()) {
    result = SetDecryptionPassphrase(password);
  } else if (!IsUsingSecondaryPassphrase()) {
    SetEncryptionPassphrase(password_used, ProfileSyncService::EXPLICIT);
    result = true;
  }

  if (result)
    sync_client_->GetPrefService()->SetInteger(
        vivaldiprefs::kSyncIsUsingSeparateEncryptionPassword,
        static_cast<int>(
            separate_password
                ? vivaldiprefs::SyncIsUsingSeparateEncryptionPasswordValues::AYE
                : vivaldiprefs::SyncIsUsingSeparateEncryptionPasswordValues::
                      NAY));

  return result;
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
  if (IsSyncActive()) {
    password_.clear();
    SetFirstSetupComplete();
  }

  if (IsPassphraseRequiredForDecryption()) {
    if (password_.empty() || !SetDecryptionPassphrase(password_))
      NotifyEncryptionPasswordRequested();
    else
      sync_client_->GetPrefService()->SetInteger(
          vivaldiprefs::kSyncIsUsingSeparateEncryptionPassword,
          static_cast<int>(
              vivaldiprefs::SyncIsUsingSeparateEncryptionPasswordValues::NAY));
    password_.clear();
  }

  NotifyLoginDone();

  if (IsFirstSetupComplete())
    sync_blocker_.reset();
}
}  // namespace vivaldi
