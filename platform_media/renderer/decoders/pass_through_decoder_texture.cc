// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/renderer/decoders/pass_through_decoder_texture.h"

namespace media {

PassThroughDecoderTexture::PassThroughDecoderTexture() = default;
PassThroughDecoderTexture::~PassThroughDecoderTexture() = default;

AutoReleasedPassThroughDecoderTexture::AutoReleasedPassThroughDecoderTexture(
    std::unique_ptr<PassThroughDecoderTexture> texture)
    : texture_(std::move(texture)) {
  DCHECK(texture_);
}

AutoReleasedPassThroughDecoderTexture::
    ~AutoReleasedPassThroughDecoderTexture() {
  // The texture didn't reach its user, it has to be released.
  if (texture_) {
    texture_->mailbox_holder_release_cb.Run(
        texture_->mailbox_holder->sync_token);
  }
}

}  // namespace media
