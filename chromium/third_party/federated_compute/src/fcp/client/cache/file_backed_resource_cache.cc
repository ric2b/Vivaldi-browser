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

#include "fcp/client/cache/file_backed_resource_cache.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>  // NOLINT(build/c++17)
#include <fstream>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <system_error>  // NOLINT
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/cleanup/cleanup.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "fcp/base/clock.h"
#include "fcp/base/monitoring.h"
#include "fcp/base/platform.h"
#include "fcp/base/time_util.h"
#include "fcp/client/cache/cache_manifest.pb.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/log_manager.h"
#include "protostore/file-storage.h"
#include "protostore/proto-data-store.h"

namespace fcp {
namespace client {
namespace cache {

constexpr char kCacheManifestFileName[] = "cache_manifest.pb";
constexpr char kParentDir[] = "fcp";
// Cached files will be saved in <cache directory>/fcp/cache.
constexpr char kCacheDir[] = "cache";

absl::StatusOr<CacheManifest> FileBackedResourceCache::ReadInternal() {
  absl::StatusOr<const CacheManifest*> data = pds_->Read();
  if (data.ok()) {
    return *data.value();
  }
  log_manager_.LogDiag(ProdDiagCode::RESOURCE_CACHE_MANIFEST_READ_FAILED);
  // Ignore the status from DeleteManifest() even if it's an error, and bubble
  // up the status from pds. We call DeleteManifest() here instead of
  // Initialize(), as Initialize() calls ReadInternal(), potentially causing
  // infinite recursion. This means that any resources that were tracked by the
  // deleted manifest will not be cleaned up until the next time Initialize() is
  // called.
  auto ignored_status = DeleteManifest();
  if (!ignored_status.ok()) {
    FCP_LOG(INFO) << "Failed to delete manifest: " << ignored_status;
  }
  return absl::InternalError(
      absl::StrCat("Failed to read from database, with error message: ",
                   data.status().message()));
}

absl::Status FileBackedResourceCache::WriteInternal(
    std::unique_ptr<CacheManifest> manifest) {
  absl::Status status = pds_->Write(std::move(manifest));
  if (!status.ok()) {
    log_manager_.LogDiag(ProdDiagCode::RESOURCE_CACHE_MANIFEST_WRITE_FAILED);
    // Ignore the status returned by DeleteManifest even if it's an error and
    // instead return the status from pds. We call DeleteManifest() here instead
    // of Initialize(), as Initialize() calls WriteInternal(), potentially
    // causing infinite recursion. This means that any resources that were
    // tracked by the deleted manifest will not be cleaned up until the next
    // time Initialize() is called.
    auto ignored_status = DeleteManifest();
    if (!ignored_status.ok()) {
      FCP_LOG(INFO) << "Failed to delete manifest: " << ignored_status;
    }
  }
  return status;
}

absl::StatusOr<std::unique_ptr<FileBackedResourceCache>>
FileBackedResourceCache::Create(absl::string_view base_dir,
                                absl::string_view cache_dir,
                                LogManager* log_manager, fcp::Clock* clock,
                                int64_t max_cache_size_bytes) {
  // Create <cache root>/fcp.
  // Unfortunately NDK's flavor of std::filesystem::path does not support using
  // absl::string_view.
  std::filesystem::path cache_root_path((std::string(cache_dir)));
  if (!cache_root_path.is_absolute()) {
    log_manager->LogDiag(
        ProdDiagCode::RESOURCE_CACHE_CACHE_ROOT_PATH_NOT_ABSOLUTE);
    return absl::InvalidArgumentError(
        absl::StrCat("The provided path: ", cache_dir,
                     " is invalid. The path must be absolute"));
  }
  std::filesystem::path cache_dir_path =
      cache_root_path / kParentDir / kCacheDir;
  std::error_code error;
  std::filesystem::create_directories(cache_dir_path, error);
  if (error.value() != 0) {
    log_manager->LogDiag(
        ProdDiagCode::RESOURCE_CACHE_FAILED_TO_CREATE_CACHE_DIR);
    return absl::InternalError(absl::StrCat(
        "Failed to create FileBackedResourceCache cache directory ",
        cache_dir_path.string()));
  }
  // Create <files root>/fcp/cache_manifest.pb.s
  std::filesystem::path manifest_path((std::string(base_dir)));
  if (!manifest_path.is_absolute()) {
    log_manager->LogDiag(ProdDiagCode::RESOURCE_CACHE_INVALID_MANIFEST_PATH);
    return absl::InvalidArgumentError(
        absl::StrCat("The provided path: ", manifest_path.string(),
                     " is invalid. The path must start with \"/\""));
  }
  manifest_path /= kParentDir;
  std::filesystem::create_directories(manifest_path, error);
  if (error.value() != 0) {
    log_manager->LogDiag(RESOURCE_CACHE_FAILED_TO_CREATE_MANIFEST_DIR);
    return absl::InternalError(
        absl::StrCat("Failed to create directory ", manifest_path.string()));
  }
  manifest_path /= kCacheManifestFileName;

  auto file_storage = std::make_unique<protostore::FileStorage>();
  auto pds = std::make_unique<protostore::ProtoDataStore<CacheManifest>>(
      *file_storage, manifest_path.string());
  std::unique_ptr<FileBackedResourceCache> resource_cache =
      absl::WrapUnique(new FileBackedResourceCache(
          std::move(pds), std::move(file_storage), cache_dir_path,
          manifest_path, log_manager, clock, max_cache_size_bytes));
  {
    absl::MutexLock lock(&resource_cache->mutex_);
    FCP_RETURN_IF_ERROR(resource_cache->Initialize());
  }

  return resource_cache;
}

absl::Status FileBackedResourceCache::Put(absl::string_view cache_id,
                                          const absl::Cord& resource,
                                          const google::protobuf::Any& metadata,
                                          absl::Duration max_age) {
  absl::MutexLock lock(&mutex_);

  if (resource.size() > max_cache_size_bytes_ / 2) {
    return absl::ResourceExhaustedError(absl::StrCat(cache_id, " too large"));
  }

  FCP_ASSIGN_OR_RETURN(CacheManifest manifest, ReadInternal());
  FCP_RETURN_IF_ERROR(CleanUp(resource.size(), manifest));

  std::string cache_id_str(cache_id);
  std::filesystem::path cached_file_path = cache_dir_path_ / cache_id_str;
  absl::Time now = clock_.Now();
  absl::Time expiry = now + max_age;
  CachedResource cached_resource;
  cached_resource.set_file_name(cache_id_str);
  *cached_resource.mutable_metadata() = metadata;
  *cached_resource.mutable_expiry_time() =
      TimeUtil::ConvertAbslToProtoTimestamp(expiry);
  *cached_resource.mutable_last_accessed_time() =
      TimeUtil::ConvertAbslToProtoTimestamp(now);

  // Write the manifest back to disk before we write the file.
  manifest.mutable_cache()->insert({cache_id_str, cached_resource});
  FCP_RETURN_IF_ERROR(
      WriteInternal(std::make_unique<CacheManifest>(std::move(manifest))));

  // Write file if it doesn't exist.
  std::error_code exists_error;
  bool cached_file_exists =
      std::filesystem::exists(cached_file_path, exists_error);
  if (exists_error.value() != 0) {
    log_manager_.LogDiag(
        ProdDiagCode::RESOURCE_CACHE_PUT_FAILED_TO_CHECK_IF_FILE_EXISTS);
    return absl::InternalError(absl::StrCat(
        "Failed to check if cached resource already exists with error code: ",
        exists_error.value()));
  }
  if (!cached_file_exists) {
    auto status = WriteCordToFile(cached_file_path.string(), resource);
    if (!status.ok()) {
      log_manager_.LogDiag(ProdDiagCode::RESOURCE_CACHE_RESOURCE_WRITE_FAILED);
      return status;
    }
  }

  return absl::OkStatus();
}

absl::StatusOr<FileBackedResourceCache::ResourceAndMetadata>
FileBackedResourceCache::Get(absl::string_view cache_id,
                             std::optional<absl::Duration> max_age) {
  // By default, set up a "CACHE_MISS" diag code to be logged when this method
  // exits.
  DebugDiagCode diag_code = DebugDiagCode::RESOURCE_CACHE_MISS;
  absl::Cleanup diag_code_logger = [this, &diag_code] {
    log_manager_.LogDiag(diag_code);
  };
  absl::MutexLock lock(&mutex_);
  FCP_ASSIGN_OR_RETURN(CacheManifest manifest, ReadInternal());

  std::string cache_id_str(cache_id);
  if (!manifest.cache().contains(cache_id_str)) {
    return absl::NotFoundError(absl::StrCat(cache_id, " not found"));
  }
  CachedResource cached_resource = manifest.cache().at(cache_id_str);
  std::filesystem::path cached_file_path = cache_dir_path_ / cache_id_str;
  google::protobuf::Any metadata = cached_resource.metadata();
  absl::Time now = clock_.Now();
  *cached_resource.mutable_last_accessed_time() =
      TimeUtil::ConvertAbslToProtoTimestamp(now);
  if (max_age.has_value()) {
    absl::Time expiry = now + max_age.value();
    *cached_resource.mutable_expiry_time() =
        TimeUtil::ConvertAbslToProtoTimestamp(expiry);
  }

  absl::StatusOr<absl::Cord> contents =
      ReadFileToCord(cached_file_path.string());
  if (!contents.ok()) {
    log_manager_.LogDiag(ProdDiagCode::RESOURCE_CACHE_RESOURCE_READ_FAILED);
    manifest.mutable_cache()->erase(cache_id_str);
    std::error_code error;
    std::filesystem::remove(cached_file_path, error);
    if (error.value() != 0) {
      return absl::InternalError(error.message());
    }
    // Treat as not found, the resource should be fetched again.
    return absl::NotFoundError(absl::StrCat(cache_id, " not found"));
  }

  manifest.mutable_cache()->erase(cache_id_str);
  manifest.mutable_cache()->insert({cache_id_str, cached_resource});

  absl::Status status =
      WriteInternal(std::make_unique<CacheManifest>(std::move(manifest)));
  if (!status.ok()) return status;

  // We've reached the end, this is a hit! The absl::Cleanup above has a
  // reference to diag_code, so we update it to CACHE_HIT here.
  diag_code = DebugDiagCode::RESOURCE_CACHE_HIT;
  return FileBackedResourceCache::ResourceAndMetadata{*contents, metadata};
}

absl::Status FileBackedResourceCache::Initialize() {
  absl::string_view errorInInitializePrefix = "Error in initialize: ";
  std::string pds_path = manifest_path_.string();
  if (!std::filesystem::exists(pds_path)) {
    std::ofstream ofs(pds_path);
  }
  absl::StatusOr<int64_t> file_size = storage_->GetFileSize(pds_path);
  if (!file_size.ok()) {
    log_manager_.LogDiag(
        ProdDiagCode::RESOURCE_CACHE_INIT_FAILED_TO_GET_MANIFEST_SIZE);
    return absl::InternalError(absl::StrCat(
        errorInInitializePrefix, "Failed to get file size of cache manifest: ",
        file_size.status().message()));
  }
  // Initialize db if it's not initialized.
  if (*file_size == 0) {
    auto status = WriteInternal(std::make_unique<CacheManifest>());
    if (!status.ok()) {
      log_manager_.LogDiag(
          ProdDiagCode::RESOURCE_CACHE_INIT_FAILED_TO_INITIALIZE_MANIFEST);
      return absl::InternalError(absl::StrCat(
          errorInInitializePrefix,
          "Failed to initialize cache manifest for the first time: ",
          status.message()));
    }
  }
  // Then run CleanUp. Even if our manifest was empty we still might have
  // stranded cache files to delete, i.e. in the case that the manifest was
  // deleted but the cache dir was not deleted.
  absl::StatusOr<CacheManifest> manifest = ReadInternal();
  if (!manifest.ok()) {
    return absl::InternalError(
        absl::StrCat(errorInInitializePrefix,
                     "Failed to read manifest: ", manifest.status().message()));
  }
  auto cleanup_status = CleanUp(std::nullopt, *manifest);
  if (!cleanup_status.ok()) {
    log_manager_.LogDiag(ProdDiagCode::RESOURCE_CACHE_INIT_FAILED_CLEANUP);
    return absl::InternalError(absl::StrCat(
        errorInInitializePrefix,
        "Failed to clean up resource cache: ", cleanup_status.message()));
  }
  auto write_status = WriteInternal(std::make_unique<CacheManifest>(*manifest));
  if (!write_status.ok()) {
    return absl::InternalError(absl::StrCat(
        errorInInitializePrefix,
        "Failed to write cleaned up resource cache: ", write_status.message()));
  }
  return absl::OkStatus();
}

absl::Status FileBackedResourceCache::CleanUp(
    std::optional<int64_t> reserved_space_bytes, CacheManifest& manifest) {
  // Expire any cached resources past their expiry.
  // Clean up any files that are not tracked in the manifest.
  // Clean up any manifest entries that point to nonexistent files.

  // In order to delete files we don't track in the CacheManifest (or that
  // became untracked due to a crash), fill cache_dir_files with every file in
  // the cache dir. We'll then remove any file not actively tracked in the cache
  // manifest.
  std::set<std::filesystem::path> cache_dir_files;

  // We don't have any subdirectories in the cache, so we can use a directory
  // iterator.
  std::error_code directory_error;
  auto cache_dir_iterator =
      std::filesystem::directory_iterator(cache_dir_path_, directory_error);
  if (directory_error.value() != 0) {
    log_manager_.LogDiag(
        ProdDiagCode::RESOURCE_CACHE_CLEANUP_FAILED_TO_ITERATE_OVER_CACHE_DIR);
    return absl::InternalError(absl::StrCat(
        "Error iterating over cache dir. Error code: ", directory_error.value(),
        " message: ", directory_error.message()));
  }
  for (auto& file : cache_dir_iterator) {
    cache_dir_files.insert(cache_dir_path_ / file);
  }

  int64_t max_allowed_size_bytes = max_cache_size_bytes_;
  max_allowed_size_bytes -= reserved_space_bytes.value_or(0);

  std::set<std::string> cache_ids_to_delete;
  absl::Time now = clock_.Now();
  for (const auto& [id, resource] : manifest.cache()) {
    absl::Time expiry =
        TimeUtil::ConvertProtoToAbslTime(resource.expiry_time());
    std::filesystem::path resource_file =
        cache_dir_path_ / resource.file_name();
    // It's possible that this manifest entry points at a file in the cache dir
    // that doesn't exist, i.e. due to a failed write. In this case, the entry
    // should be deleted as well. cache_dir_files should contain a scan of the
    // entire cache dir, so the file pointed at by this manifest entry should be
    // there.
    bool cached_resource_exists =
        cache_dir_files.find(resource_file) != cache_dir_files.end();
    if (expiry < now || !cached_resource_exists) {
      cache_ids_to_delete.insert(id);
    } else {
      cache_dir_files.erase(resource_file);
    }
  }

  // Then delete CacheManifest entries.
  for (const auto& cache_id : cache_ids_to_delete) {
    manifest.mutable_cache()->erase(cache_id);
  }

  // Then delete files.
  absl::Status filesystem_status = absl::OkStatus();
  for (const auto& file : cache_dir_files) {
    std::error_code remove_error;
    std::filesystem::remove(file, remove_error);
    // We intentionally loop through all files and attempt to remove as many as
    // we can, then return the first error we saw.
    if (remove_error.value() != 0 && filesystem_status.ok()) {
      log_manager_.LogDiag(
          ProdDiagCode::RESOURCE_CACHE_CLEANUP_FAILED_TO_DELETE_CACHED_FILE);
      filesystem_status = absl::InternalError(absl::StrCat(
          "Failed to delete file. Error code: ", remove_error.value(),
          ", message: ", remove_error.message()));
    }
  }

  FCP_RETURN_IF_ERROR(filesystem_status);

  // If we still exceed the allowed size of the cache, delete entries until
  // we're under the allowed size, sorted by least recently used.

  // Build up a list of (cache_id, least recently used timestamp) and compute
  // the total size of the cache.
  std::vector<std::pair<std::string, absl::Time>> cache_id_lru;
  cache_id_lru.reserve(manifest.cache().size());
  uintmax_t cache_dir_size = 0;

  for (const auto& [id, resource] : manifest.cache()) {
    cache_id_lru.emplace_back(std::make_pair(
        id, TimeUtil::ConvertProtoToAbslTime(resource.last_accessed_time())));
    std::filesystem::path resource_file =
        cache_dir_path_ / resource.file_name();
    // We calculate the sum of tracked files instead of taking the file_size()
    // of the cache directory, because the latter generally does not reflect the
    // total size of the sum of all the files inside a directory.
    std::error_code ignored_exists_error;
    if (!std::filesystem::exists(resource_file, ignored_exists_error)) {
      // We log that the manifest entry pointed at a file in the cache that
      // doesn't exist, but otherwise continue. The next time the cache is
      // initialized, the manifest entry will be cleaned up.
      log_manager_.LogDiag(
          ProdDiagCode::RESOURCE_CACHE_CLEANUP_FAILED_TO_GET_FILE_SIZE);
      continue;
    }
    std::error_code file_size_error;
    std::uintmax_t size =
        std::filesystem::file_size(resource_file, file_size_error);
    // Loop through as many as we can and if there's an error, return the first
    // error we saw.
    if (file_size_error.value() != 0) {
      log_manager_.LogDiag(
          ProdDiagCode::RESOURCE_CACHE_CLEANUP_FAILED_TO_GET_FILE_SIZE);
      if (filesystem_status.ok()) {
        filesystem_status = absl::InternalError(absl::StrCat(
            "Error getting file size. Error code: ", file_size_error.value(),
            ", message: ", file_size_error.message()));
      }
      // If the file exists, but we failed to get the file size for some reason,
      // try to delete it then continue.
      std::error_code ignored_remove_error;
      std::filesystem::remove(resource_file, ignored_remove_error);
    } else {
      cache_dir_size += size;
    }
  }

  FCP_RETURN_IF_ERROR(filesystem_status);

  // Then, if the cache is bigger than the allowed size, delete entries ordered
  // by least recently used until we're below the threshold.
  if (cache_dir_size > max_allowed_size_bytes) {
    std::sort(cache_id_lru.begin(), cache_id_lru.end(),
              [](std::pair<std::string, absl::Time> first,
                 std::pair<std::string, absl::Time> second) -> bool {
                // Sort by least recently used timestamp.
                return first.second < second.second;
              });
    for (auto const& [cache_id, timestamp] : cache_id_lru) {
      std::string id_to_remove = cache_id;
      std::filesystem::path file_to_remove =
          cache_dir_path_ / manifest.cache().at(id_to_remove).file_name();
      manifest.mutable_cache()->erase(id_to_remove);
      std::error_code remove_error;
      uintmax_t file_size =
          std::filesystem::file_size(file_to_remove, remove_error);
      if (remove_error.value() != 0 && filesystem_status.ok()) {
        log_manager_.LogDiag(
            ProdDiagCode::RESOURCE_CACHE_CLEANUP_FAILED_TO_GET_FILE_SIZE);
        filesystem_status = absl::InternalError(absl::StrCat(
            "Error getting file size. Error code: ", remove_error.value(),
            ", message: ", remove_error.message()));
      }
      std::filesystem::remove(file_to_remove, remove_error);
      if (remove_error.value() != 0 && filesystem_status.ok()) {
        log_manager_.LogDiag(
            ProdDiagCode::RESOURCE_CACHE_CLEANUP_FAILED_TO_GET_FILE_SIZE);
        filesystem_status = absl::InternalError(absl::StrCat(
            "Failed to delete file. Error code: ", remove_error.value(),
            ", message: ", remove_error.message()));
      }
      cache_dir_size -= file_size;
      if (cache_dir_size < max_allowed_size_bytes) break;
    }
  }

  FCP_RETURN_IF_ERROR(filesystem_status);

  return absl::OkStatus();
}

absl::Status FileBackedResourceCache::DeleteManifest() {
  if (std::filesystem::exists(manifest_path_)) {
    std::error_code error;
    std::filesystem::remove(manifest_path_, error);
    if (error.value() != 0) {
      log_manager_.LogDiag(
          ProdDiagCode::RESOURCE_CACHE_FAILED_TO_DELETE_MANIFEST);
      return absl::InternalError(
          absl::StrCat("Failed to delete manifest! error code: ", error.value(),
                       ", message: ", error.message()));
    }
  }
  return absl::OkStatus();
}

}  // namespace cache
}  // namespace client
}  // namespace fcp
