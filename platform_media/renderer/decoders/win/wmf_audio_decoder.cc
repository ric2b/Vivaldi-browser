// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/renderer/decoders/win/wmf_audio_decoder.h"

namespace media {

WMFAudioDecoder::WMFAudioDecoder(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : impl_(task_runner) {
}

WMFAudioDecoder::~WMFAudioDecoder() {
}

std::string WMFAudioDecoder::GetDisplayName() const {
  return "WMFAudioDecoder";
}

void WMFAudioDecoder::Initialize(const AudioDecoderConfig& config,
                                 CdmContext* cdm_context,
                                 const InitCB& init_cb,
                                 const OutputCB& output_cb,
                                 const WaitingForDecryptionKeyCB& waiting_for_decryption_key_cb) {
  impl_.Initialize(config, init_cb, output_cb);
}

void WMFAudioDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                             const DecodeCB& decode_cb) {
  impl_.Decode(buffer, decode_cb);
}

void WMFAudioDecoder::Reset(const base::Closure& closure) {
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  impl_.Reset(closure);
}

}  // namespace media
