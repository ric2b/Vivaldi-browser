// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_DECODERS_WIN_WMF_VIDEO_DECODER_H_
#define PLATFORM_MEDIA_DECODERS_WIN_WMF_VIDEO_DECODER_H_

#include <mftransform.h>
#include <wrl/client.h>

#include <string>
#include <string_view>

#include "base/task/sequenced_task_runner.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"

#include "platform_media/decoders/debug_buffer_logger.h"

namespace media {

// Decodes H.264 video streams using Windows Media Foundation library.
class WMFVideoDecoder : public VideoDecoder {
 public:
  WMFVideoDecoder(scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~WMFVideoDecoder() override;
  WMFVideoDecoder(const WMFVideoDecoder&) = delete;
  WMFVideoDecoder& operator=(const WMFVideoDecoder&) = delete;

  // VideoDecoder implementation.
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_for_decryption_key_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure closure) override;
  VideoDecoderType GetDecoderType() const override;
  bool NeedsBitstreamConversion() const override;

 private:
  bool ConfigureDecoder();
  bool SetInputMediaType();
  bool SetOutputMediaType();

  bool DoDecode(scoped_refptr<DecoderBuffer> buffer);

  // Try to extract more output from the decoder. Return false on errors. Set
  // need_more_input if the decoder cannot produce more output.
  bool ProcessOutput(bool& need_more_input);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  Microsoft::WRL::ComPtr<IMFTransform> decoder_;
  VideoDecoderConfig config_;
  OutputCB output_cb_;
  Microsoft::WRL::ComPtr<IMFSample> output_sample_;
  DWORD input_buffer_alignment_ = 0;
  LONG stride_ = 0;

  DebugBufferLogger debug_buffer_logger_;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_DECODERS_WIN_WMF_VIDEO_DECODER_H_
