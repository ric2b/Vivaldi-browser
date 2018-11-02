// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is based on original work developed by Opera Software ASA.


#ifndef PLATFORM_MEDIA_RENDERER_PIPELINE_IPC_PIPELINE_DATA_H_
#define PLATFORM_MEDIA_RENDERER_PIPELINE_IPC_PIPELINE_DATA_H_

#include "platform_media/common/feature_toggles.h"

#include "platform_media/common/platform_media_pipeline_types.h"

#include "platform_media/renderer/pipeline/ipc_pipeline_source.h"

#include <deque>
#include <memory>

namespace gpu {
  class GpuChannelHost;
}

namespace media {

class DecoderBuffer;

class IPCPipelineData {
public:
  explicit IPCPipelineData(DataSource* data_source,
                           gpu::GpuChannelHost* channel);
  ~IPCPipelineData(); // override;

  // Decoded API
  bool PrepareDecoded(PlatformMediaDataType type, size_t requested_size);
  bool IsSufficientDecoded(PlatformMediaDataType type, int needed_size) const;
  base::SharedMemoryHandle HandleDecoded(PlatformMediaDataType type) const;
  uint8_t* MemoryDecoded(PlatformMediaDataType type) const;
  int MappedSizeDecoded(PlatformMediaDataType type) const;

  // Raw API
  bool PrepareRaw(size_t requested_size);
  bool IsSufficientRaw(int needed_size) const;
  base::SharedMemoryHandle HandleRaw() const;
  uint8_t* MemoryRaw() const;
  int MappedSizeRaw() const;

  // Source API
  void AppendBuffer(const scoped_refptr<DecoderBuffer>& buffer,
                    const VideoDecoder::DecodeCB& decode_cb);
  bool HasEnoughData();
  int GetMaxDecodeBuffers();
  bool GetSizeSource(int64_t* size_out);
  bool IsStreamingSource();
  void StopSource();
  void ReadFromSource(int64_t position,
                      int size,
                      uint8_t* data,
                      const DataSource::ReadCB& read_cb);

private:
  class SharedData;

  std::unique_ptr<IPCPipelineSource> data_source_;

  // A buffer for raw media data, shared with the GPU process. Filled in the
  // renderer process, consumed in the GPU process.
  std::unique_ptr<SharedData> shared_raw_data_;

  // Buffers for decoded media data, shared with the GPU process. Filled in
  // the GPU process, consumed in the renderer process.
  std::unique_ptr<SharedData>
      shared_decoded_data_[PlatformMediaDataType::PLATFORM_MEDIA_DATA_TYPE_COUNT];
};

}  // namespace media

#endif  // PLATFORM_MEDIA_RENDERER_PIPELINE_IPC_PIPELINE_DATA_H_
