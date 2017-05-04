// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef _SYNC_VIVALDI_SYNC_CLIENT_H
#define _SYNC_VIVALDI_SYNC_CLIENT_H

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
}

#endif  // _SYNC_VIVALDI_SYNC_CLIENT_H
