// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is based on original work developed by Opera Software ASA.


#include "platform_media/renderer/pipeline/ipc_pipeline_data.h"

#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/base/decoder_buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"

#include "platform_media/renderer/pipeline/ipc_pipeline_data_source.h"

namespace {

std::unique_ptr<base::SharedMemory> AllocateSharedMemory(size_t size) {
  VLOG(5) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " size of allocation " << size;

  mojo::ScopedSharedBufferHandle handle = mojo::SharedBufferHandle::Create(size);
  if (!handle.is_valid()) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << " allocation failed for size " << size;
    return std::unique_ptr<base::SharedMemory>();
  }

  base::SharedMemoryHandle platform_handle;
  size_t shared_memory_size = 0;
  mojo::UnwrappedSharedMemoryHandleProtection protection;
  MojoResult result = mojo::UnwrapSharedMemoryHandle(std::move(handle), &platform_handle, &shared_memory_size, &protection);
  if (result != MOJO_RESULT_OK) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << " unwrapping of shared memory handle failed for size " << size;
    return std::unique_ptr<base::SharedMemory>();
  }
  DCHECK_EQ(shared_memory_size, size);

  bool read_only =
      protection == mojo::UnwrappedSharedMemoryHandleProtection::kReadOnly;
  return std::make_unique<base::SharedMemory>(platform_handle, read_only);
}

}  // namespace

namespace media {

class IPCPipelineData::SharedData {
 public:
  explicit SharedData(gpu::GpuChannelHost* channel);
  ~SharedData();

  bool Prepare(size_t size);
  // Checks if internal buffer is present and big enough.
  bool IsSufficient(int needed_size) const;

  base::SharedMemoryHandle handle() const;
  uint8_t* memory() const;
  int mapped_size() const;

 private:
  gpu::GpuChannelHost* channel_;
  std::unique_ptr<base::SharedMemory> memory_;
};

IPCPipelineData::SharedData::SharedData(gpu::GpuChannelHost* channel)
    : channel_(channel) {
  DCHECK(channel_);
}

IPCPipelineData::SharedData::~SharedData() {
}

bool IPCPipelineData::SharedData::Prepare(size_t size) {
  if (size == 0 || !base::IsValueInRangeForNumericType<int>(size)) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << " size is not valid : " << size;
    return false;
  }

  if (!IsSufficient(static_cast<int>(size))) {
    memory_ = AllocateSharedMemory(size);
    if (!memory_) {
      LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " allocation failed for size " << size;
      return false;
    }

    if (!memory_->Map(size)) {
      LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " mapping of memory failed for size " << size;
      memory_.reset(NULL);
      return false;
    }
  }
  return true;
}

bool IPCPipelineData::SharedData::IsSufficient(int needed_size) const {
  return base::IsValueInRangeForNumericType<size_t>(needed_size) && memory_ &&
         memory_->mapped_size() >= static_cast<size_t>(needed_size);
}

base::SharedMemoryHandle IPCPipelineData::SharedData::handle() const {
  DCHECK(memory_);
  return memory_->handle();
}

uint8_t* IPCPipelineData::SharedData::memory() const {
  DCHECK(memory_);
  return static_cast<uint8_t*>(memory_->memory());
}

int IPCPipelineData::SharedData::mapped_size() const {
  DCHECK(memory_);
  return base::saturated_cast<int>(memory_->mapped_size());
}

IPCPipelineData::IPCPipelineData(DataSource* data_source,
                                 gpu::GpuChannelHost* channel)
  : data_source_(new IPCPipelineDataSource(data_source)),
    shared_raw_data_(new SharedData(channel)) {
  LOG(INFO) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;

  for (int i = 0; i < PlatformMediaDataType::PLATFORM_MEDIA_DATA_TYPE_COUNT; ++i) {
    shared_decoded_data_[i].reset(new SharedData(channel));
  }
}

IPCPipelineData::~IPCPipelineData() {

}

bool IPCPipelineData::PrepareDecoded(PlatformMediaDataType type,
                                     size_t requested_size) {
  return shared_decoded_data_[type]->Prepare(requested_size);
}

bool IPCPipelineData::IsSufficientDecoded(PlatformMediaDataType type,
                                          int needed_size) const {
  return shared_decoded_data_[type]->IsSufficient(needed_size);
}

base::SharedMemoryHandle IPCPipelineData::HandleDecoded(
    PlatformMediaDataType type) const {
  return shared_decoded_data_[type]->handle();
}

uint8_t* IPCPipelineData::MemoryDecoded(PlatformMediaDataType type) const {
  return shared_decoded_data_[type]->memory();
}

int IPCPipelineData::MappedSizeDecoded(PlatformMediaDataType type) const {
  return shared_decoded_data_[type]->mapped_size();
}

bool IPCPipelineData::PrepareRaw(size_t requested_size) {
  return shared_raw_data_->Prepare(requested_size);
}

bool IPCPipelineData::IsSufficientRaw(int needed_size) const {
  return shared_raw_data_->IsSufficient(needed_size);
}

base::SharedMemoryHandle IPCPipelineData::HandleRaw() const {
  return shared_raw_data_->handle();
}

uint8_t* IPCPipelineData::MemoryRaw() const {
  return shared_raw_data_->memory();
}

int IPCPipelineData::MappedSizeRaw() const {
  return shared_raw_data_->mapped_size();
}

void IPCPipelineData::ReadFromSource(int64_t position,
          int size,
          uint8_t* data,
          const DataSource::ReadCB& read_cb) {
  data_source_->ReadFromSource(position, size, data, read_cb);
}

void IPCPipelineData::IPCPipelineData::StopSource() {
  data_source_->StopSource();
}

bool IPCPipelineData::GetSizeSource(int64_t* size_out) {
  return data_source_->GetSizeSource(size_out);
}

bool IPCPipelineData::IsStreamingSource() {
  return data_source_->IsStreamingSource();
}

void IPCPipelineData::AppendBuffer(const scoped_refptr<DecoderBuffer>& buffer,
                                     const VideoDecoder::DecodeCB& decode_cb) {
  data_source_->AppendBuffer(buffer, decode_cb);
}

bool IPCPipelineData::HasEnoughData() {
  return data_source_->HasEnoughData();
}

int IPCPipelineData::GetMaxDecodeBuffers() {
  return data_source_->GetMaxDecodeBuffers();
}

}  // namespace media
