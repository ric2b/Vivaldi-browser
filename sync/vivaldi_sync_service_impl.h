// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_VIVALDI_SYNC_SERVICE_IMPL_H_
#define SYNC_VIVALDI_SYNC_SERVICE_IMPL_H_

#include <memory>
#include <string>

#include "base/observer_list.h"
#include "components/sync/service/sync_service_impl.h"
#include "net/http/http_response_headers.h"
#include "sync/vivaldi_sync_ui_helper.h"

namespace base {
class CommandLine;
}

namespace vivaldi {
class VivaldiInvalidationService;
class VivaldiSyncAuthManager;
class VivaldiAccountManager;

class VivaldiSyncServiceImpl : public syncer::SyncServiceImpl {
 public:
  // invalidation_service as parameter to work around possible effects of
  // immedatiate move of init_params
  VivaldiSyncServiceImpl(
      syncer::SyncServiceImpl::InitParams init_params,
      PrefService* prefs,
      VivaldiAccountManager* account_manager);
  ~VivaldiSyncServiceImpl() override;
  VivaldiSyncServiceImpl(const VivaldiSyncServiceImpl&) = delete;
  VivaldiSyncServiceImpl& operator=(const VivaldiSyncServiceImpl&) = delete;

  base::WeakPtr<VivaldiSyncServiceImpl> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void Initialize();

  void ClearSyncData();

  bool is_clearing_sync_data() { return is_clearing_sync_data_; }

  VivaldiSyncUIHelper* ui_helper() { return &ui_helper_; }

 private:
  friend class VivaldiSyncUIHelper;

  void ResetEngine(syncer::ShutdownReason reason,
                   ResetEngineReason reset_reason) override;
  void OnEngineInitialized(
      bool success,
      bool is_first_time_sync_configure) override;

  void OnClearDataComplete(scoped_refptr<net::HttpResponseHeaders> headers);

  void ResetEncryptionBootstrapToken(const std::string& token);

  bool force_local_data_reset_ = false;

  bool is_clearing_sync_data_ = false;
  std::unique_ptr<network::SimpleURLLoader> clear_data_url_loader_;

  VivaldiSyncUIHelper ui_helper_;

  base::WeakPtrFactory<VivaldiSyncServiceImpl> weak_factory_;
};

}  // namespace vivaldi

#endif  // SYNC_VIVALDI_SYNC_SERVICE_IMPL_H_
