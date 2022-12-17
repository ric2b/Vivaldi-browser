// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_IPC_DEMUXER_GPU_DATA_SOURCE_IPC_DATA_SOURCE_H_
#define PLATFORM_MEDIA_IPC_DEMUXER_GPU_DATA_SOURCE_IPC_DATA_SOURCE_H_

#include "base/callback_forward.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "media/base/media_export.h"

#include <stddef.h>
#include <stdint.h>
#include <string>

namespace ipc_data_source {

constexpr int kReadError = -1;

class Buffer;

using ReadCB = base::OnceCallback<void(Buffer buffer)>;

using Reader = base::RepeatingCallback<void(Buffer buffer)>;

// Move-only class to cache the shared region holding the result of the
// previouis IPC read operation while allowing lock-free access from media
// decoding threads to the received data.
class MEDIA_EXPORT Buffer {
 public:
  Buffer();
  Buffer(Buffer&& buffer);
  ~Buffer();

  // The move assignment is only allowed if this is not initialized or was moved
  // out.
  Buffer& operator=(Buffer&& buffer);

  void Init(base::ReadOnlySharedMemoryMapping mapping, Reader source_reader);

  operator bool() const { return !is_null(); }

  // Check for not initialized or moved out buffer.
  bool is_null() const { return !impl_; }

  int GetCapacity();

  // Set start and size of the read request.
  void SetReadRange(int64_t position, int size);
  int64_t GetReadPosition() const;
  int GetRequestedSize() const;

  // Perform an asynchronous read. The method consumes this instance. It will be
  // handed back to the caller as a callback argument. The method must not be
  // called again until the callback returns. Read() must be called from the
  // main thread and the callback will be called also from this thread. The
  // callback can be called before the method returns if there is cached data or
  // on errors.
  static void Read(Buffer buffer, ReadCB read_cb);

  // Deliver the buffer with new data back to the callback passed to Read(). The
  // method consumes the instance.
  static bool OnRawDataRead(int read_size, Buffer buffer);

  static void OnRawDataError(Buffer buffer) {
    OnRawDataRead(kReadError, std::move(buffer));
  }

  bool IsReadError() const;
  int GetReadSize() const;
  int64_t GetLastReadEnd() const;
  const uint8_t* GetReadData() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// Move-only struct with information about the data source.
struct Info {
  bool is_streaming = false;

  // A negative value means the size is not known.
  int64_t size = -1;

  std::string mime_type;

  // The shared memory buffer to use for IPC.
  Buffer buffer;
};

}  // namespace ipc_data_source

#endif  // PLATFORM_MEDIA_IPC_DEMUXER_GPU_DATA_SOURCE_IPC_DATA_SOURCE_H_
