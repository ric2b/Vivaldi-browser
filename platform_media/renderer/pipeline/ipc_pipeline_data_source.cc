// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is based on original work developed by Opera Software ASA.

#include "platform_media/renderer/pipeline/ipc_pipeline_data_source.h"

namespace media {

void IPCPipelineDataSource::AppendBuffer(
    const scoped_refptr<DecoderBuffer>& buffer,
    const VideoDecoder::DecodeCB& decode_cb) {
  NOTIMPLEMENTED();
}

bool IPCPipelineDataSource::GetSizeSource(int64_t* size_out) {
  return data_source_->GetSize(size_out);
}

bool IPCPipelineDataSource::HasEnoughData() {
  NOTIMPLEMENTED();
  return true;
}

int IPCPipelineDataSource::GetMaxDecodeBuffers() {
  NOTIMPLEMENTED();
  return 0;
}

bool IPCPipelineDataSource::IsStreamingSource() {
  return data_source_->IsStreaming();
}

void IPCPipelineDataSource::StopSource()  {
  data_source_->Stop();
}

IPCPipelineDataSource::IPCPipelineDataSource(DataSource* data_source)
  : data_source_(data_source)
{
  DCHECK(data_source_);
}

IPCPipelineDataSource::~IPCPipelineDataSource() {

}

void IPCPipelineDataSource::ReadFromSource(
    const int64_t position,
    const int size,
    uint8_t* data,
    const DataSource::ReadCB& read_cb)  {
  data_source_->Read(position, size, data, read_cb);
}

}
