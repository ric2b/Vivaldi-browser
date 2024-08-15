// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_stream_impl.h"

#include <string_view>

#include "quiche/common/platform/api/quiche_iovec.h"
#include "util/osp_logging.h"
#include "util/trace_logging.h"

namespace openscreen::osp {

QuicStreamImpl::QuicStreamImpl(Delegate& delegate,
                               quic::QuicStreamId id,
                               quic::QuicSession* session,
                               quic::StreamType type)
    : osp::QuicStream(delegate),
      quic::QuicStream(id, session, /*is_static*/ false, type) {}

QuicStreamImpl::~QuicStreamImpl() = default;

uint64_t QuicStreamImpl::GetStreamId() {
  TRACE_SCOPED(TraceCategory::kQuic, "QuicStreamImpl::StreamId");
  return id();
}

void QuicStreamImpl::Write(const ByteView& bytes) {
  TRACE_SCOPED(TraceCategory::kQuic, "QuicStreamImpl::Write");
  OSP_CHECK(!write_side_closed());
  WriteOrBufferData(
      std::string_view(reinterpret_cast<const char*>(bytes.data()),
                       bytes.size()),
      false, nullptr);
}

void QuicStreamImpl::CloseWriteEnd() {
  TRACE_SCOPED(TraceCategory::kQuic, "QuicStreamImpl::CloseWriteEnd");
  CloseWriteSide();
}

void QuicStreamImpl::OnDataAvailable() {
  TRACE_SCOPED(TraceCategory::kQuic, "QuicStreamImpl::OnDataAvailable");
  iovec iov;
  while (!reading_stopped() && sequencer()->GetReadableRegion(&iov)) {
    OSP_CHECK(!sequencer()->IsClosed());
    delegate_.OnReceived(
        this,
        ByteView(reinterpret_cast<const uint8_t*>(iov.iov_base), iov.iov_len));
    sequencer()->MarkConsumed(iov.iov_len);
  }
}

void QuicStreamImpl::OnClose() {
  TRACE_SCOPED(TraceCategory::kQuic, "QuicStreamImpl::OnClose");
  quic::QuicStream::OnClose();
  delegate_.OnClose(GetStreamId());
}

}  // namespace openscreen::osp
