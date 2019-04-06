// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_VIVALDI_SYNCMANAGER_H_
#define SYNC_VIVALDI_SYNCMANAGER_H_

#include <memory>
#include <string>

#include "base/observer_list.h"
#include "chrome/browser/sync/sync_startup_tracker.h"
#include "components/browser_sync/profile_sync_service.h"
#include "sync/vivaldi_sync_manager_observer.h"

namespace base {
class CommandLine;
};

using browser_sync::ProfileSyncService;

namespace vivaldi {
class VivaldiInvalidationService;
class VivaldiSyncAuthManager;

class VivaldiSyncManager : public ProfileSyncService,
                           public SyncStartupTracker::Observer {
 public:
  // invalidation_service as parameter to work around possible effects of
  // immedatiate move of init_params
  VivaldiSyncManager(
      ProfileSyncService::InitParams* init_params,
      std::shared_ptr<VivaldiInvalidationService> invalidation_service);
  ~VivaldiSyncManager() override;

  base::WeakPtr<VivaldiSyncManager> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void AddVivaldiObserver(VivaldiSyncManagerObserver* observer);
  void RemoveVivaldiObserver(VivaldiSyncManagerObserver* observer);

  void SetToken(bool start_sync, std::string account_id, std::string token);
  bool SetEncryptionPassword(const std::string& password);

  void ClearSyncData();

  void Logout();
  void SetupComplete();
  void ConfigureTypes(bool sync_everything, syncer::ModelTypeSet chosen_types);
  VivaldiInvalidationService* invalidation_service() {
    return invalidation_service_.get();
  }

  void NotifyEngineStarted();
  void NotifySyncStarted();
  void NotifySyncCompleted();
  void NotifyEngineInitFailed();
  void NotifyEngineStopped();
  void NotifyAccessTokenRequested();
  void NotifyEncryptionPasswordRequested();

  static bool IsSyncEnabled() { return true; }

  void OnSyncCycleCompleted(const syncer::SyncCycleSnapshot& snapshot) override;
  void OnConfigureDone(
      const syncer::DataTypeManager::ConfigureResult& result) override;

  // SyncStartupTracker::Observer
  void SyncStartupCompleted() override;
  void SyncStartupFailed() override;

 private:
  void ShutdownImpl(syncer::ShutdownReason reason) override;

  void SetupConfiguration();

  std::unique_ptr<syncer::SyncSetupInProgressHandle> sync_blocker_;
  std::unique_ptr<SyncStartupTracker> sync_startup_tracker_;
  std::shared_ptr<VivaldiInvalidationService> invalidation_service_;

  // Avoid name collision with observers_ from the base class
  base::ObserverList<VivaldiSyncManagerObserver> vivaldi_observers_;

  VivaldiSyncAuthManager* vivaldi_sync_auth_manager_;

  base::WeakPtrFactory<VivaldiSyncManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiSyncManager);
};

}  // namespace vivaldi

#endif  // SYNC_VIVALDI_SYNCMANAGER_H_
