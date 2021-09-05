// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_GPU_DATA_SOURCE_IPC_DATA_SOURCE_H_
#define PLATFORM_MEDIA_GPU_DATA_SOURCE_IPC_DATA_SOURCE_H_

#include "platform_media/common/feature_toggles.h"

#include "base/callback_forward.h"
#include "base/macros.h"

#include <stddef.h>
#include <stdint.h>
#include <string>

namespace ipc_data_source {

constexpr int kReadError = -1;

using ReadCB = base::OnceCallback<void(int read_size, const uint8_t* data)>;

// Perform asynchronous read. The Run() method must not be called again until
// the callback returns. Run() must be called from the main thread and the
// callback will be called also from this thread. The callback can be called
// before the method returns if there is cached data or on errors.
using Reader = base::RepeatingCallback<
    void(int64_t position, int size, ReadCB read_cb)>;

struct Info {
  Info() = default;
  Info(Info&&) = default;
  ~Info() = default;
  Info& operator=(Info&&) = default;

  bool is_streaming = false;

  // A negative value means the size is not known.
  int64_t size = -1;

  std::string mime_type;

  DISALLOW_COPY_AND_ASSIGN(Info);
};

}  // namespace ipc_data_source

#endif  // PLATFORM_MEDIA_GPU_DATA_SOURCE_IPC_DATA_SOURCE_H_
