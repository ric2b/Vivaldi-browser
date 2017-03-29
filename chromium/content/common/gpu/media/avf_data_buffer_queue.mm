// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "content/common/gpu/media/avf_data_buffer_queue.h"

#include <deque>

#include "base/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "media/base/buffers.h"
#include "media/base/data_buffer.h"

namespace content {

class AVFDataBufferQueue::Queue {
 public:
  Queue() : data_size_(0) {}

  void Push(const scoped_refptr<media::DataBuffer>& buffer);
  scoped_refptr<media::DataBuffer> Pop();

  void Clear();

  bool empty() const { return buffers_.empty(); }
  size_t data_size() const { return data_size_; }
  base::TimeDelta GetDuration() const;

 private:
  std::deque<scoped_refptr<media::DataBuffer>> buffers_;
  size_t data_size_;
};

void AVFDataBufferQueue::Queue::Push(
    const scoped_refptr<media::DataBuffer>& buffer) {
  DCHECK(buffer->timestamp() != media::kNoTimestamp());
  DCHECK(buffers_.empty() || buffer->timestamp() > buffers_.back()->timestamp())
      << "Out-of-order buffer @" << buffer->timestamp().InMicroseconds();
  DCHECK_GE(buffer->data_size(), 0);

  buffers_.push_back(buffer);
  data_size_ += buffer->data_size();
}

scoped_refptr<media::DataBuffer> AVFDataBufferQueue::Queue::Pop() {
  scoped_refptr<media::DataBuffer> buffer = buffers_.front();
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
    Type type,
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

void AVFDataBufferQueue::Read(const ReadCB& read_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(read_cb_.is_null());

  DVLOG(7) << "Read()";

  read_cb_ = read_cb;
  SatisfyPendingRead();
}

void AVFDataBufferQueue::BufferReady(
    const scoped_refptr<media::DataBuffer>& buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(buffer.get() != NULL);
  DCHECK(!end_of_stream_) << "Can't enqueue buffers anymore";

  DVLOG(7) << (type_ == AUDIO ? "AUDIO" : "VIDEO")
           << " buffer ready: timestamp="
           << buffer->timestamp().InMicroseconds()
           << " duration=" << buffer->duration().InMicroseconds()
           << " size=" << buffer->data_size();

  buffer_queue_->Push(buffer);
  DVLOG(7) << DescribeBufferSize();

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

  DVLOG(7) << DescribeBufferSize();
}

bool AVFDataBufferQueue::HasAvailableCapacity() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  return buffer_queue_->GetDuration() < capacity_;
}

size_t AVFDataBufferQueue::memory_usage() const {
  return buffer_queue_->data_size();
}

std::string AVFDataBufferQueue::DescribeBufferSize() const {
  return base::StringPrintf("Have %lld us of queued %s data, queue size: %lu",
                            buffer_queue_->GetDuration().InMicroseconds(),
                            type_ == AUDIO ? "AUDIO" : "VIDEO",
                            buffer_queue_->data_size());
}

void AVFDataBufferQueue::SatisfyPendingRead() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!read_cb_.is_null()) {
    scoped_refptr<media::DataBuffer> buffer;

    if (end_of_stream_) {
      buffer = !buffer_queue_->empty() ? buffer_queue_->Pop()
                                       : media::DataBuffer::CreateEOSBuffer();
    } else if (!HasAvailableCapacity() ||
               (catching_up_ && !buffer_queue_->empty())) {
      // The condition above will allow the queue to collect some number of
      // buffers before we start to return them.  Unless we are in the
      // "catching up" mode, which is when we want to return the buffers as
      // quickly as possible.
      buffer = buffer_queue_->Pop();
      catching_up_ = true;
    }

    if (buffer.get() != NULL) {
      DVLOG(7) << DescribeBufferSize();
      base::ResetAndReturn(&read_cb_).Run(buffer);
    } else {
      DVLOG(7) << "Not enough data available to satisfy read request. Request "
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

}  // namespace content
