// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_STREAM_IMPL_H_
#define OSP_IMPL_QUIC_QUIC_STREAM_IMPL_H_

#include "osp/impl/quic/quic_stream.h"
#include "quiche/quic/core/quic_stream.h"

namespace openscreen::osp {

class QuicStreamImpl final : public QuicStream, public quic::QuicStream {
 public:
  QuicStreamImpl(Delegate& delegate,
                 quic::QuicStreamId id,
                 quic::QuicSession* session,
                 quic::StreamType type);
  ~QuicStreamImpl() override;

  // QuicStream overrides.
  uint64_t GetStreamId() override;
  void Write(ByteView bytes) override;
  void CloseWriteEnd() override;

  // quic::QuicStream overrides.
  void OnDataAvailable() override;
  void OnClose() override;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_STREAM_IMPL_H_
