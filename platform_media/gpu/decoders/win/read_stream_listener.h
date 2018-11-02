// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2013 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_GPU_DECODERS_WIN_READ_STREAM_LISTENER_H_
#define PLATFORM_MEDIA_GPU_DECODERS_WIN_READ_STREAM_LISTENER_H_

#include "platform_media/common/feature_toggles.h"

namespace media {

class ReadStreamListener {
public:
  virtual void OnReadData(int size) = 0;
};

}

#endif // PLATFORM_MEDIA_GPU_DECODERS_WIN_READ_STREAM_LISTENER_H_
