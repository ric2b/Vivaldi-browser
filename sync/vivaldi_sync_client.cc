// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_sync_client.h"

#include "sync/vivaldi_invalidation_service.h"

namespace vivaldi {
VivaldiSyncClient::VivaldiSyncClient(Profile* profile)
    : browser_sync::ChromeSyncClient(profile),
      invalidation_service_(new VivaldiInvalidationService(profile)) {}

VivaldiSyncClient::~VivaldiSyncClient() {}

invalidation::InvalidationService* VivaldiSyncClient::GetInvalidationService() {
  return invalidation_service_.get();
}

}  // namespace vivaldi
