// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
//
// This file is an original work developed by Vivaldi Technologies AS.

#include "platform_media/gpu/decoders/mac/avf_media_reader_warmup.h"

#include "platform_media/gpu/decoders/mac/avf_media_reader.h"

#include "media/base/data_source.h"
#include "media/filters/file_data_source.h"

namespace media {
  void InitializeAVFMediaReader() {
    std::unique_ptr<DataSource> data_source_;
    data_source_.reset(new FileDataSource());

    std::unique_ptr<AVFMediaReader> reader_;
    reader_.reset(new AVFMediaReader(dispatch_get_current_queue()));
    reader_->Initialize(data_source_.get(), "video/mp4");
  }
}
