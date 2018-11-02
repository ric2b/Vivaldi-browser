// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is based on original work developed by Opera Software ASA.

#ifndef PLATFORM_MEDIA_RENDERER_PIPELINE_IPC_PIPELINE_SOURCE_H_
#define PLATFORM_MEDIA_RENDERER_PIPELINE_IPC_PIPELINE_SOURCE_H_

#include "platform_media/common/feature_toggles.h"

#include "media/base/data_source.h"
#include "media/base/video_decoder.h"

namespace media {

class DecoderBuffer;

class IPCPipelineSource {
public:
  virtual ~IPCPipelineSource() {}
  virtual void AppendBuffer(const scoped_refptr<DecoderBuffer>& buffer,
                            const VideoDecoder::DecodeCB& decode_cb) = 0;
  virtual bool GetSizeSource(int64_t* size_out) = 0;
  virtual bool HasEnoughData() = 0;
  virtual int GetMaxDecodeBuffers() = 0;
  virtual bool IsStreamingSource() = 0;
  virtual void StopSource() = 0;
  virtual void ReadFromSource(const int64_t position,
                              const int size,
                              uint8_t* data,
                              const DataSource::ReadCB& read_cb) = 0;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_RENDERER_PIPELINE_IPC_PIPELINE_SOURCE_H_
