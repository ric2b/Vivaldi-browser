// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is based on original work developed by Opera Software ASA.


#ifndef PLATFORM_MEDIA_RENDERER_PIPELINE_IPC_PIPELINE_DATA_SOURCE_H_
#define PLATFORM_MEDIA_RENDERER_PIPELINE_IPC_PIPELINE_DATA_SOURCE_H_

#include "platform_media/common/feature_toggles.h"

#include "platform_media/renderer/pipeline/ipc_pipeline_source.h"

namespace media {

class DecoderBuffer;
class IPCPipelineDataSource : public IPCPipelineSource {
public:
  explicit IPCPipelineDataSource(DataSource* data_source);
  ~IPCPipelineDataSource() override;
  void AppendBuffer(const scoped_refptr<DecoderBuffer>& buffer,
                    const VideoDecoder::DecodeCB& decode_cb) override;
  bool GetSizeSource(int64_t* size_out) override;
  bool HasEnoughData() override;
  int GetMaxDecodeBuffers() override;
  bool IsStreamingSource() override;
  void StopSource() override;
  void ReadFromSource(const int64_t position,
                      const int size,
                      uint8_t* data,
                      const DataSource::ReadCB& read_cb) override;
private:
  DataSource* data_source_;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_RENDERER_PIPELINE_IPC_PIPELINE_DATA_SOURCE_H_
