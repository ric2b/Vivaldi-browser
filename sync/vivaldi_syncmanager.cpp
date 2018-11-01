// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_syncmanager.h"

#include <memory>
#include <string>
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
#include "sync/vivaldi_sync_model.h"
#include "sync/vivaldi_sync_urls.h"

using syncer::ModelType;
using syncer::ModelTypeSet;
using syncer::JsBackend;
using syncer::SyncCredentials;
using syncer::WeakHandle;

namespace vivaldi {

VivaldiSyncManager::VivaldiSyncManager(
    ProfileSyncService::InitParams* init_params,
    std::shared_ptr<VivaldiInvalidationService> invalidation_service)
    : ProfileSyncService(std::move(*init_params)),
      model_(NULL),
      polling_posted_(false),
      invalidation_service_(invalidation_service),
      weak_factory_(this) {}

VivaldiSyncManager::~VivaldiSyncManager() {
  if (model_) {
    model_ = NULL;
  }
}

void VivaldiSyncManager::Init(VivaldiSyncModel* model) {
  DCHECK(model);
  model_ = model;
}

void VivaldiSyncManager::HandleLoggedInMessage(
    const base::DictionaryValue& args) {
  CHECK(!args.empty());
  DCHECK(model_);

  SetToken(args, true);
}

void VivaldiSyncManager::HandleRefreshToken(const base::DictionaryValue& args) {
  CHECK(!args.empty());
  DCHECK(model_);

  SetToken(args);
}

void VivaldiSyncManager::HandleLogOutMessage(
    const base::DictionaryValue& args) {
  DCHECK(model_);

  signin()->SignOut(signin_metrics::USER_CLICKED_SIGNOUT_SETTINGS,
                    signin_metrics::SignoutDelete::IGNORE_METRIC);
  sync_client_->GetPrefService()->ClearPref(prefs::kGoogleServicesAccountId);
  sync_client_->GetPrefService()->ClearPref(prefs::kGoogleServicesUsername);
  sync_client_->GetPrefService()->ClearPref(
      prefs::kGoogleServicesUserAccountId);
  RequestStop(CLEAR_DATA);
}

void VivaldiSyncManager::HandleConfigureSyncMessage(
    const base::DictionaryValue& args) {
  DVLOG(1) << "Setting preferred types for non-blocking DTM"
           << ModelTypeSetToString(syncer::ProtocolTypes());

  SignalSyncConfigured();
}

void VivaldiSyncManager::HandleConfigurePollingMessage(
    const base::DictionaryValue& args) {
  std::string interval_seconds_str;
  if (!args.GetString("polling_interval_seconds", &interval_seconds_str))
    return;

  uint32_t interval_seconds;

  if (!base::StringToUint(interval_seconds_str, &interval_seconds))
    return;

  polling_interval_ = base::TimeDelta::FromSeconds(interval_seconds);
}

void VivaldiSyncManager::StartPollingServer() {
  if (polling_posted_)
    return;

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&VivaldiSyncManager::PerformPollServer,
                 weak_factory_.GetWeakPtr()),
      polling_interval_);

  polling_posted_ = true;
}

void VivaldiSyncManager::PerformPollServer() {
  polling_posted_ = false;

  base::DictionaryValue dummy;
  HandlePollServerMessage(dummy);
  StartPollingServer();
}

void VivaldiSyncManager::HandlePollServerMessage(
    const base::DictionaryValue& args) {
  CHECK(model_);
  if (engine_) {
    syncer::ObjectIdInvalidationMap invalidation_map =
        syncer::ObjectIdInvalidationMap::InvalidateAll(
            syncer::ModelTypeSetToObjectIdSet(syncer::ProtocolTypes()));
    invalidation_service_->PerformInvalidation(invalidation_map);
    // SignalSyncStarted();
  }
}

void VivaldiSyncManager::HandleStartSyncMessage(
    const base::DictionaryValue& args) {
  StartSyncingWithServer();
  SignalSyncStarted();
}

void VivaldiSyncManager::SignalSyncEngineStarted() {
  base::DictionaryValue dummy;
  HandleConfigureSyncMessage(dummy);
  onNewMessage(std::string("Starting Sync engine"), std::string());
}

