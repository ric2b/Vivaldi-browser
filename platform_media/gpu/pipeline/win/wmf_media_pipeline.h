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

class WMFByteStream;

class WMFMediaPipeline : public PlatformMediaPipeline {
 public:
  WMFMediaPipeline(
      DataSource* data_source,
      const AudioConfigChangedCB& audio_config_changed_cb,
      const VideoConfigChangedCB& video_config_changed_cb);
  ~WMFMediaPipeline() override;

  void Initialize(const std::string& mime_type,
                  const InitializeCB& initialize_cb) override;
  void ReadAudioData(const ReadDataCB& read_audio_data_cb) override;
  void ReadVideoData(const ReadDataCB& read_video_data_cb) override;
  void WillSeek() override {}
  void Seek(base::TimeDelta time, const SeekCB& seek_cb) override;

 private:
  class AudioTimestampCalculator;

  void InitializeDone(bool success,
                      int bitrate,
                      PlatformMediaTimeInfo time_info,
                      PlatformAudioConfig audio_config,
                      PlatformVideoConfig video_config);
  void ReadDataDone(PlatformMediaDataType type,
                    scoped_refptr<DataBuffer> decoded_data);
  void SeekDone(bool success);

  void AudioConfigChanged(PlatformAudioConfig audio_config);
  void VideoConfigChanged(PlatformVideoConfig video_config);

  AudioConfigChangedCB audio_config_changed_cb_;
  VideoConfigChangedCB video_config_changed_cb_;
  InitializeCB initialize_cb_;
  ReadDataCB read_audio_data_cb_;
  ReadDataCB read_video_data_cb_;
  SeekCB seek_cb_;

  scoped_refptr<WMFByteStream> byte_stream_;

  class ThreadedImpl {
   public:
    explicit ThreadedImpl(DataSource* data_source);
    ~ThreadedImpl();

    void Initialize(base::WeakPtr<WMFMediaPipeline> wmf_media_pipeline,
                    scoped_refptr<WMFByteStream> byte_stream,
                    const std::string& mime_type);
    void ReadData(PlatformMediaDataType type);
    void Seek(base::TimeDelta time);

   private:
    bool CreateSourceReaderCallbackAndAttributes(
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
    scoped_refptr<DataBuffer> CreateDataBuffer(
        IMFSample* sample,
        PlatformMediaDataType media_type);
    scoped_refptr<DataBuffer> CreateDataBufferFromMemory(IMFSample* sample);

    base::WeakPtr<WMFMediaPipeline> wmf_media_pipeline_;
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
    DataSource* data_source_;
    Microsoft::WRL::ComPtr<IMFSourceReaderCallback> source_reader_callback_;
    platform_media::SourceReaderWorker source_reader_worker_;

    DWORD
    stream_indices_[PlatformMediaDataType::PLATFORM_MEDIA_DATA_TYPE_COUNT];

    std::unique_ptr<AudioTimestampCalculator> audio_timestamp_calculator_;

    scoped_refptr<DataBuffer> pending_decoded_data_
        [PlatformMediaDataType::PLATFORM_MEDIA_DATA_TYPE_COUNT];

    // See |WMFDecoderImpl::get_stride_function_|.
    decltype(MFGetStrideForBitmapInfoHeader)* get_stride_function_;

    SEQUENCE_CHECKER(sequence_checker_);
    base::WeakPtrFactory<ThreadedImpl> weak_ptr_factory_;
  };
  friend class ThreadedImpl;
  std::unique_ptr<ThreadedImpl> threaded_impl_;

  base::Thread media_pipeline_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> media_pipeline_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<WMFMediaPipeline> weak_ptr_factory_;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_GPU_PIPELINE_WIN_WMF_MEDIA_PIPELINE_H_
