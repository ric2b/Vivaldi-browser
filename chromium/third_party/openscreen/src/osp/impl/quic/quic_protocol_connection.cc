// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_protocol_connection.h"

#include <utility>

#include "osp/impl/quic/quic_connection.h"
#include "osp/impl/quic/quic_stream.h"
#include "osp/impl/quic/quic_stream_manager.h"
#include "util/osp_logging.h"

namespace openscreen::osp {

// static
std::unique_ptr<QuicProtocolConnection> QuicProtocolConnection::FromExisting(
    Owner& owner,
    QuicConnection& connection,
    QuicStreamManager& manager,
    uint64_t instance_id) {
  OSP_VLOG << "QUIC stream created for instance " << instance_id;
  QuicStream* stream = connection.MakeOutgoingStream(manager);
  auto pc = std::make_unique<QuicProtocolConnection>(
      owner, *stream, instance_id, stream->GetStreamId());
  manager.AddStreamPair(ServiceStreamPair{stream, pc->id(), pc.get()});
  return pc;
}

QuicProtocolConnection::QuicProtocolConnection(Owner& owner,
                                               QuicStream& stream,
                                               uint64_t instance_id,
                                               uint64_t connection_id)
    : ProtocolConnection(instance_id, connection_id),
      owner_(owner),
      stream_(&stream) {}

QuicProtocolConnection::~QuicProtocolConnection() {
  if (stream_) {
    stream_->CloseWriteEnd();
    owner_.OnConnectionDestroyed(*this);
    stream_ = nullptr;
  }
}

void QuicProtocolConnection::Write(ByteView bytes) {
  if (stream_) {
    stream_->Write(bytes);
  }
}

void QuicProtocolConnection::CloseWriteEnd() {
  if (stream_) {
    stream_->CloseWriteEnd();
  }
}

void QuicProtocolConnection::OnClose() {
  stream_ = nullptr;
  if (observer_) {
    observer_->OnConnectionClosed(*this);
  }
}

}  // namespace openscreen::osp
