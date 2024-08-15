// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_STREAM_H_
#define OSP_IMPL_QUIC_QUIC_STREAM_H_

#include <cstddef>
#include <cstdint>

#include "platform/base/span.h"

namespace openscreen::osp {

class QuicStream {
 public:
  class Delegate {
   public:
    virtual void OnReceived(QuicStream* stream, const ByteView& bytes) = 0;
    virtual void OnClose(uint64_t stream_id) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  explicit QuicStream(Delegate& delegate) : delegate_(delegate) {}
  virtual ~QuicStream() = default;

  virtual uint64_t GetStreamId() = 0;
  virtual void Write(const ByteView& bytes) = 0;
  virtual void CloseWriteEnd() = 0;

 protected:
  Delegate& delegate_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_STREAM_H_
