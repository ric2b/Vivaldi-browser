// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_FILE_SYNC_FILE_STORE_STORAGE_H_
#define SYNC_FILE_SYNC_FILE_STORE_STORAGE_H_

#include "base/files/file_path.h"
#include "base/files/important_file_writer.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "sync/file_sync/file_data.h"

namespace base {
class SequencedTaskRunner;
}

namespace file_sync {
class SyncedFileStore;

class SyncedFileStoreStorage
    : public base::ImportantFileWriter::BackgroundDataSerializer {
 public:
  using FilesDataGetter = base::RepeatingCallback<const SyncedFilesData&()>;
  using LoadCallback = base::OnceCallback<void(SyncedFilesData)>;
  SyncedFileStoreStorage(
      FilesDataGetter files_data_getter,
      const base::FilePath& local_store_path,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner);
  ~SyncedFileStoreStorage() override;

  SyncedFileStoreStorage(const SyncedFileStoreStorage&) = delete;
  SyncedFileStoreStorage& operator=(const SyncedFileStoreStorage&) = delete;

  static void Load(const base::FilePath& local_store_path,
                   LoadCallback loading_done_callback);

  // Schedules saving the store metadata to disk.
  void ScheduleSave();

  // Notification the file store is going to be deleted. If there is
  // a pending save, it is saved immediately.
  void OnFileStoreDeleted();

  // ImportantFileWriter::BackgroundDataSerializer implementation.
  base::ImportantFileWriter::BackgroundDataProducerCallback
  GetSerializedDataProducerForBackgroundSequence() override;

 private:
  FilesDataGetter files_data_getter_;

  // Sequenced task runner where disk writes will be performed at.
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // Helper to write bookmark data safely.
  base::ImportantFileWriter writer_;

  // The state of the backup file creation which is created lazily just before
  // the first scheduled save.
  bool backup_triggered_ = false;
};
}  // namespace file_sync

#endif  // SYNC_FILE_SYNC_FILE_STORE_STORAGE_H_