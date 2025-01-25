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
    Delegate() = default;
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    Delegate(Delegate&&) noexcept = delete;
    Delegate& operator=(Delegate&&) noexcept = delete;
    virtual ~Delegate() = default;

    virtual void OnReceived(QuicStream* stream, ByteView bytes) = 0;
    virtual void OnClose(uint64_t stream_id) = 0;
  };

  explicit QuicStream(Delegate& delegate) : delegate_(delegate) {}
  QuicStream(const QuicStream&) = delete;
  QuicStream& operator=(const QuicStream&) = delete;
  QuicStream(QuicStream&&) noexcept = delete;
  QuicStream& operator=(QuicStream&&) noexcept = delete;
  virtual ~QuicStream() = default;

  virtual uint64_t GetStreamId() = 0;
  virtual void Write(ByteView bytes) = 0;
  virtual void Close() = 0;

 protected:
  Delegate& delegate_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_STREAM_H_