void VivaldiSyncManager::SignalSyncConfigured() {
  base::DictionaryValue dummy;
  HandleStartSyncMessage(dummy);

  onNewMessage(std::string("Sync Initialized"), std::string());
}

void VivaldiSyncManager::SignalSyncStarted() {
  onNewMessage(std::string("SignalSyncStarted"),
               std::string("SignalSyncStarted"));
}

void VivaldiSyncManager::SignalSyncCompleted() {
  onNewMessage(std::string("SignalSyncCompleted"),
               std::string("SignalSyncCompleted"));
  current_types_ = GetActiveDataTypes();
  StartPollingServer();
}

void VivaldiSyncManager::OnSyncCycleCompleted(
    const syncer::SyncCycleSnapshot& snapshot) {
  ProfileSyncService::OnSyncCycleCompleted(snapshot);
  SignalSyncCompleted();
}

void VivaldiSyncManager::onNewMessage(const std::string& param1,
                                      const std::string& param2) {
  if (model_)
    model_->newMessage(param1, param2);
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
    onNewMessage(std::string("RequestAccessToken"), std::string());
  else
    ProfileSyncService::RequestAccessToken();
}

bool VivaldiSyncManager::DisableNotifications() const {
  return !vivaldi::ForcedVivaldiRunning();
}

void VivaldiSyncManager::SetToken(const base::DictionaryValue& args,
                                  bool full_login) {
  CHECK(!args.empty());

  std::string token;
  std::string account_id;
  std::string username;
  std::string password;
  std::string expire;
  if (!(args.GetString("token", &token) && args.GetString("expire", &expire) &&
        args.GetString("account_id", &account_id) &&
        (!full_login || (args.GetString("username", &username) &&
                         args.GetString("password", &password))))) {
    return;
  }

  if (token.empty()) {
    HandleLogOutMessage(args);
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
  sync_tracker_.reset(new SyncStartupTracker(sync_client_->GetProfile(), this));

  if (full_login) {
    signin()->SetAuthenticatedAccountInfo(account_id, username);
  }

  if (!IsEngineInitialized()) {
    sync_blocker_ = GetSetupInProgressHandle();
    RequestStart();
  }

  if (full_login) {
    password_ = password;
    GoogleSigninSucceeded(account_id, username, password);
  }

  token_service->UpdateCredentials(account_id, token);
}

void VivaldiSyncManager::ChangePreferredDataTypes(
    syncer::ModelTypeSet preferred_types) {
  ProfileSyncService::ChangePreferredDataTypes(preferred_types);
  current_types_ = GetActiveDataTypes();
}

void VivaldiSyncManager::OnEngineInitialized(
    syncer::ModelTypeSet initial_types,
    const syncer::WeakHandle<syncer::JsBackend>& js_backend,
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
        debug_info_listener,
    const std::string& cache_guid,
    bool success) {
  ProfileSyncService::OnEngineInitialized(
      initial_types, js_backend, debug_info_listener, cache_guid, success);
  if (!success) {
    onNewMessage(std::string("Sync Initialization Failed"), std::string());
    return;
  }

  SignalSyncEngineStarted();
}

void VivaldiSyncManager::SyncStartupCompleted() {
  if (sync_blocker_) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&VivaldiSyncManager::SetupConfiguration,
                   weak_factory_.GetWeakPtr()));
  }
  sync_tracker_.reset();
}

void VivaldiSyncManager::SyncStartupFailed() {}

void VivaldiSyncManager::SetupConfiguration() {
  OnUserChoseDatatypes(true, syncer::UserSelectableTypes());
  EnableEncryptEverything();
  if (IsPassphraseRequired()) {
    // TODO(yngve): ask for password again
    ignore_result(SetDecryptionPassphrase(password_));
  } else {
    if (!IsUsingSecondaryPassphrase())
      SetEncryptionPassphrase(password_, ProfileSyncService::EXPLICIT);
  }
  OnUserChoseDatatypes(true, syncer::UserSelectableTypes());
  sync_blocker_.reset();
  if (!IsFirstSetupComplete())
    SetFirstSetupComplete();

  sync_tracker_.reset(new SyncStartupTracker(sync_client_->GetProfile(), this));
}

}  // namespace vivaldi
