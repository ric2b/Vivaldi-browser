// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "platform_media/gpu/data_source/ipc_data_source.h"

#include "platform_media/common/platform_ipc_util.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/numerics/safe_conversions.h"

#include <algorithm>
#include <memory>

// Uncomment to turn on content logging
//#define CONTENT_LOG_FOLDER FILE_PATH_LITERAL("D:\\logs")

#ifdef CONTENT_LOG_FOLDER

#include "base/files/file.h"
#include "base/files/file_util.h"

#endif  // CONTENT_LOG_FOLDER

namespace ipc_data_source {

// The media log must never be included into official builds even by an
// accident.
#if defined(CONTENT_LOG_FOLDER) && !defined(OFFICIAL_BUILD)

namespace {

struct MediaLogItem {
  FILE* fp = nullptr;
};

std::map<const void*, MediaLogItem> media_log_items;

void OpenMediaLog(const void* hash_key) {
  base::FilePath path;
  FILE* fp = CreateAndOpenTemporaryFileInDir(base::FilePath(CONTENT_LOG_FOLDER),
                                             &path);
  // Will fail if we are sandboxed
  if (!fp) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " failed to open a file to capture the media, check that "
                  "--disable-gpu-sandbox is on";
    return;
  }
  LOG(INFO) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " capturing media to "
            << path.AsUTF8Unsafe();
  MediaLogItem item;
  item.fp = fp;
  media_log_items.emplace(hash_key, std::move(item));
}

void CloseMediaLog(const void* hash_key) {
  auto i = media_log_items.find(hash_key);
  if (i == media_log_items.end())
    return;
  fclose(i->second.fp);
  media_log_items.erase(i);
}

void WriteMediaLog(const void* hash_key,
                   int64_t position,
                   const void* data,
                   size_t size) {
  if (read_size <= 0) {
    CloseMediaLog(hash_key);
    return;
  }
  auto i = media_log_items.find(source);
  if (i == media_log_items.end()) {
    OpenMediaLog(hash_key);
    i = media_log_items.find(source);
    if (i == media_log_items.end())
      return;
  }
#ifdef OS_WIN
  _fseeki64(i->second.fp, position, SEEK_SET);
#else
  fseeko(i->second.fp, position, SEEK_SET);
#endif  // OS_WIN
  fwrite(data, 1, size, i->second.fp);
  LOG(INFO) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " position=" << position
            << " write_size=" << size;
}

}  // namespace

#endif  // CONTENT_LOG_FOLDER

struct Buffer::Impl {
  Impl() {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " this=" << this;
  }
  ~Impl() {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " this=" << this;
  }
  int64_t position = 0;
  int requested_size = 0;
  int read_size = 0;
  base::ReadOnlySharedMemoryMapping mapping;
  Reader source_reader;
  ReadCB read_cb;
};

Buffer::Buffer() = default;
Buffer::Buffer(Buffer&& buffer) = default;
Buffer::~Buffer() = default;

Buffer& Buffer::operator=(Buffer&& buffer) {
  DCHECK(!impl_);
  impl_ = std::move(buffer.impl_);
  return *this;
}

void Buffer::Init(base::ReadOnlySharedMemoryMapping mapping,
                  Reader source_reader) {
  DCHECK(!impl_);
  impl_ = std::make_unique<Impl>();
  impl_->mapping = std::move(mapping);
  impl_->source_reader = std::move(source_reader);
}

int Buffer::GetCapacity() {
  DCHECK(impl_);
  if (!impl_->mapping.IsValid())
    return 0;
  return impl_->mapping.size();
}

void Buffer::SetReadRange(int64_t position, int size) {
  DCHECK(impl_);
  DCHECK_GE(position, 0);
  DCHECK_GT(size, 0);
  DCHECK_LE(size, GetCapacity());
  impl_->position = position;
  impl_->requested_size = size;
}

int64_t Buffer::GetReadPosition() const {
  DCHECK(impl_);
  return impl_->position;
}

int Buffer::GetRequestedSize() const {
  DCHECK(impl_);
  return impl_->requested_size;
}

// static
void Buffer::Read(Buffer buffer, ReadCB read_cb) {
  DCHECK(buffer);
  DCHECK(read_cb);
  DCHECK_GT(buffer.impl_->requested_size, 0);
  if (buffer.IsReadError()) {
    std::move(read_cb).Run(std::move(buffer));
    return;
  }
  buffer.impl_->read_cb = std::move(read_cb);
  Reader& source_reader = buffer.impl_->source_reader;
  source_reader.Run(std::move(buffer));
}

// static
bool Buffer::OnRawDataRead(int read_size, Buffer buffer) {
  DCHECK(buffer.impl_);
  DCHECK(buffer.impl_->read_cb);

  VLOG(4) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " read_size=" << read_size
          << " requested_size=" << buffer.GetRequestedSize()
          << " read_position=" << buffer.GetReadPosition();

  do {
    if (read_size < 0)
      break;

    if (read_size > buffer.GetRequestedSize()) {
      LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " the size from the reply exceeded the requested size "
                 << buffer.GetRequestedSize();
      read_size = kReadError;
      break;
    }

    // Read error cannot be reset.
    if (buffer.IsReadError()) {
      read_size = kReadError;
      break;
    }

    if (read_size > 0) {
      size_t mapping_size =
          buffer.impl_->mapping.IsValid() ? buffer.impl_->mapping.size() : 0;
      if (static_cast<size_t>(read_size) > mapping_size) {
        LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
                   << " the shared memory buffer is null or too small: "
                   << mapping_size;
        read_size = kReadError;
        break;
      }
    }
  } while (false);

  buffer.impl_->read_size = read_size;
#if defined(CONTENT_LOG_FOLDER)
  WriteMediaLog(buffer.impl_, buffer.GetReadPosition(), buffer.GetReadData(),
                read_size);
#endif  // CONTENT_LOG_FOLDER

  if (read_size < 0) {
    // Errors are not recoverable so release no longer used mapping.
    buffer.impl_->mapping = base::ReadOnlySharedMemoryMapping();
  }

  ReadCB& read_cb = buffer.impl_->read_cb;
  std::move(read_cb).Run(std::move(buffer));
  return read_size >= 0;
}

bool Buffer::IsReadError() const {
  DCHECK(impl_);
  return impl_->read_size < 0;
}

int Buffer::GetReadSize() const {
  DCHECK(impl_);
  return impl_->read_size;
}

int64_t Buffer::GetLastReadEnd() const {
  DCHECK(impl_);
  if (impl_->read_size < 0)
    return -1;
  return impl_->position + impl_->read_size;
}

const uint8_t* Buffer::GetReadData() const {
  DCHECK(impl_);
  if (impl_->read_size <= 0)
    return nullptr;
  DCHECK(impl_->mapping.IsValid());
  return impl_->mapping.GetMemoryAs<uint8_t>();
}

}  // namespace ipc_data_source
