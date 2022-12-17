// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_VIVALDI_SYNC_CLIENT_H_
#define SYNC_VIVALDI_SYNC_CLIENT_H_

#include <memory>

#include "build/build_config.h"
#include "ios/chrome/browser/sync/ios_chrome_sync_client.h"

namespace vivaldi {
class VivaldiInvalidationService;

class VivaldiSyncClient : public IOSChromeSyncClient {
 public:
  explicit VivaldiSyncClient(ChromeBrowserState*);
  ~VivaldiSyncClient() override;

  invalidation::InvalidationService* GetInvalidationService() override;

 private:
  ChromeBrowserState* context_;
};
}  // namespace vivaldi

#endif  // SYNC_VIVALDI_SYNC_CLIENT_H_
