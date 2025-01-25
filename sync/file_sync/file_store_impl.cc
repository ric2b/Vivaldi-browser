// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved.

#include "sync/file_sync/file_store_impl.h"

#include <algorithm>

#include "base/containers/span.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "components/base32/base32.h"
#include "components/datasource/resource_reader.h"
#include "crypto/sha2.h"
#include "net/base/mime_sniffer.h"

namespace file_sync {
namespace {
#if BUILDFLAG(IS_ANDROID)
constexpr char kunknownFile[] = "unknown_file.png";
constexpr char kMissingContent[] = "unsynced_file.png";
#else  // BUILDFLAG(IS_ANDROID)
constexpr char kunknownFile[] = "resources/unknown_file.png";
constexpr char kMissingContent[] = "resources/unsynced_file.png";
#endif  // BUILDFLAG(IS_ANDROID)
constexpr char kunknownFileFallback[] = "Unknown file.";
constexpr char kMissingContentFallback[] =
    "Placeholder for synced file. Removing this will remove the corresponding "
    "original file in the vivaldi instance that created this. Synchronization "
    "of the file content is not supported yet.";

constexpr base::FilePath::CharType kStoreDirectoryName[] =
    FILE_PATH_LITERAL("SyncedFiles");

struct Resources {
  std::vector<uint8_t> unknown_file;
  std::string unknown_file_mimetype;
  std::vector<uint8_t> missing_content;
  std::string missing_content_mimetype;
};

const Resources* g_resources = nullptr;

std::vector<uint8_t> ToVector(std::string_view str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

std::unique_ptr<Resources> LoadResources() {
  std::unique_ptr<Resources> resources = std::make_unique<Resources>();
  ResourceReader unknown_file(kunknownFile);
  if (unknown_file.IsValid()) {
    resources->unknown_file = ToVector(unknown_file.as_string_view());
    resources->unknown_file_mimetype = "image/png";
  } else {
    resources->unknown_file = ToVector(kunknownFileFallback);
    resources->unknown_file_mimetype = "text/plain";
  }
  ResourceReader missing_content(kMissingContent);
  if (missing_content.IsValid()) {
    resources->missing_content = ToVector(missing_content.as_string_view());
    resources->missing_content_mimetype = "image/png";
  } else {
    resources->missing_content = ToVector(kMissingContentFallback);
    resources->missing_content_mimetype = "text/plain";
  }

  return resources;
}

// This wrapper ensures that we get a copy of the content itself when called
// on the thread pool, instead of a copy of a span, which wouldn't be
// thread-safe
void WriteFileWrapper(base::FilePath path, std::vector<uint8_t> content) {
  base::WriteFile(path, content);
}
}  // namespace

SyncedFileStoreImpl::SyncedFileStoreImpl(base::FilePath profile_path)
    : local_store_path_(profile_path.Append(kStoreDirectoryName)),
      file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {}

SyncedFileStoreImpl::~SyncedFileStoreImpl() = default;

void SyncedFileStoreImpl::Load() {
  static auto resources_loaded = [](std::unique_ptr<Resources> resources) {
    if (!g_resources)
      g_resources = resources.release();
  };

  if (!g_resources) {
    file_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&LoadResources),
        base::BindOnce(resources_loaded));
  }
  SyncedFileStoreStorage::Load(
      local_store_path_, base::BindOnce(&SyncedFileStoreImpl::OnLoadingDone,
                                        weak_factory_.GetWeakPtr()));
}

void SyncedFileStoreImpl::AddOnLoadedCallback(
    base::OnceClosure on_loaded_callback) {
  DCHECK(!IsLoaded());
  on_loaded_callbacks_.push_back(std::move(on_loaded_callback));
}

void SyncedFileStoreImpl::OnLoadingDone(SyncedFilesData files_data) {
  files_data_ = std::move(files_data);

  for (auto& file_data : files_data_) {
    if (file_data.second.IsUnreferenced()) {
      DCHECK(file_data.second.has_content_locally);
      DeleteLocalContent(file_data);
    }
  }

  // Can't use a weak pointer here, because "weak_ptrs can only bind to methods
  // without return values"
  // Unretained is fine, because the callback is ultimately going to be
  // destroyed alongside with storage_
  storage_.emplace(base::BindRepeating(&SyncedFileStoreImpl::GetFilesData,
                                       base::Unretained(this)),
                   local_store_path_, file_task_runner_);

  for (const auto& file_data : files_data_) {
    for (const auto& references : file_data.second.local_references) {
      for (const auto& owner : references.second) {
        checksums_for_local_owners_[references.first][owner] = file_data.first;
      }
    }
  }

  for (const auto& file_data : files_data_) {
    for (const auto& references : file_data.second.sync_references) {
      for (const auto& owner : references.second) {
        checksums_for_sync_owners_[references.first][owner] = file_data.first;
      }
    }
  }

  for (auto& on_loaded_callback : on_loaded_callbacks_)
    std::move(on_loaded_callback).Run();

  on_loaded_callbacks_.clear();
}

