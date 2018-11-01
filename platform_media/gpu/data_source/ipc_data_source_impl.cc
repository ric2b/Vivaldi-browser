// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/gpu/data_source/ipc_data_source_impl.h"

#include <algorithm>

#include "base/numerics/safe_conversions.h"
#include "platform_media/common/media_pipeline_messages.h"
#include "ipc/ipc_sender.h"

namespace content {

class IPCDataSourceImpl::ReadOperation {
 public:
  ReadOperation(int64_t position,
                int size,
                uint8_t* data,
                const ReadCB& callback)
      : position_(position), size_(size), data_(data), callback_(callback) {}

  // Fills |data_| with |size_read| bytes from |data_read| and runs
  // |callback_|, deleting the operation afterwards.
  static void Finish(std::unique_ptr<ReadOperation> operation,
                     const uint8_t* data_read,
                     int size_read) {
    if (size_read > 0) {
      DCHECK_LE(size_read, operation->size_);
      std::copy(data_read, data_read + size_read, operation->data_);
    }

    operation->callback_.Run(size_read);
  }

  int64_t position() const { return position_; }
  int size() const { return size_; }

 private:
  int64_t position_;
  int size_;
  uint8_t* data_;
  ReadCB callback_;
};

IPCDataSourceImpl::IPCDataSourceImpl(IPC::Sender* channel,
                                     int32_t routing_id,
                                     int64_t size,
                                     bool streaming)
    : channel_(channel),
      routing_id_(routing_id),
      size_(size),
      streaming_(streaming),
      stopped_(false),
      suspended_(false),
      should_discard_next_buffer_(false) {
  DCHECK(channel_);
}

IPCDataSourceImpl::~IPCDataSourceImpl() = default;

void IPCDataSourceImpl::Suspend() {
  DVLOG(1) << __FUNCTION__;

  base::AutoLock auto_lock(lock_);
  suspended_ = true;
  should_discard_next_buffer_ = read_operation_.get() != NULL;
}

void IPCDataSourceImpl::Resume() {
  DVLOG(1) << __FUNCTION__;

  base::AutoLock auto_lock(lock_);
  suspended_ = false;
}

void IPCDataSourceImpl::Read(int64_t position,
                             int size,
                             uint8_t* data,
                             const ReadCB& read_cb) {
  DCHECK_GE(size, 0);
  {
    base::AutoLock auto_lock(lock_);

    if (stopped_ || suspended_) {
      read_cb.Run(stopped_ ? kReadError : kReadInterrupted);
      return;
    }

    DCHECK(read_operation_.get() == NULL);
    read_operation_.reset(new ReadOperation(position, size, data, read_cb));
  }

  if (shared_data_ &&
      shared_data_->mapped_size() >= base::saturated_cast<size_t>(size)) {
    channel_->Send(
        new MediaPipelineMsg_ReadRawData(routing_id_, position, size));
  } else {
    channel_->Send(
        new MediaPipelineMsg_RequestBufferForRawData(routing_id_, size));
  }
}

void IPCDataSourceImpl::Stop() {
  {
    base::AutoLock auto_lock(lock_);

    if (stopped_)
      return;

    if (read_operation_.get() != NULL)
      ReadOperation::Finish(std::move(read_operation_), NULL, kReadError);

    stopped_ = true;
  }
}

void IPCDataSourceImpl::Abort() {
  {
    base::AutoLock auto_lock(lock_);

    if (read_operation_.get() != NULL)
      ReadOperation::Finish(std::move(read_operation_), NULL, kReadError);
  }
}

bool IPCDataSourceImpl::GetSize(int64_t* size) {
  *size = size_;
  return size_ >= 0;
}

bool IPCDataSourceImpl::IsStreaming() {
  return streaming_;
}

void IPCDataSourceImpl::SetBitrate(int bitrate) {
  NOTREACHED();
}

void IPCDataSourceImpl::OnBufferForRawDataReady(
    size_t buffer_size,
    base::SharedMemoryHandle handle) {
  if (read_operation_.get() == NULL) {
    DLOG(ERROR) << "Received buffer while no read operation is in progress.";
    return;
  }

  if (!base::SharedMemory::IsHandleValid(handle)) {
    ReadOperation::Finish(std::move(read_operation_), NULL, kReadError);
    return;
  }

  if (base::saturated_cast<int>(buffer_size) < read_operation_->size()) {
    DLOG(ERROR) << "Received buffer is too small.";
    ReadOperation::Finish(std::move(read_operation_), NULL, kReadError);
    return;
  }

  shared_data_.reset(new base::SharedMemory(handle, true));
  if (!shared_data_->Map(buffer_size)) {
    ReadOperation::Finish(std::move(read_operation_), NULL, kReadError);
    return;
  }

  channel_->Send(new MediaPipelineMsg_ReadRawData(
      routing_id_, read_operation_->position(), read_operation_->size()));
}

void IPCDataSourceImpl::OnRawDataReady(int size) {
  {
    base::AutoLock auto_lock(lock_);

    if (stopped_)
      return;

    if (should_discard_next_buffer_) {
      DCHECK(read_operation_);
      should_discard_next_buffer_ = false;
      ReadOperation::Finish(std::move(read_operation_), NULL, kReadInterrupted);
      return;
    }

    if (read_operation_.get() == NULL) {
      DLOG(ERROR) << "Unexpected call to " << __FUNCTION__;
      return;
    }

    if (size > 0) {
      if (shared_data_.get() != NULL &&
          base::saturated_cast<int>(shared_data_->mapped_size()) >= size) {
        ReadOperation::Finish(std::move(read_operation_),
                              static_cast<uint8_t*>(shared_data_->memory()),
                              size);
        return;
      }

      size = kReadError;
    }

    ReadOperation::Finish(std::move(read_operation_), NULL, size);
  }
}

}  // namespace content
