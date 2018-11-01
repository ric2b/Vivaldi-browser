// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef CONTENT_COMMON_GPU_MEDIA_IPC_DATA_SOURCE_H_
#define CONTENT_COMMON_GPU_MEDIA_IPC_DATA_SOURCE_H_

#include "media/base/data_source.h"

namespace content {

// A DataSource that can be suspended and resumed.
class IPCDataSource : public media::DataSource {
 public:
  enum { kReadInterrupted = -2 };

  // Suspend and resume the data source.  While an IPCDataSource is suspended,
  // all reads return |kReadInterrupted|.
  virtual void Suspend() = 0;
  virtual void Resume() = 0;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_IPC_DATA_SOURCE_H_
