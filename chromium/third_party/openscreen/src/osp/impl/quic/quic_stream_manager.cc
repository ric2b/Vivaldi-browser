// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_stream_manager.h"

#include <memory>
#include <utility>

#include "util/osp_logging.h"

namespace openscreen::osp {

QuicStreamManager::Delegate::Delegate() = default;
QuicStreamManager::Delegate::~Delegate() = default;

QuicStreamManager::QuicStreamManager(Delegate& delegate)
    : delegate_(delegate) {}

QuicStreamManager::~QuicStreamManager() = default;

void QuicStreamManager::OnReceived(QuicStream* stream, ByteView bytes) {
  auto stream_entry = streams_by_id_.find(stream->GetStreamId());
  if (stream_entry == streams_by_id_.end()) {
    return;
  }

  delegate_.OnDataReceived(quic_connection_->instance_id(),
                           stream->GetStreamId(), bytes);
}

void QuicStreamManager::OnClose(uint64_t stream_id) {
  OSP_VLOG << "QUIC stream is closed for instance "
           << quic_connection_->instance_name();
  auto stream_entry = streams_by_id_.find(stream_id);
  if (stream_entry == streams_by_id_.end()) {
    return;
  }

  delegate_.OnClose(quic_connection_->instance_id(), stream_id);
  auto* protocol_connection = stream_entry->second;
  if (protocol_connection) {
    protocol_connection->OnClose();
  }
  streams_by_id_.erase(stream_entry);
}

std::unique_ptr<QuicProtocolConnection> QuicStreamManager::OnIncomingStream(
    QuicStream* stream) {
  OSP_VLOG << "Incoming QUIC stream from instance "
           << quic_connection_->instance_name();
  auto protocol_connection = std::make_unique<QuicProtocolConnection>(
      stream, quic_connection_->instance_id());
  AddStream(*protocol_connection);
  return protocol_connection;
}

void QuicStreamManager::AddStream(QuicProtocolConnection& protocol_connection) {
  streams_by_id_.emplace(protocol_connection.GetID(), &protocol_connection);
}

}  // namespace openscreen::osp
