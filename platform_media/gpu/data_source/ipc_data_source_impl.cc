// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/gpu/data_source/ipc_data_source_impl.h"

#include <algorithm>

#include "base/numerics/safe_conversions.h"
#include "platform_media/common/media_pipeline_messages.h"
#include "ipc/ipc_sender.h"

#ifndef NDEBUG
#include "base/files/file.h"
#include "base/files/file_util.h"

// Uncomment to turn on content logging
//#define CONTENT_LOG_FOLDER FILE_PATH_LITERAL("D:\\logs")
#endif //NDEBUG

namespace media {

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
      should_discard_next_buffer_(false)
#ifndef NDEBUG
      ,content_log_size_(0)
#endif //NDEBUG
      {
  DCHECK(channel_);
  VLOG(4) << " PROPMEDIA(GPU) : " << __FUNCTION__;

#if !defined(NDEBUG) && defined(CONTENT_LOG_FOLDER)
  // Will fail if we are sandboxed
  if (CreateTemporaryFileInDir(base::FilePath(CONTENT_LOG_FOLDER),
                               &content_log_path_)) {
    base::File content_log_file(content_log_path_, base::File::FLAG_OPEN |
                                                       base::File::FLAG_READ |
                                                       base::File::FLAG_WRITE);
    if (!GetSize(&content_log_size_))
      content_log_size_ = 1024 * 1024 * 16;
    content_log_ = std::make_unique<base::MemoryMappedFile>();
    content_log_->Initialize(std::move(content_log_file),
                             {0, content_log_size_},
                             base::MemoryMappedFile::READ_WRITE_EXTEND);
  }
#endif //NDEBUG
}

IPCDataSourceImpl::~IPCDataSourceImpl() = default;

void IPCDataSourceImpl::Suspend() {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;

  base::AutoLock auto_lock(lock_);
  suspended_ = true;
  should_discard_next_buffer_ = read_operation_.get() != NULL;
}

void IPCDataSourceImpl::Resume() {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;

  base::AutoLock auto_lock(lock_);
  suspended_ = false;
}

void IPCDataSourceImpl::Read(int64_t position,
                             int size,
                             uint8_t* data,
                             const ReadCB& read_cb) {
  VLOG(4) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " Requesting read of size " << size
          << " from position " << position;
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
    VLOG(4) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " MediaPipelineMsg_ReadRawData";
    channel_->Send(
        new MediaPipelineMsg_ReadRawData(routing_id_, position, size));
  } else {
    VLOG(4) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " MediaPipelineMsg_RequestBufferForRawData";
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
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Received buffer while no read operation is in progress.";
    return;
  }

  if (!base::SharedMemory::IsHandleValid(handle)) {
    ReadOperation::Finish(std::move(read_operation_), NULL, kReadError);
    return;
  }

  if (base::saturated_cast<int>(buffer_size) < read_operation_->size()) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " Received buffer is too small.";
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
      LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " Unexpected call to " << __FUNCTION__;
      return;
    }

    if (size > 0) {
      if (shared_data_.get() != NULL &&
          base::saturated_cast<int>(shared_data_->mapped_size()) >= size) {
        auto* shared_memory = static_cast<uint8_t*>(shared_data_->memory());
#ifndef NDEBUG
        if (content_log_size_ > 0) {
          if (read_operation_->position() + read_operation_->size() >
              content_log_size_) {
            content_log_size_ = read_operation_->position() + read_operation_->size() + 1024 * 1024 * 16;
            content_log_ = std::make_unique<base::MemoryMappedFile>();
            base::File content_log_file(content_log_path_,
                                        base::File::FLAG_OPEN |
                                            base::File::FLAG_READ |
                                            base::File::FLAG_WRITE);
            content_log_->Initialize(std::move(content_log_file),
                                     {0, content_log_size_},
                                     base::MemoryMappedFile::READ_WRITE_EXTEND);
          }
          std::copy(shared_memory, shared_memory + size, content_log_->data() + read_operation_->position());
        }
#endif //NDEBUG
        ReadOperation::Finish(std::move(read_operation_),
                              shared_memory,
                              size);
        return;
      }

      size = kReadError;
    }

    ReadOperation::Finish(std::move(read_operation_), NULL, size);
  }
}

}  // namespace media
