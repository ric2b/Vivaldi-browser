// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_stream_manager.h"

#include <memory>
#include <utility>

#include "util/osp_logging.h"

namespace openscreen::osp {

QuicStreamManager::QuicStreamManager(Delegate& delegate)
    : delegate_(delegate) {}

QuicStreamManager::~QuicStreamManager() {
  OSP_CHECK(streams_.empty());
}

void QuicStreamManager::OnReceived(QuicStream* stream, ByteView bytes) {
  auto stream_entry = streams_.find(stream->GetStreamId());
  if (stream_entry == streams_.end()) {
    return;
  }

  delegate_.OnDataReceived(quic_connection_->instance_id(),
                           stream->GetStreamId(), bytes);
}

void QuicStreamManager::OnClose(uint64_t stream_id) {
  OSP_VLOG << "QUIC stream is closed for instance "
           << quic_connection_->instance_name();
  auto stream_entry = streams_.find(stream_id);
  if (stream_entry == streams_.end()) {
    return;
  }

  ServiceStreamPair& stream_pair = stream_entry->second;
  delegate_.OnClose(quic_connection_->instance_id(), stream_id);
  if (stream_pair.protocol_connection) {
    stream_pair.protocol_connection->OnClose();
  }
  streams_.erase(stream_entry);
}

std::unique_ptr<QuicProtocolConnection> QuicStreamManager::OnIncomingStream(
    QuicStream* stream) {
  OSP_VLOG << "Incoming QUIC stream from instance "
           << quic_connection_->instance_name();
  auto protocol_connection = std::make_unique<QuicProtocolConnection>(
      delegate_, *stream, quic_connection_->instance_id());
  AddStreamPair(ServiceStreamPair{stream, protocol_connection.get()});
  return protocol_connection;
}

void QuicStreamManager::AddStreamPair(const ServiceStreamPair& stream_pair) {
  const uint64_t stream_id = stream_pair.stream->GetStreamId();
  streams_.emplace(stream_id, stream_pair);
}

void QuicStreamManager::DropProtocolConnection(
    QuicProtocolConnection& connection) {
  auto stream_entry = streams_.find(connection.GetID());
  if (stream_entry == streams_.end()) {
    return;
  }

  stream_entry->second.protocol_connection = nullptr;
}

}  // namespace openscreen::osp
