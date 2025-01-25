// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved.

#ifndef SYNC_FILE_SYNC_FILE_STORE_IMPL_H_
#define SYNC_FILE_SYNC_FILE_STORE_IMPL_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/uuid.h"
#include "components/sync/base/data_type.h"
#include "sync/file_sync/file_data.h"
#include "sync/file_sync/file_store.h"
#include "sync/file_sync/file_store_storage.h"

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
  void SetLocalFileRef(base::Uuid owner_uuid,
                       syncer::DataType sync_type,
                       std::string checksum) override;
  std::string SetLocalFile(base::Uuid owner_uuid,
                           syncer::DataType sync_type,
                           std::vector<uint8_t> content) override;
  void SetSyncFileRef(std::string owner_sync_id,
                      syncer::DataType sync_type,
                      std::string checksum) override;
  void GetFile(std::string checksum, GetFileCallback callback) override;
  std::string GetMimeType(std::string checksum) override;
  void RemoveLocalRef(base::Uuid owner_uuid,
                      syncer::DataType sync_type) override;
  void RemoveSyncRef(std::string owner_sync_id,
                     syncer::DataType sync_type) override;
  void RemoveAllSyncRefsForType(syncer::DataType sync_type) override;

  size_t GetTotalStorageSize() override;

 private:
  base::FilePath GetFilePath(const std::string& checksum) const;

  void DoSetLocalFileRef(base::Uuid owner_uuid,
                         syncer::DataType sync_type,
                         std::string checksum);

  void OnReadContentDone(std::string checksum,
                         std::optional<std::vector<uint8_t>> content);

  void OnLoadingDone(SyncedFilesData files_data);
  const SyncedFilesData& GetFilesData();

  void DeleteLocalContent(
      std::pair<const std::string, SyncedFileData>& file_data);
  void OnLocalContentDeleted(const std::string& checksum, bool success);

  base::FilePath local_store_path_;
  SyncedFilesData files_data_;

  std::vector<base::OnceClosure> on_loaded_callbacks_;

  std::map<syncer::DataType, std::map<base::Uuid, std::string>>
      checksums_for_local_owners_;
  std::map<syncer::DataType, std::map<std::string, std::string>>
      checksums_for_sync_owners_;

  std::optional<SyncedFileStoreStorage> storage_;

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  base::WeakPtrFactory<SyncedFileStoreImpl> weak_factory_{this};
};
}  // namespace file_sync

#endif  // SYNC_FILE_SYNC_FILE_STORE_IMPL_H_
