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
#include <d3d9.h>  // if included before |mfidl.h| breaks <propvarutil.h>
// Work around bug in this header by disabling the relevant warning for it.
// https://connect.microsoft.com/VisualStudio/feedback/details/911260/dxva2api-h-in-win8-sdk-triggers-c4201-with-w4
#pragma warning(push)
#pragma warning(disable : 4201)
#include <dxva2api.h>  // if included before |mfidl.h| breaks <propvarutil.h>
#pragma warning(pop)
#include <mfreadwrite.h>
#include <propvarutil.h>

#include "platform_media/common/platform_media_pipeline_types.h"
#include "platform_media/gpu/decoders/win/wmf_byte_stream.h"
#include "platform_media/gpu/pipeline/platform_media_pipeline.h"
#include "platform_media/gpu/pipeline/win/source_reader_worker.h"

#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"

#include <string>
#include <unordered_map>
#include <wrl/client.h>

namespace media {

class WMFByteStream;

class WMFMediaPipeline : public PlatformMediaPipeline {
 public:
  WMFMediaPipeline(
      DataSource* data_source,
      const AudioConfigChangedCB& audio_config_changed_cb,
      const VideoConfigChangedCB& video_config_changed_cb,
      PlatformMediaDecodingMode preferred_video_decoding_mode,
      const MakeGLContextCurrentCB& make_gl_context_current_cb);
  ~WMFMediaPipeline() override;

  void Initialize(const std::string& mime_type,
                  const InitializeCB& initialize_cb) override;
  void ReadAudioData(const ReadDataCB& read_audio_data_cb) override;
  void ReadVideoData(const ReadDataCB& read_video_data_cb,
                     uint32_t texture_id) override;
  void WillSeek() override {}
  void Seek(base::TimeDelta time, const SeekCB& seek_cb) override;

 private:
  using EGLConfig = void*;

  class AudioTimestampCalculator;
  struct Direct3DContext;
#if defined(PLATFORM_MEDIA_HWA)
  class DXVAPictureBuffer;
#endif
  struct InitializationResult;

  // Caller doesn't become an owner of object pointed-to by return value.
  static EGLConfig GetEGLConfig(
      const MakeGLContextCurrentCB& make_gl_context_current_cb);
  static InitializationResult CreateSourceReader(
      const scoped_refptr<WMFByteStream>& byte_stream,
      const Microsoft::WRL::ComPtr<IMFAttributes>& attributes,
      PlatformMediaDecodingMode preferred_decoding_mode);
  static bool CreateDXVASourceReader(
      const scoped_refptr<WMFByteStream>& byte_stream,
      const Microsoft::WRL::ComPtr<IMFAttributes>& attributes,
      InitializationResult* result);

  bool CreateSourceReaderCallbackAndAttributes(
      Microsoft::WRL::ComPtr<IMFAttributes>* attributes);

  bool InitializeImpl(const std::string& mime_type,
                      const InitializeCB& initialize_cb);
  void FinalizeInitialization(const InitializeCB& initialize_cb,
                              const InitializationResult& result);
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
  scoped_refptr<DataBuffer> CreateDataBufferFromMemory(
      IMFSample* sample);

#if defined(PLATFORM_MEDIA_HWA)
  scoped_refptr<DataBuffer> CreateDataBufferFromTexture(
      IMFSample* sample);
#endif

#if defined(PLATFORM_MEDIA_HWA)
  // Caller doesn't become an owner of object pointed-to by return value.
  DXVAPictureBuffer* GetDXVAPictureBuffer(uint32_t texture_id);
#endif

  DataSource* data_source_;
  scoped_refptr<WMFByteStream> byte_stream_;
  Microsoft::WRL::ComPtr<IMFSourceReaderCallback> source_reader_callback_;
  platform_media::SourceReaderWorker source_reader_worker_;

  AudioConfigChangedCB audio_config_changed_cb_;
  VideoConfigChangedCB video_config_changed_cb_;

  base::Thread source_reader_creation_thread_;

  DWORD stream_indices_[PlatformMediaDataType::PLATFORM_MEDIA_DATA_TYPE_COUNT];

  GUID input_video_subtype_guid_;

  std::unique_ptr<AudioTimestampCalculator> audio_timestamp_calculator_;

  PlatformVideoConfig video_config_;
  GUID source_reader_output_video_format_;

#if defined(PLATFORM_MEDIA_HWA)
  MakeGLContextCurrentCB make_gl_context_current_cb_;
  EGLConfig egl_config_;
  std::unique_ptr<Direct3DContext> direct3d_context_;
  DXVAPictureBuffer* current_dxva_picture_buffer_;
  std::unordered_map<uint32_t, std::unique_ptr<DXVAPictureBuffer>>
      known_picture_buffers_;
#endif

  ReadDataCB read_audio_data_cb_;
  ReadDataCB read_video_data_cb_;

  scoped_refptr<DataBuffer>
      pending_decoded_data_[PlatformMediaDataType::PLATFORM_MEDIA_DATA_TYPE_COUNT];

  // See |WMFDecoderImpl::get_stride_function_|.
  decltype(MFGetStrideForBitmapInfoHeader)* get_stride_function_;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<WMFMediaPipeline> weak_ptr_factory_;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_GPU_PIPELINE_WIN_WMF_MEDIA_PIPELINE_H_
