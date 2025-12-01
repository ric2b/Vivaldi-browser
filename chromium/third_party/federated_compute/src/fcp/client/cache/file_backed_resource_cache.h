/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FCP_CLIENT_CACHE_FILE_BACKED_RESOURCE_CACHE_H_
#define FCP_CLIENT_CACHE_FILE_BACKED_RESOURCE_CACHE_H_

#include <cstdint>
#include <filesystem>  // NOLINT(build/c++17)
#include <memory>
#include <optional>
#include <utility>

#include "google/protobuf/any.pb.h"
#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "fcp/base/clock.h"
#include "fcp/client/cache/cache_manifest.pb.h"
#include "fcp/client/cache/resource_cache.h"
#include "fcp/client/log_manager.h"
#include "protostore/file-storage.h"
#include "protostore/proto-data-store.h"

namespace fcp {
namespace client {
namespace cache {

/**
 * A FileBackedResourceCache is a ResourceCache implementation where each
 * resource payload is stored as an individual file in a directory, along with a
 * ProtoDataStore manifest that tracks each entry.
 *
 * FileBackedResourceCache is thread safe.
 */
class FileBackedResourceCache : public ResourceCache {
 public:
  // The CacheManifest will be created in
  // <base directory>/fcp/cache_manifest.pb.

  // Factory method to create FileBackedResourceCache. The provided cache dir is
  // the absolute path for storing cached files, and the provided base dir is
  // the absolute path for longer term storage. FileBackedResourceCache will
  // attempt to create subdirectories and files, so the directory must grant
  // read/write access.
  //
  // A FileBackedResourceCache will not store any resources larger than
  // `max_cache_size_bytes` / 2.
  //
  // Deletes any stored resources past expiry.
  static absl::StatusOr<std::unique_ptr<FileBackedResourceCache>> Create(
      absl::string_view base_dir, absl::string_view cache_dir,
      LogManager* log_manager, fcp::Clock* clock, int64_t max_cache_size_bytes);

  // Implementation of `ResourceCache::Put`.
  //
  // If storing `resource` pushes the size of the cache directory over
  // `max_cache_size_bytes`, entries with the oldest last_accessed_time will be
  // deleted until the directory is under `max_cache_size_bytes` Returns Ok on
  // success. On error, returns:
  // - INTERNAL - unexpected error.
  // - INVALID_ARGUMENT - if max_age is in the past.
  // - RESOURCE_EXHAUSTED - if resource bytes is bigger than
  //   `max_cache_size_bytes` / 2.
  absl::Status Put(absl::string_view cache_id, const absl::Cord& resource,
                   const google::protobuf::Any& metadata,
                   absl::Duration max_age) override ABSL_LOCKS_EXCLUDED(mutex_);

  // Implementation of `ResourceCache::Get`.
  absl::StatusOr<ResourceAndMetadata> Get(absl::string_view cache_id,
                                          std::optional<absl::Duration> max_age)
      override ABSL_LOCKS_EXCLUDED(mutex_);

  ~FileBackedResourceCache() override = default;

  // FileBackedResourceCache is neither copyable nor movable.
  FileBackedResourceCache(const FileBackedResourceCache&) = delete;
  FileBackedResourceCache& operator=(const FileBackedResourceCache&) = delete;

 private:
  FileBackedResourceCache(
      std::unique_ptr<protostore::ProtoDataStore<CacheManifest>> pds,
      std::unique_ptr<protostore::FileStorage> storage,
      std::filesystem::path cache_dir_path, std::filesystem::path manifest_path,
      LogManager* log_manager, Clock* clock, const int64_t max_cache_size_bytes)
      : storage_(std::move(storage)),
        pds_(std::move(pds)),
        cache_dir_path_(cache_dir_path),
        manifest_path_(manifest_path),
        log_manager_(*log_manager),
        clock_(*clock),
        max_cache_size_bytes_(max_cache_size_bytes) {}

  absl::StatusOr<CacheManifest> ReadInternal()
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  absl::Status WriteInternal(std::unique_ptr<CacheManifest> manifest)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Initializes the CacheManifest ProtoDataStore db if necessesary, then runs
  // CleanUp().
  absl::Status Initialize() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Deletes the cache manifest.
  absl::Status DeleteManifest() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // TTLs any cached resources stored past their expiry, then deletes any
  // stranded files without matching manifest entries, and any entries without
  // matching resource files. If `reserved_space_bytes` is set, cleans up
  // resources sorted by least recently used until the cache size is less than
  // `max_cache_size_bytes_ - reserved_space_bytes`.
  // This modifies the passed `manifest`.
  absl::Status CleanUp(std::optional<int64_t> reserved_space_bytes,
                       CacheManifest& manifest)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Unused, but must be kept alive for longer than pds_.
  std::unique_ptr<protostore::FileStorage> storage_;
  std::unique_ptr<protostore::ProtoDataStore<CacheManifest>> pds_
      ABSL_GUARDED_BY(mutex_);
  const std::filesystem::path cache_dir_path_;
  const std::filesystem::path manifest_path_;
  LogManager& log_manager_;
  Clock& clock_;
  const int64_t max_cache_size_bytes_;
  absl::Mutex mutex_;
};

// Used by the class and in tests only.
namespace internal {}  // namespace internal

}  // namespace cache
}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_CACHE_FILE_BACKED_RESOURCE_CACHE_H_
