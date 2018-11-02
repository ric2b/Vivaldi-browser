// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_GPU_DATA_SOURCE_IPC_DATA_SOURCE_H_
#define PLATFORM_MEDIA_GPU_DATA_SOURCE_IPC_DATA_SOURCE_H_

#include "platform_media/common/feature_toggles.h"

#include "media/base/data_source.h"

namespace media {

// A DataSource that can be suspended and resumed.
class IPCDataSource : public DataSource {
 public:
  enum { kReadInterrupted = -2 };

  // Suspend and resume the data source.  While an IPCDataSource is suspended,
  // all reads return |kReadInterrupted|.
  virtual void Suspend() = 0;
  virtual void Resume() = 0;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_GPU_DATA_SOURCE_IPC_DATA_SOURCE_H_