void SyncedFileStoreImpl::DoSetLocalFileRef(base::Uuid owner_uuid,
                                            syncer::DataType sync_type,
                                            std::string checksum) {
  DCHECK(IsLoaded());

  auto existing_mapping =
      checksums_for_local_owners_[sync_type].find(owner_uuid);
  if (existing_mapping != checksums_for_local_owners_[sync_type].end()) {
    if (existing_mapping->second == checksum)
      return;

    RemoveLocalRef(owner_uuid, sync_type);
  }

  checksums_for_local_owners_[sync_type][owner_uuid] = checksum;
  files_data_[checksum].local_references[sync_type].insert(owner_uuid);
  storage_->ScheduleSave();
}

void SyncedFileStoreImpl::SetLocalFileRef(base::Uuid owner_uuid,
                                          syncer::DataType sync_type,
                                          std::string checksum) {
  DoSetLocalFileRef(owner_uuid, sync_type, checksum);
  storage_->ScheduleSave();
}

std::string SyncedFileStoreImpl::SetLocalFile(base::Uuid owner_uuid,
                                              syncer::DataType sync_type,
                                              std::vector<uint8_t> content) {
  DCHECK(IsLoaded());
  DCHECK(!content.empty());

  auto hash = crypto::SHA256Hash(content);

  // The checskum will be used as a file name for storage on disk. We use base32
  // in order to support case-insensitive file systems.
  std::string checksum = base32::Base32Encode(
      base::make_span(hash),
      base32::Base32EncodePolicy::OMIT_PADDING);

  checksum += "." + base::NumberToString(content.size());
  DoSetLocalFileRef(owner_uuid, sync_type, checksum);
  auto& file_data = files_data_[checksum];
  if (!file_data.content) {
    net::SniffMimeTypeFromLocalData(
        std::string_view(reinterpret_cast<char*>(&content[0]), content.size()),
        &file_data.mimetype);
    if (file_data.mimetype.empty())
      file_data.mimetype = "text/plain";
    file_data.content = std::move(content);
    if (!file_data.has_content_locally) {
      // We should only be in the process of deleting if we had local content in
      // the first place.
      DCHECK(!file_data.is_deleting);
      file_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&WriteFileWrapper, GetFilePath(checksum),
                                    *file_data.content));
      file_data.has_content_locally = true;
    }

    // Unlikely to occur but there might be pending requests for a duplicate of
    // the file we are just adding.
    file_data.RunPendingCallbacks();
  } else {
    DCHECK(file_data.content == content);
  }
  storage_->ScheduleSave();

  return checksum;
}

void SyncedFileStoreImpl::SetSyncFileRef(std::string owner_sync_id,
                                         syncer::DataType sync_type,
                                         std::string checksum) {
  DCHECK(IsLoaded());

  auto existing_mapping =
      checksums_for_sync_owners_[sync_type].find(owner_sync_id);
  if (existing_mapping != checksums_for_sync_owners_[sync_type].end()) {
    if (existing_mapping->second == checksum)
      return;

    RemoveSyncRef(owner_sync_id, sync_type);
  }

  checksums_for_sync_owners_[sync_type][owner_sync_id] = checksum;
  files_data_[checksum].sync_references[sync_type].insert(owner_sync_id);
  storage_->ScheduleSave();
}

void SyncedFileStoreImpl::GetFile(std::string checksum,
                                  GetFileCallback callback) {
  DCHECK(IsLoaded());

  if (!base::Contains(files_data_, checksum)) {
    std::move(callback).Run(g_resources->unknown_file);
    return;
  }

  auto& file_data = files_data_.at(checksum);
  if (file_data.content) {
    std::move(callback).Run(*file_data.content);
    return;
  }

  if (!file_data.has_content_locally) {
    std::move(callback).Run(g_resources->missing_content);
    return;
  }

  bool first_read_attempt = file_data.pending_callbacks.empty();
  file_data.pending_callbacks.push_back(std::move(callback));
  if (!first_read_attempt) {
    // A request for the file is already being processed. All callbacks will be
    // invoked once the content is available.
    return;
  }

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::ReadFileToBytes, GetFilePath(checksum)),
      base::BindOnce(&SyncedFileStoreImpl::OnReadContentDone,
                     weak_factory_.GetWeakPtr(), checksum));
}

std::string SyncedFileStoreImpl::GetMimeType(std::string checksum) {
  DCHECK(IsLoaded());
  if (!base::Contains(files_data_, checksum))
    return g_resources->unknown_file_mimetype;
  auto& file_data = files_data_.at(checksum);
  if (!file_data.has_content_locally) {
    return g_resources->missing_content_mimetype;
  }
  if (file_data.mimetype.empty())
    return "text/plain";
  return file_data.mimetype;
}

