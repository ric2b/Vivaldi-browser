// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_RENDERER_PIPELINE_IPC_MEDIA_PIPELINE_HOST_H_
#define PLATFORM_MEDIA_RENDERER_PIPELINE_IPC_MEDIA_PIPELINE_HOST_H_

#include "platform_media/common/feature_toggles.h"

#include "platform_media/common/platform_media.mojom.h"
#include "platform_media/common/platform_media_pipeline_types.h"

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/base/demuxer_stream.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

#include <string>

struct MediaPipelineMsg_DecodedDataReady_Params;

namespace base {
class SequencedTaskRunner;
}

namespace media {
class DataSource;
}  // namespace media

namespace media {

class IPCMediaPipelineHost : public platform_media::mojom::PipelineDataSource {
 public:
  IPCMediaPipelineHost();
  ~IPCMediaPipelineHost() override;

  static bool IsAvailable();

  DataSource* data_source() const { return data_source_; }
  int bitrate() const { return bitrate_; }
  PlatformMediaTimeInfo time_info() const { return time_info_; }
  const PlatformAudioConfig& audio_config() const { return audio_config_; }
  const PlatformVideoConfig& video_config() const { return video_config_; }

  using InitializeCB = base::OnceCallback<void(bool success)>;
  void Initialize(media::DataSource* data_source,
                  std::string mimetype,
                  InitializeCB callback);
  void StartWaitingForSeek();
  void Seek(base::TimeDelta time, PipelineStatusCallback status_cb);
  void ReadDecodedData(PlatformStreamType stream_type,
                       DemuxerStream::ReadCB read_cb);

 private:
  void DisconnectHandler();

  void OnInitialized(platform_media::mojom::PipelineInitResultPtr result);

  void DecodedVideoReady(DemuxerStream::Status status,
                         scoped_refptr<DecoderBuffer> buffer);

  void OnDecodedDataReady(PlatformStreamType stream_type,
                          platform_media::mojom::DecodingResultPtr result);

  void OnSeekDone(bool success);

  void OnReadRawDataFinished(base::WritableSharedMemoryMapping mapping,
                             ReadRawDataCallback callback,
                             int size);

  // PipelineDataSource

  void ReadRawData(int64_t offset,
                   int32_t size,
                   ReadRawDataCallback callback) override;

  static bool g_available;

  // Owner of this instances also owns the source.
  DataSource* data_source_ = nullptr;

  int bitrate_ = 0;
  PlatformMediaTimeInfo time_info_;
  PlatformAudioConfig audio_config_;
  PlatformVideoConfig video_config_;

  // A buffer for raw media data, shared with the GPU process. Filled in the
  // renderer process, consumed in the GPU process.
  base::WritableSharedMemoryMapping raw_mapping_;

  // Cached buffers for decoded media data, shared with the GPU process. Filled
  // in the GPU process, consumed in the renderer process.
  base::ReadOnlySharedMemoryMapping decoded_mappings_[kPlatformStreamTypeCount];

  InitializeCB init_callback_;
  PipelineStatusCallback seek_callback_;
  DemuxerStream::ReadCB decoded_data_read_callbacks_[kPlatformStreamTypeCount];
  bool reading_raw_data_ = false;

  mojo::Remote<platform_media::mojom::Pipeline> remote_pipeline_;
  mojo::Receiver<platform_media::mojom::PipelineDataSource> receiver_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<IPCMediaPipelineHost> weak_ptr_factory_{this};
};

}  // namespace media

#endif  // PLATFORM_MEDIA_RENDERER_PIPELINE_IPC_MEDIA_PIPELINE_HOST_H_
