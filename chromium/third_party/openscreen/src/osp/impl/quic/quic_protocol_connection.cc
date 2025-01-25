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
    QuicConnection& connection,
    QuicStreamManager& manager,
    uint64_t instance_id) {
  OSP_VLOG << "QUIC stream created for instance " << instance_id;
  QuicStream* stream = connection.MakeOutgoingStream(manager);
  auto pc = std::make_unique<QuicProtocolConnection>(stream, instance_id);
  manager.AddStream(*pc);
  return pc;
}

QuicProtocolConnection::QuicProtocolConnection(QuicStream* stream,
                                               uint64_t instance_id)
    : instance_id_(instance_id), stream_(stream) {}

QuicProtocolConnection::~QuicProtocolConnection() {
  // When this is destroyed, if there is still a underlying QuicStream serving
  // this, we should close it and OnClose will be triggered before this function
  // completes.
  if (stream_) {
    stream_->Close();
  }
}

uint64_t QuicProtocolConnection::GetID() const {
  return stream_ ? stream_->GetStreamId() : 0;
}

void QuicProtocolConnection::Write(ByteView bytes) {
  if (stream_) {
    stream_->Write(bytes);
  }
}

void QuicProtocolConnection::Close() {
  if (stream_) {
    stream_->Close();
  }
}

void QuicProtocolConnection::OnClose() {
  // This is called when the underlying QuicStream is closed. `observer_` can be
  // notified by OnConnectionClosed interface and it can delete this instance at
  // this time. Otherwise, this instance will exist without a underlying
  // QuicStream serving it.
  stream_ = nullptr;
  if (observer_) {
    observer_->OnConnectionClosed(*this);
  }
}

}  // namespace openscreen::osp
