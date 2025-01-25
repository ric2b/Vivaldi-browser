// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_VIVALDI_SYNC_SERVICE_IMPL_H_
#define SYNC_VIVALDI_SYNC_SERVICE_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/sync/service/sync_service_impl.h"
#include "net/http/http_response_headers.h"

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
  VivaldiSyncServiceImpl(syncer::SyncServiceImpl::InitParams init_params,
                         PrefService* prefs,
                         VivaldiAccountManager* account_manager);
  ~VivaldiSyncServiceImpl() override;
  VivaldiSyncServiceImpl(const VivaldiSyncServiceImpl&) = delete;
  VivaldiSyncServiceImpl& operator=(const VivaldiSyncServiceImpl&) = delete;

  /*Implementing vivaldi methods of syncer::SyncService*/
  void ClearSyncData() override;
  bool is_clearing_sync_data() const override { return is_clearing_sync_data_; }
  std::string GetEncryptionBootstrapTokenForBackup() override;
  void ResetEncryptionBootstrapTokenFromBackup(
      const std::string& token) override;

 private:
  /*Overriding syncer::SyncServiceImpl*/
  void OnEngineInitialized(bool success,
                           bool is_first_time_sync_configure) override;

  void OnClearDataComplete(scoped_refptr<net::HttpResponseHeaders> headers);

  bool force_local_data_reset_ = false;
  bool is_clearing_sync_data_ = false;
  std::unique_ptr<network::SimpleURLLoader> clear_data_url_loader_;

  base::WeakPtrFactory<VivaldiSyncServiceImpl> weak_factory_;
};

}  // namespace vivaldi

#endif  // SYNC_VIVALDI_SYNC_SERVICE_IMPL_H_
