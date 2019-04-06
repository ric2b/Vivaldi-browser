// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is based on original work developed by Opera Software ASA.

#ifndef PLATFORM_MEDIA_TEST_TEST_IPC_DATA_SOURCE_H_
#define PLATFORM_MEDIA_TEST_TEST_IPC_DATA_SOURCE_H_

#include "platform_media/common/feature_toggles.h"

#include "platform_media/gpu/data_source/ipc_data_source.h"
#include "platform_media/renderer/pipeline/ipc_pipeline_source.h"

namespace media {

class TestIPCDataSource : public IPCDataSource {
 public:
  explicit TestIPCDataSource(DataSource* data_source);
  ~TestIPCDataSource() override;

  void Suspend() override {}
  void Resume() override {}
  void Read(int64_t position,
            int size,
            uint8_t* data,
            const ReadCB& read_cb) override;
  void Stop() override;
  void Abort() override;
  bool GetSize(int64_t* size_out) override;
  bool IsStreaming() override;
  void SetBitrate(int bitrate) override;

  void AppendBuffer(const scoped_refptr<DecoderBuffer>& buffer,
                    const VideoDecoder::DecodeCB& decode_cb);

 private:
  std::unique_ptr<IPCPipelineSource> data_source_;
};

}

#endif  // PLATFORM_MEDIA_TEST_TEST_IPC_DATA_SOURCE_H_