void SyncedFileStoreImpl::RemoveLocalRef(base::Uuid owner_uuid,
                                         syncer::DataType sync_type) {
  DCHECK(IsLoaded());
  auto checksum_node =
      checksums_for_local_owners_[sync_type].extract(owner_uuid);
  if (!checksum_node)
    return;

  auto file_data = files_data_.find(checksum_node.mapped());
  DCHECK(file_data != files_data_.end());
  file_data->second.local_references[sync_type].erase(owner_uuid);
  if (file_data->second.IsUnreferenced()) {
    if (file_data->second.has_content_locally)
      DeleteLocalContent(*file_data);
    else
      files_data_.erase(file_data);
  }

  storage_->ScheduleSave();
}

void SyncedFileStoreImpl::RemoveSyncRef(std::string owner_sync_id,
                                        syncer::DataType sync_type) {
  DCHECK(IsLoaded());
  auto checksum_node =
      checksums_for_sync_owners_[sync_type].extract(owner_sync_id);
  if (!checksum_node)
    return;

  auto file_data = files_data_.find(checksum_node.mapped());
  DCHECK(file_data != files_data_.end());

  file_data->second.sync_references[sync_type].erase(owner_sync_id);
  if (file_data->second.IsUnreferenced()) {
    if (file_data->second.has_content_locally)
      DeleteLocalContent(*file_data);
    else
      files_data_.erase(file_data);
  }
  storage_->ScheduleSave();
}

void SyncedFileStoreImpl::RemoveAllSyncRefsForType(syncer::DataType sync_type) {
  DCHECK(IsLoaded());

  checksums_for_sync_owners_.erase(sync_type);
  std::erase_if(files_data_, [this, sync_type](auto& file_data) {
    file_data.second.sync_references[sync_type].clear();
    if (file_data.second.IsUnreferenced()) {
      if (!file_data.second.has_content_locally)
        return true;
      DeleteLocalContent(file_data);
    }
    return false;
  });
  storage_->ScheduleSave();
}

void SyncedFileStoreImpl::OnReadContentDone(
    std::string checksum,
    std::optional<std::vector<uint8_t>> content) {
  if (!base::Contains(files_data_, checksum)) {
    // The file was removed in the interval since the read was required.
    return;
  }

  if (!content) {
    return;
  }

  auto& file_data = files_data_.at(checksum);
  DCHECK(file_data.has_content_locally);
  if (file_data.content) {
    // The content was obtained from a different source in the interval.
    DCHECK(file_data.content == content);
    return;
  }

  file_data.content = std::move(content);
  file_data.RunPendingCallbacks();
}

bool SyncedFileStoreImpl::IsLoaded() {
  // We instanciate storage only after loading is done
  return storage_.has_value();
}

size_t SyncedFileStoreImpl::GetTotalStorageSize() {
  DCHECK(IsLoaded());
  size_t total_size = 0;
  for (const auto& file_data : files_data_) {
    std::string_view name(file_data.first);
    const size_t size_start = name.find(".");
    if (size_start != std::string_view::npos) {
      size_t size;
      if (base::StringToSizeT(name.substr(size_start + 1), &size))
        total_size += size;
    }
  }
  return total_size;
}

base::FilePath SyncedFileStoreImpl::GetFilePath(
    const std::string& checksum) const {
  return local_store_path_.Append(base::FilePath::FromASCII(checksum));
}

const SyncedFilesData& SyncedFileStoreImpl::GetFilesData() {
  DCHECK(IsLoaded());
  return files_data_;
}

void SyncedFileStoreImpl::DeleteLocalContent(
    std::pair<const std::string, SyncedFileData>& file_data) {
  DCHECK(file_data.second.has_content_locally);
  // Avoid triggering multiple deletions
  file_data.second.content = std::nullopt;
  if (file_data.second.is_deleting)
    return;

  file_data.second.is_deleting = true;
  file_task_runner_->PostTask(
      FROM_HERE,
      base::GetDeleteFileCallback(
          GetFilePath(file_data.first),
          base::BindOnce(&SyncedFileStoreImpl::OnLocalContentDeleted,
                         weak_factory_.GetWeakPtr(), file_data.first)));
}

void SyncedFileStoreImpl::OnLocalContentDeleted(const std::string& checksum,
                                                bool success) {
  auto file_data = files_data_.find(checksum);

  // The metadata shouldn't be removed before the content is gone.
  DCHECK(file_data != files_data_.end());

  // If we didn't succeed, we'll try again next time the store is loaded.
  if (!success) {
    file_data->second.is_deleting = false;
    return;
  }

  // The file may have been re-added while deletion was taking place.
  if (file_data->second.IsUnreferenced()) {
    files_data_.erase(file_data);
  } else if (file_data->second.content) {
    // The file was re-added and we have its content. Recreate the file on disk.
    file_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WriteFileWrapper, GetFilePath(checksum),
                                  *file_data->second.content));
  }

  storage_->ScheduleSave();
}
}  // namespace file_sync