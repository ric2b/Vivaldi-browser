// Copyright (c) 2025 Vivaldi. All rights reserved.

#include "components/sync_bookmarks/bookmark_sync_service.h"

namespace sync_bookmarks {

void BookmarkSyncService::SetVivaldiSyncedFileStore(
    file_sync::SyncedFileStore* synced_file_store) {
  bookmark_data_type_processor_.set_vivaldi_synced_file_store(
      synced_file_store);
}

}  // namespace sync_bookmarks
