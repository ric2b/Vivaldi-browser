// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_STREAM_MANAGER_H_
#define OSP_IMPL_QUIC_QUIC_STREAM_MANAGER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "osp/impl/quic/quic_connection.h"
#include "osp/impl/quic/quic_protocol_connection.h"
#include "osp/impl/quic/quic_stream.h"

namespace openscreen::osp {

struct ServiceStreamPair {
  QuicStream* stream = nullptr;
  QuicProtocolConnection* protocol_connection = nullptr;
};

// There is one instance of this class per QuicConnectionImpl instance, see
// ServiceConnectionData. The responsibility of this class is to manage all
// QuicStreams for the corresponding QuicConnection.
class QuicStreamManager final : public QuicStream::Delegate {
 public:
  class Delegate : public QuicProtocolConnection::Owner {
   public:
    ~Delegate() override = default;

    virtual void OnDataReceived(uint64_t instance_id,
                                uint64_t protocol_connection_id,
                                ByteView bytes) = 0;
    virtual void OnClose(uint64_t instance_id,
                         uint64_t protocol_connection_id) = 0;
  };

  explicit QuicStreamManager(Delegate& delegate);
  QuicStreamManager(const QuicStreamManager&) = delete;
  QuicStreamManager& operator=(const QuicStreamManager&) = delete;
  QuicStreamManager(QuicStreamManager&&) noexcept = delete;
  QuicStreamManager& operator=(QuicStreamManager&&) noexcept = delete;
  ~QuicStreamManager() override;

  // QuicStream::Delegate overrides.
  void OnReceived(QuicStream* stream, ByteView bytes) override;
  void OnClose(uint64_t stream_id) override;

  std::unique_ptr<QuicProtocolConnection> OnIncomingStream(QuicStream* stream);
  void AddStreamPair(const ServiceStreamPair& stream_pair);
  // This is called when `connection` is about to be destroyed. However, the
  // underlying QuicStream of `connection` is still working. So we should not
  // remove the corresponding item from `streams_`.
  // As a comparison, OnClose is called when a underlying QuicStream is about to
  // be closed. So we should remove the corresponding item from `streams_`.
  void DropProtocolConnection(QuicProtocolConnection& connection);

  void set_quic_connection(QuicConnection* quic_connection) {
    quic_connection_ = quic_connection;
  }

 private:
  Delegate& delegate_;
  // This class manages all QuicStreams for `quic_connection_`;
  QuicConnection* quic_connection_ = nullptr;
  std::map<uint64_t, ServiceStreamPair> streams_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_STREAM_MANAGER_H_
