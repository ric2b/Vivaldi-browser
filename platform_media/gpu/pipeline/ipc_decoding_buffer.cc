// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "platform_media/gpu/pipeline/ipc_decoding_buffer.h"

#include "platform_media/common/platform_ipc_util.h"

#include "base/logging.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"

#include <algorithm>

namespace media {

IPCDecodingBuffer::Impl::Impl(PlatformMediaDataType type) : type(type) {}

IPCDecodingBuffer::Impl::~Impl() = default;

IPCDecodingBuffer::IPCDecodingBuffer() = default;
IPCDecodingBuffer::IPCDecodingBuffer(IPCDecodingBuffer&&) = default;
IPCDecodingBuffer::~IPCDecodingBuffer() = default;
IPCDecodingBuffer& IPCDecodingBuffer::operator=(IPCDecodingBuffer&&) = default;

// static
void IPCDecodingBuffer::Reply(IPCDecodingBuffer buffer) {
  const ReplyCB& reply_cb = buffer.impl_->reply_cb;
  reply_cb.Run(std::move(buffer));
}

void IPCDecodingBuffer::Init(PlatformMediaDataType type) {
  // Init should not be called twice.
  DCHECK(!impl_);
  impl_ = std::make_unique<Impl>(type);
}

PlatformAudioConfig& IPCDecodingBuffer::GetAudioConfig() {
  DCHECK(impl_->type == PlatformMediaDataType::PLATFORM_MEDIA_AUDIO);
  if (!impl_->audio_config) {
    impl_->audio_config = std::make_unique<PlatformAudioConfig>();
  }
  return *impl_->audio_config;
}

PlatformVideoConfig& IPCDecodingBuffer::GetVideoConfig() {
  DCHECK(impl_->type == PlatformMediaDataType::PLATFORM_MEDIA_VIDEO);
  if (!impl_->video_config) {
    impl_->video_config = std::make_unique<PlatformVideoConfig>();
  }
  return *impl_->video_config;
}

const uint8_t* IPCDecodingBuffer::GetDataForTests() const {
  return impl_->data_size <= 0 ? nullptr
                               : impl_->mapping.GetMemoryAs<uint8_t>();
}

uint8_t* IPCDecodingBuffer::PrepareMemory(size_t data_size) {
  // It is not clear if zero-size can be absolutely ruled out on all
  // platforms in all corner cases. So assert but handle. In particular, always
  // allocate the region of at least one page even when data_size == 0.
  DCHECK(data_size > 0);
  if (data_size > static_cast<size_t>(kMaxSharedMemorySize)) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " too big buffer - "
               << data_size;
    return nullptr;
  }
  if (!impl_->mapping.IsValid() || impl_->mapping.mapped_size() < data_size) {
    size_t region_size =
        RoundUpTo4kPage(std::max(data_size, static_cast<size_t>(1)));
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " Allocating new shared region " << region_size;
    // To make sure that on errors we do not change the state of the buffer we
    // first allocate the new region, then check the allocation then release the
    // old mapping.
    base::MappedReadOnlyRegion region =
        base::ReadOnlySharedMemoryRegion::Create(region_size);
    if (!region.IsValid()) {
      LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " allocation failed for size " << region_size;
      return nullptr;
    }

    impl_->region = std::move(region.region);
    impl_->mapping = std::move(region.mapping);
  }
  impl_->data_size = static_cast<int>(data_size);
  return impl_->mapping.GetMemoryAs<uint8_t>();
}

}  // namespace media
