// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef MEDIA_FILTERS_WMF_DECODER_IMPL_H_
#define MEDIA_FILTERS_WMF_DECODER_IMPL_H_

#include <mfapi.h>
#include <mftransform.h>

#include <deque>
#include <string>

#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "base/win/scoped_comptr.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/sample_format.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/decoder_stream_traits.h"

namespace media {

template <DemuxerStream::Type StreamType>
struct WMFDecoderImplTraits : public DecoderStreamTraits<StreamType> {};

template <>
struct WMFDecoderImplTraits<DemuxerStream::AUDIO>
    : public DecoderStreamTraits<DemuxerStream::AUDIO> {
  using DecoderConfigType = AudioDecoderConfig;
};

template <>
struct WMFDecoderImplTraits<DemuxerStream::VIDEO>
    : public DecoderStreamTraits<DemuxerStream::VIDEO> {
  using DecoderConfigType = VideoDecoderConfig;
};

// Decodes AAC audio or H.264 video streams using Windows Media Foundation
// library.
template <DemuxerStream::Type StreamType>
class WMFDecoderImpl {
 public:
  using DecoderTraits = WMFDecoderImplTraits<StreamType>;
  using DecoderConfig = typename DecoderTraits::DecoderConfigType;
  using DecoderType = typename DecoderTraits::DecoderType;
  using DecodeCB = typename DecoderType::DecodeCB;
  using InitCB = typename DecoderType::InitCB;
  using OutputCB = typename DecoderTraits::OutputCB;
  using OutputType = typename DecoderTraits::OutputType;

  explicit WMFDecoderImpl(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  void Initialize(const DecoderConfig& config,
                  const InitCB& init_cb,
                  const OutputCB& output_cb);
  void Decode(const scoped_refptr<DecoderBuffer>& buffer,
              const DecodeCB& decode_cb);
  void Reset(const base::Closure& closure);

 private:
  // Performs decoder config checks specific to the WMFDecoder,
  // beyond the generic DecoderConfig::IsValidConfig() check.
  static bool IsValidConfig(const DecoderConfig& config);
  static std::string GetModuleName();
  static GUID GetMediaObjectGUID();
  static base::win::ScopedComPtr<IMFTransform> CreateWMFDecoder();

  // Methods used for initialization and configuration.
  bool ConfigureDecoder();
  bool SetInputMediaType();
  bool SetOutputMediaType();
  HRESULT SetOutputMediaTypeInternal(GUID subtype, IMFMediaType* media_type);
  bool InitializeDecoderFunctions();

  // Methods used during decoding.
  HRESULT ProcessInput(const scoped_refptr<DecoderBuffer>& input);
  void RecordInputTimestamp(base::TimeDelta timestamp);
  HRESULT ProcessOutput();
  bool ProcessOutputLoop();
  bool Drain();
  bool PrepareInputSample(IMFSample** sample,
                          IMFMediaBuffer** buffer,
                          const scoped_refptr<DecoderBuffer>& input) const;
  scoped_refptr<OutputType> CreateOutputBuffer(
      const MFT_OUTPUT_DATA_BUFFER& output_data_buffer);
  base::TimeDelta GetOutputTimestamp(IMFSample* output);
  scoped_refptr<OutputType> CreateOutputBufferInternal(
      const uint8_t* data,
      DWORD data_size,
      base::TimeDelta timestamp);
  bool CreateSampleAndBuffer(IMFSample** sample,
                             IMFMediaBuffer** buffer,
                             DWORD buffer_size,
                             int buffer_alignment) const;
  bool IsDecoderCreatingSamples() const;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::win::ScopedComPtr<IMFTransform> decoder_;
  DecoderConfig config_;
  OutputCB output_cb_;
  MFT_INPUT_STREAM_INFO input_stream_info_;
  MFT_OUTPUT_STREAM_INFO output_stream_info_;
  uint32_t output_sample_size_;  // in Bytes

  // Used to transfer timestamps from input to output buffers when we can't
  // rely on IMFTransform timestamps.
  std::deque<base::TimeDelta> timestamps_;

  // We always call MFGetStrideForBitmapInfoHeader() through this pointer.
  // This guarantees the call succeeds both on Vista and newer systems.  On
  // Vista, the function is provided by evr.dll, but we build Opera on newer
  // Windows, where the function is provided by mfplat.dll.  We set up this
  // pointer to the function in evr.dll explicitly.  Luckily, on newer Windows
  // evr.dll still provides a stub that calls the function in mfplat.dll, so
  // this approach always works.
  decltype(MFGetStrideForBitmapInfoHeader)* get_stride_function_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_WMF_DECODER_IMPL_H_
