// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "platform_media/gpu/data_source/ipc_data_source.h"

#include "platform_media/common/platform_ipc_util.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/numerics/safe_conversions.h"

#include <algorithm>
#include <memory>

namespace ipc_data_source {

struct Buffer::Impl {
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
  DCHECK(position >= 0);
  DCHECK(size > 0);
  DCHECK(size <= GetCapacity());
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
  DCHECK(buffer.impl_->requested_size > 0);
  if (buffer.IsReadError()) {
    std::move(read_cb).Run(std::move(buffer));
    return;
  }
  buffer.impl_->read_cb = std::move(read_cb);
  Reader& source_reader = buffer.impl_->source_reader;
  source_reader.Run(std::move(buffer));
}

void Buffer::SetReadError() {
  DCHECK(impl_);
  impl_->read_size = kReadError;

  // Errors are not recoverable so release no longer used mapping.
  impl_->mapping = base::ReadOnlySharedMemoryMapping();
}

void Buffer::SetReadSize(int read_size) {
  DCHECK(impl_);

  // Read error cannot be reset.
  if (IsReadError())
    return;

  if (read_size < 0) {
    SetReadError();
    return;
  }
  if (read_size > 0) {
    size_t mapping_size = impl_->mapping.IsValid() ? impl_->mapping.size() : 0;
    if (static_cast<size_t>(read_size) > mapping_size) {
      LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " the shared memory buffer is null or too small: "
                 << mapping_size;
      SetReadError();
      return;
    }
  }

  impl_->read_size = read_size;
}

// static
void Buffer::SendReply(Buffer buffer) {
  DCHECK(buffer.impl_);
  DCHECK(buffer.impl_->read_cb);
  ReadCB& read_cb = buffer.impl_->read_cb;
  std::move(read_cb).Run(std::move(buffer));
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
