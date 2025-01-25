// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_CONNECTION_H_
#define OSP_IMPL_QUIC_QUIC_CONNECTION_H_

#include <string>
#include <string_view>
#include <vector>

#include "osp/impl/quic/quic_stream.h"
#include "platform/base/udp_packet.h"

namespace openscreen::osp {

class QuicConnection {
 public:
  class Delegate {
   public:
    // Called when the QUIC handshake has successfully completed. After the
    // handshake, an instance ID is assigned to `instance_id_`, which can be
    // used to as an identifier to find data about this connection.
    virtual uint64_t OnCryptoHandshakeComplete(
        std::string_view instance_name) = 0;

    // Called when a new stream on this connection is initiated by the other
    // endpoint.  `stream` will use a delegate returned by GetStreamDelegate.
    virtual void OnIncomingStream(uint64_t instance_id, QuicStream* stream) = 0;

    // Called when the QUIC connection was closed.  The QuicConnection should
    // not be destroyed immediately, because the QUIC implementation will still
    // reference it briefly.  Instead, it should be destroyed during the next
    // event loop.
    // TODO(btolsch): Hopefully this can be changed with future QUIC
    // implementations.
    virtual void OnConnectionClosed(uint64_t instance_id) = 0;

    // This is used to get a QuicStream::Delegate for an incoming stream, which
    // will be returned via OnIncomingStream immediately after this call.
    virtual QuicStream::Delegate& GetStreamDelegate(uint64_t instance_id) = 0;

    // This is used to propagate client certificate to QuicServer.
    virtual void OnClientCertificates(
        std::string_view instance_name,
        const std::vector<std::string>& certs) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  QuicConnection(std::string_view instance_name, Delegate& delegate)
      : instance_name_(instance_name), delegate_(delegate) {}
  virtual ~QuicConnection() = default;

  virtual void OnPacketReceived(const UdpPacket& packet) = 0;
  virtual QuicStream* MakeOutgoingStream(QuicStream::Delegate& delegate) = 0;
  virtual void Close() = 0;

  const std::string& instance_name() { return instance_name_; }
  uint64_t instance_id() { return instance_id_; }

 protected:
  std::string instance_name_;
  uint64_t instance_id_ = 0u;
  Delegate& delegate_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_CONNECTION_H_
