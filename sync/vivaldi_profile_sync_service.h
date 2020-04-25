// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_VIVALDI_PROFILE_SYNC_SERVICE_H_
#define SYNC_VIVALDI_PROFILE_SYNC_SERVICE_H_

#include <memory>
#include <string>

#include "base/observer_list.h"
#include "components/sync/driver/profile_sync_service.h"
#include "sync/vivaldi_sync_ui_helper.h"

class Profile;

namespace base {
class CommandLine;
}

namespace vivaldi {
class VivaldiInvalidationService;
class VivaldiSyncAuthManager;
class VivaldiAccountManager;

class VivaldiProfileSyncService : public syncer::ProfileSyncService {
 public:
  // invalidation_service as parameter to work around possible effects of
  // immedatiate move of init_params
  VivaldiProfileSyncService(
      syncer::ProfileSyncService::InitParams* init_params,
      Profile* profile,
      std::shared_ptr<VivaldiInvalidationService> invalidation_service,
      VivaldiAccountManager* account_manager);
  ~VivaldiProfileSyncService() override;

  base::WeakPtr<VivaldiProfileSyncService> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void ClearSyncData();

  VivaldiInvalidationService* invalidation_service() {
    return invalidation_service_.get();
  }

  bool is_clearing_sync_data() { return is_clearing_sync_data_; }

  VivaldiSyncUIHelper* ui_helper() { return &ui_helper_; }

 private:
  void ShutdownImpl(syncer::ShutdownReason reason) override;
  void StartSyncingWithServer() override;
  void OnEngineInitialized(
    syncer::ModelTypeSet initial_types,
    const syncer::WeakHandle<syncer::JsBackend>& js_backend,
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>& debug_info_listener,
    const std::string& cache_guid,
    const std::string& birthday,
    const std::string& bag_of_chips,
    bool success) override;

  void OnClearDataComplete(scoped_refptr<net::HttpResponseHeaders> headers);

  bool force_local_data_reset_ = false;

  bool is_clearing_sync_data_ = false;
  std::unique_ptr<network::SimpleURLLoader> clear_data_url_loader_;

  Profile* profile_;

  std::shared_ptr<VivaldiInvalidationService> invalidation_service_;
  VivaldiSyncUIHelper ui_helper_;

  base::WeakPtrFactory<VivaldiProfileSyncService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiProfileSyncService);
};

}  // namespace vivaldi

#endif  // SYNC_VIVALDI_PROFILE_SYNC_SERVICE_H_
