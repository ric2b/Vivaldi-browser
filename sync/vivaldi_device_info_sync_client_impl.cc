// Copyright (c) 2025 Vivaldi. All rights reserved.

#include "chrome/browser/sync/device_info_sync_client_impl.h"

#include "chrome/browser/profiles/profile.h"

#include "sync/file_sync/file_store.h"
#include "sync/file_sync/file_store_factory.h"

namespace browser_sync {

size_t DeviceInfoSyncClientImpl::VivaldiGetSyncedFileStorageSize() const {
  return SyncedFileStoreFactory::GetForBrowserContext(profile_)
      ->GetTotalStorageSize();
}

}  // namespace browser_sync
