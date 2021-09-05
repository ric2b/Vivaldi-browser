// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
//
// This file is an original work developed by Vivaldi Technologies AS.

#include "platform_media/gpu/decoders/mac/avf_media_reader_warmup.h"

#include "platform_media/gpu/pipeline/mac/avf_media_reader_runner.h"

namespace media {

void InitializeAVFMediaReader() {
  AVFMediaReaderRunner::WarmUp();
}

}  // namespace media
