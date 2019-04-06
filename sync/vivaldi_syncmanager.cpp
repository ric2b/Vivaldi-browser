// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_syncmanager.h"

#include <utility>

#include "app/vivaldi_apptools.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/browser_sync/sync_auth_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync/driver/signin_manager_wrapper.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "content/public/browser/browser_thread.h"
#if !defined(OS_ANDROID)
#include "extensions/api/runtime/runtime_api.h"
#endif
#include "prefs/vivaldi_gen_pref_enums.h"
#include "prefs/vivaldi_gen_prefs.h"
#include "sync/vivaldi_invalidation_service.h"
#include "sync/vivaldi_sync_auth_manager.h"
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
      weak_factory_(this) {
  auto vivaldi_sync_auth_manager = std::make_unique<VivaldiSyncAuthManager>(
      &sync_prefs_, signin_ ? signin_->GetIdentityManager() : nullptr,
      base::BindRepeating(&VivaldiSyncManager::AccountStateChanged,
                          base::Unretained(this)),
      base::BindRepeating(&VivaldiSyncManager::CredentialsChanged,
                          base::Unretained(this)),
      base::BindRepeating(&VivaldiSyncManager::NotifyAccessTokenRequested,
                          base::Unretained(this)),
      sync_client_->GetPrefService()->GetString(vivaldiprefs::kSyncUsername));
  vivaldi_sync_auth_manager_ = vivaldi_sync_auth_manager.get();
  auth_manager_.reset(vivaldi_sync_auth_manager.release());
}

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!engine_)
    return;

  engine_->StartConfiguration();
  engine_->ClearServerData(
      base::Bind(&VivaldiSyncManager::Logout, weak_factory_.GetWeakPtr()));
}

void VivaldiSyncManager::Logout() {
  RequestStop(CLEAR_DATA);

  vivaldi_sync_auth_manager_->ResetLoginInfo();
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

void VivaldiSyncManager::NotifyEngineStarted() {
  for (auto& observer : vivaldi_observers_) {
    observer.OnEngineStarted();
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

void VivaldiSyncManager::NotifyEngineInitFailed() {
  for (auto& observer : vivaldi_observers_) {
    observer.OnEngineInitFailed();
  }
}

void VivaldiSyncManager::NotifyEngineStopped() {
  for (auto& observer : vivaldi_observers_) {
    observer.OnEngineStopped();
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

void VivaldiSyncManager::ShutdownImpl(syncer::ShutdownReason reason) {
  if (reason == syncer::DISABLE_SYNC) {
    sync_client_->GetPrefService()->ClearPref(
        vivaldiprefs::kSyncIsUsingSeparateEncryptionPassword);
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&VivaldiSyncManager::NotifyEngineStopped,
                   weak_factory_.GetWeakPtr()));
  }
  ProfileSyncService::ShutdownImpl(reason);
}

void VivaldiSyncManager::SetToken(bool start_sync,
                                  std::string account_id,
                                  std::string token) {
  // This can only really happens when switching between sync servers and
  // using different accounts at the same time.
  std::string current_account =
      auth_manager_->GetAuthenticatedAccountInfo().account_id;
  if (!current_account.empty() && current_account != account_id) {
    Logout();
    return;
  }

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

  vivaldi_sync_auth_manager_->SetLoginInfo(account_id, token);

  if (!IsEngineInitialized()) {
    sync_blocker_ = GetSetupInProgressHandle();
    RequestStart();
  }

  if (!IsSyncActive()) {
    sync_startup_tracker_.reset(
        new SyncStartupTracker(sync_client_->GetProfile(), this));
  } else if (start_sync) {
    NotifyEngineStarted();
  }
}

bool VivaldiSyncManager::SetEncryptionPassword(const std::string& password) {
  if (!IsEngineInitialized()) {
    return false;
  }

  bool result = false;

  if (IsPassphraseRequired()) {
    result = SetDecryptionPassphrase(password);
  } else if (!IsUsingSecondaryPassphrase()) {
    SetEncryptionPassphrase(password, ProfileSyncService::EXPLICIT);
    result = true;
  }

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
  sync_startup_tracker_.reset();
  if (!IsSyncAllowed())
    Logout();
  NotifyEngineInitFailed();
}

void VivaldiSyncManager::SetupConfiguration() {
  if (IsSyncActive()) {
    SetFirstSetupComplete();
  }

  if (IsPassphraseRequiredForDecryption()) {
    NotifyEncryptionPasswordRequested();
  }

  NotifyEngineStarted();

  if (IsFirstSetupComplete())
    sync_blocker_.reset();
}
}  // namespace vivaldi
