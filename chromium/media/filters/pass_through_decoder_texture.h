// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef MEDIA_FILTERS_PASS_THROUGH_DECODER_TEXTURE_H_
#define MEDIA_FILTERS_PASS_THROUGH_DECODER_TEXTURE_H_

#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/video_frame.h"

namespace media {

struct MEDIA_EXPORT PassThroughDecoderTexture {
  PassThroughDecoderTexture();
  ~PassThroughDecoderTexture();

  uint32_t texture_id;
  scoped_ptr<gpu::MailboxHolder> mailbox_holder;
  VideoFrame::ReleaseMailboxCB mailbox_holder_release_cb;

 private:
  DISALLOW_COPY_AND_ASSIGN(PassThroughDecoderTexture);
};

// Ensures that texture described by PassThroughDecoderTexture object is
// properly released if it doesn't reach its final user.
class MEDIA_EXPORT AutoReleasedPassThroughDecoderTexture {
 public:
  explicit AutoReleasedPassThroughDecoderTexture(
      scoped_ptr<PassThroughDecoderTexture> texture);
  ~AutoReleasedPassThroughDecoderTexture();

  scoped_ptr<PassThroughDecoderTexture> Pass() {
    DCHECK(texture_);
    return std::move(texture_);
  }

 private:
  scoped_ptr<PassThroughDecoderTexture> texture_;

  DISALLOW_COPY_AND_ASSIGN(AutoReleasedPassThroughDecoderTexture);
};

}  // namespace media

#endif  // MEDIA_FILTERS_PASS_THROUGH_DECODER_TEXTURE_H_
