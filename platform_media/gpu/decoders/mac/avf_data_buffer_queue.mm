// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "platform_media/gpu/decoders/mac/avf_data_buffer_queue.h"

#include <deque>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "common/platform_media_pipeline_types.h"
#include "media/base/data_buffer.h"
#include "media/base/timestamp_constants.h"

namespace media {

class AVFDataBufferQueue::Queue {
 public:
  Queue() : data_size_(0) {}

  void Push(const scoped_refptr<DataBuffer>& buffer);
  scoped_refptr<DataBuffer> Pop();

  void Clear();

  bool empty() const { return buffers_.empty(); }
  size_t data_size() const { return data_size_; }
  base::TimeDelta GetDuration() const;

 private:
  std::deque<scoped_refptr<DataBuffer>> buffers_;
  size_t data_size_;
};

void AVFDataBufferQueue::Queue::Push(
    const scoped_refptr<DataBuffer>& buffer) {
  DCHECK(buffer->timestamp() != media::kNoTimestamp);
  DCHECK(buffers_.empty() || buffer->timestamp() > buffers_.back()->timestamp())
      << "Out-of-order buffer @" << buffer->timestamp().InMicroseconds();
  DCHECK_GE(buffer->data_size(), 0);

  buffers_.push_back(buffer);
  data_size_ += buffer->data_size();
}

scoped_refptr<DataBuffer> AVFDataBufferQueue::Queue::Pop() {
  scoped_refptr<DataBuffer> buffer = buffers_.front();
  buffers_.pop_front();
  data_size_ -= buffer->data_size();
  return buffer;
}

void AVFDataBufferQueue::Queue::Clear() {
  buffers_.clear();
  data_size_ = 0;
}

base::TimeDelta AVFDataBufferQueue::Queue::GetDuration() const {
  if (buffers_.size() < 2)
    return base::TimeDelta();

  return buffers_.back()->timestamp() - buffers_.front()->timestamp();
}


AVFDataBufferQueue::AVFDataBufferQueue(
    PlatformMediaDataType type,
    const base::TimeDelta& capacity,
    const base::Closure& capacity_available_cb,
    const base::Closure& capacity_depleted_cb)
    : type_(type),
      capacity_(capacity),
      capacity_available_cb_(capacity_available_cb),
      capacity_depleted_cb_(capacity_depleted_cb),
      buffer_queue_(new Queue),
      catching_up_(false),
      end_of_stream_(false) {
  DCHECK(!capacity_available_cb_.is_null());
  DCHECK(!capacity_depleted_cb_.is_null());
}

AVFDataBufferQueue::~AVFDataBufferQueue() = default;

void AVFDataBufferQueue::Read(IPCDecodingBuffer ipc_decoding_buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(ipc_decoding_buffer);
  DCHECK(!ipc_decoding_buffer_);

  VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " Read()";

  ipc_decoding_buffer_ = std::move(ipc_decoding_buffer);
  SatisfyPendingRead();
}

void AVFDataBufferQueue::BufferReady(
    const scoped_refptr<DataBuffer>& buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(buffer.get() != NULL);
  DCHECK(!end_of_stream_) << "Can't enqueue buffers anymore";

  VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << (type_ == PlatformMediaDataType::PLATFORM_MEDIA_AUDIO ? " AUDIO"
                                                                   : " VIDEO")
          << " buffer ready: timestamp=" << buffer->timestamp().InMicroseconds()
          << " duration=" << buffer->duration().InMicroseconds()
          << " size=" << buffer->data_size();

  buffer_queue_->Push(buffer);
  VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " " << DescribeBufferSize();

  if (!HasAvailableCapacity())
    catching_up_ = false;

  SatisfyPendingRead();
}

void AVFDataBufferQueue::SetEndOfStream() {
  DCHECK(thread_checker_.CalledOnValidThread());

  end_of_stream_ = true;
  SatisfyPendingRead();
}

void AVFDataBufferQueue::Flush() {
  DCHECK(thread_checker_.CalledOnValidThread());

  end_of_stream_ = false;
  buffer_queue_->Clear();
  catching_up_ = false;

  VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " " << DescribeBufferSize();
}

bool AVFDataBufferQueue::HasAvailableCapacity() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  return buffer_queue_->GetDuration() < capacity_;
}

size_t AVFDataBufferQueue::memory_usage() const {
  return buffer_queue_->data_size();
}

std::string AVFDataBufferQueue::DescribeBufferSize() const {
  return base::StringPrintf(
      "Have %lld us of queued %s data, queue size: %zu",
      buffer_queue_->GetDuration().InMicroseconds(),
      (type_ == PlatformMediaDataType::PLATFORM_MEDIA_AUDIO ? " AUDIO"
                                                            : " VIDEO"),
      buffer_queue_->data_size());
}

void AVFDataBufferQueue::SatisfyPendingRead() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (ipc_decoding_buffer_) {
    base::Optional<MediaDataStatus> status;
    scoped_refptr<DataBuffer> buffer;

    if (end_of_stream_) {
      if (buffer_queue_->empty()) {
        status = MediaDataStatus::kEOS;
      } else {
        status = MediaDataStatus::kOk;
        buffer = buffer_queue_->Pop();
      }
    } else if (!HasAvailableCapacity() ||
               (catching_up_ && !buffer_queue_->empty())) {
      // The condition above will allow the queue to collect some number of
      // buffers before we start to return them.  Unless we are in the
      // "catching up" mode, which is when we want to return the buffers as
      // quickly as possible.
      status = MediaDataStatus::kOk;
      buffer = buffer_queue_->Pop();
      catching_up_ = true;
    }

    if (status) {
      VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " "
              << DescribeBufferSize();

      if (*status == MediaDataStatus::kOk) {
        size_t data_size = buffer->data_size();
        uint8* memory = ipc_decoding_buffer_.PrepareMemory(data_size);
        if (!memory) {
          *status = MediaDataStatus::kMediaError;
        } else {
          memcpy(memory, buffer->data(), data_size);
          ipc_decoding_buffer_.set_timestamp(buffer->timestamp());
          ipc_decoding_buffer_.set_duration(buffer->duration());
        }
      }
      ipc_decoding_buffer_.set_status(*status);
      IPCDecodingBuffer::Reply(std::move(ipc_decoding_buffer_));
    } else {
      VLOG(7) << " PROPMEDIA(GPU) : " << __FUNCTION__
              << " Not enough data available to satisfy read request. Request "
                 "is pending.";
    }

    if (buffer_queue_->empty())
      catching_up_ = false;
  }

  if (HasAvailableCapacity() && !end_of_stream_)
    capacity_available_cb_.Run();
  else if (!HasAvailableCapacity())
    capacity_depleted_cb_.Run();
}

}  // namespace media
