// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is based on original work developed by Opera Software ASA.

#include "platform_media/test/test_ipc_data_source.h"

#include "platform_media/renderer/pipeline/ipc_pipeline_data_source.h"

namespace media {

TestIPCDataSource::~TestIPCDataSource() = default;

TestIPCDataSource::TestIPCDataSource(DataSource* data_source)
      : data_source_(new IPCPipelineDataSource(data_source)) {}

void TestIPCDataSource::Read(int64_t position,
                             int size,
                             uint8_t* data,
                             const ReadCB& read_cb) {
  data_source_->ReadFromSource(position, size, data, read_cb);
}

void TestIPCDataSource::Stop() {
  NOTIMPLEMENTED();
}

void TestIPCDataSource::Abort() {
  NOTIMPLEMENTED();
}

bool TestIPCDataSource::GetSize(int64_t* size_out) {
  return data_source_->GetSizeSource(size_out);
}

bool TestIPCDataSource::IsStreaming() {
  return data_source_->IsStreamingSource();
}

void TestIPCDataSource::SetBitrate(int bitrate) {
  NOTIMPLEMENTED();
}

void TestIPCDataSource::AppendBuffer(const scoped_refptr<DecoderBuffer>& buffer,
                                     const VideoDecoder::DecodeCB& decode_cb) {
  data_source_->AppendBuffer(buffer, decode_cb);
}

}
