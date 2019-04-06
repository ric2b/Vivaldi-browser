// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_GPU_DATA_SOURCE_IPC_DATA_SOURCE_IMPL_H_
#define PLATFORM_MEDIA_GPU_DATA_SOURCE_IPC_DATA_SOURCE_IMPL_H_

#include "platform_media/common/feature_toggles.h"

#include "base/memory/shared_memory.h"
#include "base/synchronization/lock.h"
#include "platform_media/gpu/data_source/ipc_data_source.h"

#ifndef NDEBUG
#include "base/files/memory_mapped_file.h"
#endif //NDEBUG

namespace IPC {
class Sender;
}

namespace media {

// An IPCDataSource that satisfies read requests with data obtained via IPC
// from the render process.
class IPCDataSourceImpl : public IPCDataSource {
 public:
  // Passing a value <0 for |size| indicates the size is unknown (|GetSize()|
  // will return false).
  IPCDataSourceImpl(IPC::Sender* channel,
                    int32_t routing_id,
                    int64_t size,
                    bool streaming);
  ~IPCDataSourceImpl() override;

  // IPCDataSource implementation.
  void Suspend() override;
  void Resume() override;

  // DataSource implementation.
  void Read(int64_t position,
            int size,
            uint8_t* data,
            const ReadCB& read_cb) override;
  void Stop() override;
  void Abort() override;
  bool GetSize(int64_t* size_out) override;
  bool IsStreaming() override;
  void SetBitrate(int bitrate) override;

  void OnBufferForRawDataReady(size_t buffer_size,
                               base::SharedMemoryHandle handle);
  void OnRawDataReady(int size);

 private:
  IPC::Sender* const channel_;
  const int32_t routing_id_;

  const int64_t size_;
  const bool streaming_;

  class ReadOperation;
  std::unique_ptr<ReadOperation> read_operation_;

  bool stopped_;
  bool suspended_;
  bool should_discard_next_buffer_;

  // Used to protect access to |read_operation_|, |stopped_|, |suspended_|, and
  // |should_discard_next_buffer_|.
  base::Lock lock_;

  // A buffer for raw media data, shared with the render process.  Filled in the
  // render process, consumed in the GPU process.
  std::unique_ptr<base::SharedMemory> shared_data_;

#ifndef NDEBUG
  int64_t content_log_size_;
  base::FilePath content_log_path_;
  std::unique_ptr<base::MemoryMappedFile> content_log_;
#endif //NDEBUG

  DISALLOW_COPY_AND_ASSIGN(IPCDataSourceImpl);
};

}  // namespace media

#endif  // PLATFORM_MEDIA_GPU_DATA_SOURCE_IPC_DATA_SOURCE_IMPL_H_
