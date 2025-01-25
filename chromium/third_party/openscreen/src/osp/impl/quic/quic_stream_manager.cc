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
  DestroyClosedStreams();
  OSP_CHECK(streams_.empty());
}

void QuicStreamManager::OnReceived(QuicStream* stream, ByteView bytes) {
  auto stream_entry = streams_.find(stream->GetStreamId());
  if (stream_entry == streams_.end()) {
    return;
  }

  delegate_.OnDataReceived(quic_connection_->instance_id(),
                           stream_entry->second.protocol_connection_id, bytes);
}

void QuicStreamManager::OnClose(uint64_t stream_id) {
  OSP_VLOG << "QUIC stream closed for instance "
           << quic_connection_->instance_name();
  auto stream_entry = streams_.find(stream_id);
  if (stream_entry == streams_.end()) {
    return;
  }

  ServiceStreamPair& stream_pair = stream_entry->second;
  delegate_.OnClose(quic_connection_->instance_id(),
                    stream_pair.protocol_connection_id);
  if (stream_pair.protocol_connection) {
    stream_pair.protocol_connection->OnClose();
  }
  // NOTE: If this OnClose is the result of the read end closing when the write
  // end was already closed, there will likely still be a call to OnReceived.
  // We need to delay actually destroying the stream object until the end of the
  // event loop.
  closed_streams_.push_back(std::move(stream_entry->second));
  streams_.erase(stream_entry);
}

std::unique_ptr<QuicProtocolConnection> QuicStreamManager::OnIncomingStream(
    QuicStream* stream) {
  OSP_VLOG << "Incoming QUIC stream from instance "
           << quic_connection_->instance_name();
  auto protocol_connection = std::make_unique<QuicProtocolConnection>(
      delegate_, *stream, quic_connection_->instance_id(),
      stream->GetStreamId());
  AddStreamPair(ServiceStreamPair{stream, protocol_connection->id(),
                                  protocol_connection.get()});
  return protocol_connection;
}

void QuicStreamManager::AddStreamPair(const ServiceStreamPair& stream_pair) {
  const uint64_t stream_id = stream_pair.stream->GetStreamId();
  streams_.emplace(stream_id, stream_pair);
}

void QuicStreamManager::DropProtocolConnection(
    QuicProtocolConnection& connection) {
  auto stream_entry = streams_.find(connection.stream()->GetStreamId());
  if (stream_entry == streams_.end()) {
    return;
  }

  stream_entry->second.protocol_connection = nullptr;
}

void QuicStreamManager::DestroyClosedStreams() {
  closed_streams_.clear();
}

}  // namespace openscreen::osp
