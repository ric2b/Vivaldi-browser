// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved.

#ifndef SYNC_FILE_SYNC_FILE_DATA_H_
#define SYNC_FILE_SYNC_FILE_DATA_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "sync/file_sync/file_store.h"

namespace file_sync {
struct SyncedFileData {
  SyncedFileData();
  ~SyncedFileData();
  SyncedFileData(SyncedFileData&&);
  SyncedFileData& operator=(SyncedFileData&&);

  bool IsUnreferenced() const;
  void RunPendingCallbacks();

  std::map<syncer::DataType, std::set<base::Uuid>> local_references;
  std::map<syncer::DataType, std::set<std::string>> sync_references;
  std::string mimetype;
  bool has_content_locally = false;
  bool is_deleting = false;
  std::optional<std::vector<uint8_t>> content;
  std::vector<SyncedFileStore::GetFileCallback> pending_callbacks;
};

using SyncedFilesData = std::map<std::string, SyncedFileData>;
}  // namespace file_sync

#endif  // SYNC_FILE_SYNC_FILE_DATA_H_