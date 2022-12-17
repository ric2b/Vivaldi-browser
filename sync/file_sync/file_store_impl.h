// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved.

#ifndef SYNC_FILE_SYNC_FILE_STORE_IMPL_H_
#define SYNC_FILE_SYNC_FILE_STORE_IMPL_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/base/model_type.h"
#include "sync/file_sync/file_data.h"
#include "sync/file_sync/file_store.h"
#include "sync/file_sync/file_store_storage.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace file_sync {
// The synced file store keeps track of files that must be made available for
// syncing when sync is active. It maintains a local copy of the file and
// handles uploading to and downloading from sync as needed. Files are
// identified by a SHA-256 checksum and their size, which is used to avoid
// duplicates both locally and on the sync server. Two files with the same hash
// and size would overwrite each other, but this isn't expected to happen in
// practice.
class SyncedFileStoreImpl : public SyncedFileStore {
 public:
  SyncedFileStoreImpl(base::FilePath profile_path);
  ~SyncedFileStoreImpl() override;

  SyncedFileStoreImpl(const SyncedFileStoreImpl&) = delete;
  SyncedFileStoreImpl& operator=(const SyncedFileStoreImpl&) = delete;

  void Load();

  // Implementing SyncedFileStore
  bool IsLoaded() override;
  void AddOnLoadedCallback(base::OnceClosure on_loaded_callback) override;
  void AddLocalFileRef(base::GUID owner_guid,
                       syncer::ModelType sync_type,
                       std::string checksum) override;
  std::string AddLocalFile(base::GUID owner_guid,
                           syncer::ModelType sync_type,
                           std::vector<uint8_t> content) override;
  void AddSyncFileRef(std::string owner_sync_id,
                      syncer::ModelType sync_type,
                      std::string checksum) override;
  void GetFile(std::string checksum, GetFileCallback callback) override;
  std::string GetMimeType(std::string checksum) override;
  void RemoveLocalRef(base::GUID owner_guid,
                      syncer::ModelType sync_type) override;
  void RemoveSyncRef(std::string owner_sync_id,
                     syncer::ModelType sync_type) override;
  void RemoveAllSyncRefsForType(syncer::ModelType sync_type) override;

 private:
  base::FilePath GetFilePath(const std::string& checksum) const;

  void OnReadContentDone(std::string checksum,
                         absl::optional<std::vector<uint8_t>> content);

  void OnLoadingDone(SyncedFilesData files_data);
  const SyncedFilesData& GetFilesData();

  void DeleteLocalContent(
      std::pair<const std::string, SyncedFileData>& file_data);
  void OnLocalContentDeleted(const std::string& checksum, bool success);

  base::FilePath local_store_path_;
  SyncedFilesData files_data_;

  std::vector<base::OnceClosure> on_loaded_callbacks_;

  std::map<syncer::ModelType, std::map<base::GUID, std::string>>
      checksums_for_local_owners_;
  std::map<syncer::ModelType, std::map<std::string, std::string>>
      checksums_for_sync_owners_;

  absl::optional<SyncedFileStoreStorage> storage_;

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  base::WeakPtrFactory<SyncedFileStoreImpl> weak_factory_{this};
};
}  // namespace file_sync

#endif  // SYNC_FILE_SYNC_FILE_STORE_IMPL_H_
