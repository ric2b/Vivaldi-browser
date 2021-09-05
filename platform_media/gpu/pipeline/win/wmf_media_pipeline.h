// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_GPU_PIPELINE_WIN_WMF_MEDIA_PIPELINE_H_
#define PLATFORM_MEDIA_GPU_PIPELINE_WIN_WMF_MEDIA_PIPELINE_H_

#include "platform_media/common/feature_toggles.h"

#include <mfidl.h>
// Work around bug in this header by disabling the relevant warning for it.
// https://connect.microsoft.com/VisualStudio/feedback/details/911260/dxva2api-h-in-win8-sdk-triggers-c4201-with-w4
#pragma warning(push)
#include <mfreadwrite.h>
#include <propvarutil.h>
#include <wrl/client.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/thread.h"
#include "platform_media/common/feature_toggles.h"
#include "platform_media/common/platform_media_pipeline_types.h"
#include "platform_media/gpu/decoders/win/wmf_byte_stream.h"
#include "platform_media/gpu/pipeline/platform_media_pipeline.h"
#include "platform_media/gpu/pipeline/win/source_reader_worker.h"

namespace media {

class WMFMediaPipeline : public PlatformMediaPipeline {
 public:
  WMFMediaPipeline();
  ~WMFMediaPipeline() override;

  void Initialize(ipc_data_source::Reader source_reader,
                  ipc_data_source::Info source_info,
                  InitializeCB initialize_cb) override;
  void ReadMediaData(IPCDecodingBuffer buffer) override;
  void WillSeek() override {}
  void Seek(base::TimeDelta time, SeekCB seek_cb) override;

 private:
  class AudioTimestampCalculator;

  class ThreadedImpl {
   public:
    ThreadedImpl();
    ~ThreadedImpl();

    void Initialize(ipc_data_source::Reader ipc_source_reader,
                    ipc_data_source::Info ipc_source_info,
                    InitializeCB initialize_cb);
    void ReadData(IPCDecodingBuffer buffer);
    void Seek(base::TimeDelta time, SeekCB seek_cb);

   private:
    bool CreateSourceReaderCallbackAndAttributes(
        const std::string& mime_type,
        Microsoft::WRL::ComPtr<IMFAttributes>* attributes);

    bool RetrieveStreamIndices();
    bool ConfigureStream(DWORD stream_index);
    bool ConfigureSourceReader();
    bool HasMediaStream(PlatformMediaDataType type) const;
    void SetNoMediaStream(PlatformMediaDataType type);
    base::TimeDelta GetDuration();
    int GetBitrate(base::TimeDelta duration);
    bool GetStride(int* stride);
    bool GetAudioDecoderConfig(PlatformAudioConfig* audio_config);
    bool GetVideoDecoderConfig(PlatformVideoConfig* video_config);
    void OnReadSample(MediaDataStatus status,
                      DWORD stream_index,
                      const Microsoft::WRL::ComPtr<IMFSample>& sample);
    bool CreateDataBuffer(IMFSample* sample,
                          IPCDecodingBuffer* decoding_buffer);
    bool CreateDataBufferFromMemory(IMFSample* sample,
                                    IPCDecodingBuffer* decoding_buffer);

    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
    bool is_streaming_ = false;
    Microsoft::WRL::ComPtr<IMFSourceReaderCallback> source_reader_callback_;
    platform_media::SourceReaderWorker source_reader_worker_;

    DWORD stream_indices_[kPlatformMediaDataTypeCount];

    std::unique_ptr<AudioTimestampCalculator> audio_timestamp_calculator_;

    IPCDecodingBuffer ipc_decoding_buffers_[kPlatformMediaDataTypeCount];

    // See |WMFDecoderImpl::get_stride_function_|.
    decltype(MFGetStrideForBitmapInfoHeader)* get_stride_function_;

    SEQUENCE_CHECKER(sequence_checker_);
    base::WeakPtrFactory<ThreadedImpl> weak_ptr_factory_;
  };
  friend class ThreadedImpl;
  std::unique_ptr<ThreadedImpl> threaded_impl_;

  scoped_refptr<base::SequencedTaskRunner> media_pipeline_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // PLATFORM_MEDIA_GPU_PIPELINE_WIN_WMF_MEDIA_PIPELINE_H_
