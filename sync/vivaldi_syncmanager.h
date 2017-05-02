// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef VIVALDI_SYNC_VIVALDI_SYNCMANAGER_H_
#define VIVALDI_SYNC_VIVALDI_SYNCMANAGER_H_

#include "chrome/browser/sync/sync_startup_tracker.h"
#include "components/browser_sync/profile_sync_service.h"

namespace base {
class CommandLine;
};

using browser_sync::ProfileSyncService;

class VivaldiSyncModel;

namespace vivaldi {
class VivaldiInvalidationService;

class VivaldiSyncManager : public ProfileSyncService,
                           public SyncStartupTracker::Observer {
 public:
  // invalidation_service as parameter to work around possible effects of
  // immedatiate move of init_params
  VivaldiSyncManager(
      ProfileSyncService::InitParams& init_params,
      std::shared_ptr<VivaldiInvalidationService> invalidation_service);
  ~VivaldiSyncManager() override;

  void HandleLoggedInMessage(const base::DictionaryValue& args);
  void HandleConfigureSyncMessage(const base::DictionaryValue& args);
  void HandleStartSyncMessage(const base::DictionaryValue& args);
  void HandleLogOutMessage(const base::DictionaryValue& args);
  void HandleRefreshToken(const base::DictionaryValue& args);
  void HandlePollServerMessage(const base::DictionaryValue& args);
  void HandleConfigurePollingMessage(const base::DictionaryValue& args);
  void SignalSyncEngineStarted();
  void SignalSyncConfigured();
  void SignalSyncStarted();
  void SignalSyncCompleted();

  void StartPollingServer();
  void PerformPollServer();

  static bool IsSyncEnabled() { return true; }

  void OnEngineInitialized(
      syncer::ModelTypeSet initial_types,
      const syncer::WeakHandle<syncer::JsBackend>& js_backend,
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
          debug_info_listener,
      const std::string& cache_guid,
      bool success) override;

  void OnSyncCycleCompleted(const syncer::SyncCycleSnapshot& snapshot) override;

  void VivaldiTokenSuccess();

  syncer::SyncCredentials GetCredentials() override;

  void Init(VivaldiSyncModel* model);
  void onNewMessage(const std::string& param1, const std::string& param2);

  void ChangePreferredDataTypes(syncer::ModelTypeSet preferred_types) override;

  // SyncStartupTracker::Observer
  void SyncStartupCompleted() override;
  void SyncStartupFailed() override;

  void OnPassphraseRequired(
      syncer::PassphraseRequiredReason reason,
      const sync_pb::EncryptedData& pending_keys) override;

 protected:
  bool DisableNotifications() const override;

 private:
  void SetToken(const base::DictionaryValue& args, bool full_login = false);
  void RequestAccessToken() override;
  void VivaldiDoTokenSuccess();
  void SetupConfiguration();

 private:
  // The profile whose data we are synchronizing.
  VivaldiSyncModel* model_;

  std::string vivaldi_access_token_;
  std::string password_;
  base::Time expiration_time_;

  base::TimeDelta polling_interval_ = base::TimeDelta::FromMinutes(5);

  bool polling_posted_;
  syncer::ModelTypeSet current_types_;
  std::unique_ptr<syncer::SyncSetupInProgressHandle> sync_blocker_;
  std::unique_ptr<SyncStartupTracker> sync_tracker_;
  std::shared_ptr<VivaldiInvalidationService> invalidation_service_;

  base::WeakPtrFactory<VivaldiSyncManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiSyncManager);
};

}  // namespace vivaldi

#endif  // VIVALDI_SYNC_VIVALDI_SYNCMANAGER_H_
