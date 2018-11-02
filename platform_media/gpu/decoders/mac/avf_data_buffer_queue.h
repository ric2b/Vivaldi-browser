// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#ifndef PLATFORM_MEDIA_GPU_DECODERS_MAC_AVF_DATA_BUFFER_QUEUE_H_
#define PLATFORM_MEDIA_GPU_DECODERS_MAC_AVF_DATA_BUFFER_QUEUE_H_

#include "platform_media/common/feature_toggles.h"

#include <string>

#include "base/callback.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/base/media_export.h"

namespace media {
class DataBuffer;
}

namespace media {

class MEDIA_EXPORT AVFDataBufferQueue {
 public:
  // Used for debugging only.
  enum Type { AUDIO, VIDEO };

  using ReadCB = base::Callback<void(const scoped_refptr<DataBuffer>&)>;

  AVFDataBufferQueue(Type type,
                     const base::TimeDelta& capacity,
                     const base::Closure& capacity_available_cb,
                     const base::Closure& capacity_depleted_cb);
  ~AVFDataBufferQueue();

  void Read(const ReadCB& read_cb);

  void BufferReady(const scoped_refptr<DataBuffer>& buffer);

  void SetEndOfStream();

  void Flush();

  bool HasAvailableCapacity() const;

  size_t memory_usage() const;

 private:
  class Queue;

  std::string DescribeBufferSize() const;
  void SatisfyPendingRead();

  const Type type_;
  const base::TimeDelta capacity_;
  base::Closure capacity_available_cb_;
  base::Closure capacity_depleted_cb_;
  ReadCB read_cb_;
  std::unique_ptr<Queue> buffer_queue_;

  // We are "catching up" if the stream associated with this queue lags behind
  // another stream.  This is when we want to allow the queue to return any
  // buffers it currently has as quickly as possible.
  bool catching_up_;

  bool end_of_stream_;

  base::ThreadChecker thread_checker_;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_GPU_DECODERS_MAC_AVF_DATA_BUFFER_QUEUE_H_
