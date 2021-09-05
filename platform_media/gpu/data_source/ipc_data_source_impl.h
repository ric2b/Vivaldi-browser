// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_GPU_DATA_SOURCE_IPC_DATA_SOURCE_IMPL_H_
#define PLATFORM_MEDIA_GPU_DATA_SOURCE_IPC_DATA_SOURCE_IMPL_H_

#include "platform_media/common/feature_toggles.h"

#include "platform_media/gpu/data_source/ipc_data_source.h"

#include "base/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/threading/thread_checker.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace IPC {
class Sender;
}

namespace media {

// An IPCDataSource that satisfies read requests with data obtained via IPC
// from the render process.
class IPCDataSourceImpl {
 public:
  IPCDataSourceImpl(IPC::Sender* channel, int32_t routing_id);
  ~IPCDataSourceImpl();

  void Read(ipc_data_source::Buffer buffer);

  void Stop();

  void OnRawDataReady(int64_t tag, int size);

 private:
  IPC::Sender* channel_;
  const int32_t routing_id_;

  int64_t last_message_tag_ = 0;
  ipc_data_source::Buffer buffer_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(IPCDataSourceImpl);
};

}  // namespace media

#endif  // PLATFORM_MEDIA_GPU_DATA_SOURCE_IPC_DATA_SOURCE_IMPL_H_
