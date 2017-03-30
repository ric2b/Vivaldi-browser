// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef CONTENT_COMMON_GPU_MEDIA_WMF_MEDIA_PIPELINE_H_
#define CONTENT_COMMON_GPU_MEDIA_WMF_MEDIA_PIPELINE_H_

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

#include <string>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/win/scoped_comptr.h"
#include "content/common/gpu/media/platform_media_pipeline.h"
#include "content/common/gpu/media/wmf_byte_stream.h"
#include "media/filters/platform_media_pipeline_types.h"

namespace content {

class WMFByteStream;

class WMFMediaPipeline : public PlatformMediaPipeline {
 public:
  WMFMediaPipeline(
      media::DataSource* data_source,
      const AudioConfigChangedCB& audio_config_changed_cb,
      const VideoConfigChangedCB& video_config_changed_cb,
      media::PlatformMediaDecodingMode preferred_video_decoding_mode,
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
  class DXVAPictureBuffer;
  struct InitializationResult;

  // Caller doesn't become an owner of object pointed-to by return value.
  static EGLConfig GetEGLConfig(
      const MakeGLContextCurrentCB& make_gl_context_current_cb);
  static InitializationResult CreateSourceReader(
      const scoped_refptr<WMFByteStream>& byte_stream,
      const base::win::ScopedComPtr<IMFAttributes>& attributes,
      media::PlatformMediaDecodingMode preferred_decoding_mode);
  static bool CreateDXVASourceReader(
      const scoped_refptr<WMFByteStream>& byte_stream,
      const base::win::ScopedComPtr<IMFAttributes>& attributes,
      InitializationResult* result);

  bool CreateSourceReaderCallbackAndAttributes(
      base::win::ScopedComPtr<IMFAttributes>* attributes);

  bool InitializeImpl(const std::string& mime_type,
                      const InitializeCB& initialize_cb);
  void FinalizeInitialization(const InitializeCB& initialize_cb,
                              const InitializationResult& result);
  bool RetrieveStreamIndices();
  bool ConfigureStream(DWORD stream_index);
  bool ConfigureSourceReader();
  bool HasMediaStream(media::PlatformMediaDataType type) const;
  void SetNoMediaStream(media::PlatformMediaDataType type);
  base::TimeDelta GetDuration();
  int GetBitrate(base::TimeDelta duration);
  bool GetStride(int* stride);
  bool GetAudioDecoderConfig(media::PlatformAudioConfig* audio_config);
  bool GetVideoDecoderConfig(media::PlatformVideoConfig* video_config);
  void OnReadSample(media::MediaDataStatus status,
                    DWORD stream_index,
                    const base::win::ScopedComPtr<IMFSample>& sample);
  scoped_refptr<media::DataBuffer> CreateDataBuffer(
      IMFSample* sample,
      media::PlatformMediaDataType media_type);
  scoped_refptr<media::DataBuffer> CreateDataBufferFromMemory(
      IMFSample* sample);
  scoped_refptr<media::DataBuffer> CreateDataBufferFromTexture(
      IMFSample* sample);

  // Caller doesn't become an owner of object pointed-to by return value.
  DXVAPictureBuffer* GetDXVAPictureBuffer(uint32_t texture_id);

  media::DataSource* data_source_;
  scoped_refptr<WMFByteStream> byte_stream_;
  base::win::ScopedComPtr<IMFSourceReaderCallback> source_reader_callback_;
  base::win::ScopedComPtr<IMFSourceReader> source_reader_;

  AudioConfigChangedCB audio_config_changed_cb_;
  VideoConfigChangedCB video_config_changed_cb_;

  base::Thread source_reader_creation_thread_;

  DWORD stream_indices_[media::PLATFORM_MEDIA_DATA_TYPE_COUNT];

  GUID input_video_subtype_guid_;

  scoped_ptr<AudioTimestampCalculator> audio_timestamp_calculator_;

  media::PlatformVideoConfig video_config_;
  GUID source_reader_output_video_format_;

  MakeGLContextCurrentCB make_gl_context_current_cb_;
  EGLConfig egl_config_;
  scoped_ptr<Direct3DContext> direct3d_context_;
  DXVAPictureBuffer* current_dxva_picture_buffer_;
  base::ScopedPtrHashMap<uint32_t, scoped_ptr<DXVAPictureBuffer>>
      known_picture_buffers_;

  ReadDataCB read_audio_data_cb_;
  ReadDataCB read_video_data_cb_;

  scoped_refptr<media::DataBuffer>
      pending_decoded_data_[media::PLATFORM_MEDIA_DATA_TYPE_COUNT];

  // See |WMFDecoderImpl::get_stride_function_|.
  decltype(MFGetStrideForBitmapInfoHeader)* get_stride_function_;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<WMFMediaPipeline> weak_ptr_factory_;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_WMF_MEDIA_PIPELINE_H_
