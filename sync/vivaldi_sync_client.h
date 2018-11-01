// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_VIVALDI_SYNC_CLIENT_H_
#define SYNC_VIVALDI_SYNC_CLIENT_H_

#include <memory>

#include "chrome/browser/sync/chrome_sync_client.h"

namespace vivaldi {
class VivaldiInvalidationService;

class VivaldiSyncClient : public browser_sync::ChromeSyncClient {
 public:
  explicit VivaldiSyncClient(Profile*);
  ~VivaldiSyncClient() override;
  invalidation::InvalidationService* GetInvalidationService() override;
  std::shared_ptr<VivaldiInvalidationService> GetVivaldiInvalidationService() {
    return invalidation_service_;
  }

 private:
  std::shared_ptr<VivaldiInvalidationService> invalidation_service_;
};
}  // namespace vivaldi

#endif  // SYNC_VIVALDI_SYNC_CLIENT_H_
