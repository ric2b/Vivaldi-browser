// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved.

#ifndef SYNC_FILE_SYNC_FILE_STORE_H_
#define SYNC_FILE_SYNC_FILE_STORE_H_

#include <set>
#include <string>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/uuid.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/base/data_type.h"

namespace file_sync {
// The synced file store keeps track of files that must be made available for
// syncing when sync is active. It maintains a local copy of the file and
// handles uploading to and downloading from sync as needed. Files are
// identified by a SHA-256 checksum and their size, which is used to avoid
// duplicates both locally and on the sync server. Two files with the same hash
// and size would overwrite each other, but this isn't expected to happen in
// practice. Each file is associated with one or more owners from a sync model.
// An owner cannot be referencing more than one file.
class SyncedFileStore : public KeyedService {
 public:
  using GetFileCallback = base::OnceCallback<void(
      std::optional<base::span<const uint8_t>> content)>;

  ~SyncedFileStore() override;

  // Returns whether the store has completed loading the file informations.
  virtual bool IsLoaded() = 0;

  // Adds a callback that will be invoked once loading is complete.
  virtual void AddOnLoadedCallback(base::OnceClosure on_loaded_callback) = 0;

  // Stores a reference to a file with a given |checksum| for a local syncable
  // entity of the given |sync_type|. This can be called whether or not sync is
  // active and whether or not the entity is synced.
  // If the owner already had a reference, it will be replaced.
  virtual void SetLocalFileRef(base::Uuid owner_uuid,
                               syncer::DataType sync_type,
                               std::string checksum) = 0;

  // Add a file for which the |content| is known and creates a reference to it
  // for a local syncable entity of the given |sync_type|. This can be called
  // whether or not sync is active and whether or not the entity is synced. This
  // returns the checksum that can be used to address the file in future calls.
  // If the owner already had a reference, it will be replaced.
  virtual std::string SetLocalFile(base::Uuid owner_uuid,
                                   syncer::DataType sync_type,
                                   std::vector<uint8_t> content) = 0;

  // Stores a reference to a file with a given |checksum| for a synced entity of
  // the given |sync_type|. This should be called only for entities that we know
  // have been committed to the sync server as calling this can trigger an
  // upload attempt. This also indicates that the content may be downloaded from
  // sync if it isn't available locally
  // If the owner already had a reference, it will be replaced.
  virtual void SetSyncFileRef(std::string owner_sync_id,
                              syncer::DataType sync_type,
                              std::string checksum) = 0;

  // Attempts to retrieve a file form the store. The |callback| may be invoked
  // immediately if the file is at hands or is known to be unavailable.
  // Otherwise, it will be called once the file has been obtained from disk, or
  // from the sync server.
  virtual void GetFile(std::string checksum, GetFileCallback callback) = 0;

  // Retrieves the mimetype of a file.
  virtual std::string GetMimeType(std::string checksum) = 0;

  // Indicates that an owner of a local reference to a file is gone. The
  // reference will be removed from the store. Note that a file might stick
  // around when it has no more local reference until all sync references are
  // gone to avoid needing to re-download the file if a conflict causes the
  // entity to be resurected.
  virtual void RemoveLocalRef(base::Uuid owner_uuid,
                              syncer::DataType sync_type) = 0;

  virtual void RemoveSyncRef(std::string owner_sync_id,
                             syncer::DataType sync_type) = 0;

  virtual void RemoveAllSyncRefsForType(syncer::DataType sync_type) = 0;

  // Gets the sum of the sizes of all files held by the store.
  virtual size_t GetTotalStorageSize() = 0;
};
}  // namespace file_sync

#endif  // SYNC_FILE_SYNC_FILE_STORE_H_
