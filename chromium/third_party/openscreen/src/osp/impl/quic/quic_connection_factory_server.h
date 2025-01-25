// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_CONNECTION_FACTORY_SERVER_H_
#define OSP_IMPL_QUIC_QUIC_CONNECTION_FACTORY_SERVER_H_

#include <memory>
#include <utility>
#include <vector>

#include "osp/impl/quic/quic_connection_factory_base.h"
#include "quiche/quic/core/crypto/quic_crypto_server_config.h"

namespace openscreen::osp {

class QuicDispatcherImpl;

class QuicConnectionFactoryServer : public QuicConnectionFactoryBase {
 public:
  class ServerDelegate {
   public:
    virtual ~ServerDelegate() = default;

    virtual QuicConnection::Delegate& GetConnectionDelegate() = 0;
    virtual void OnIncomingConnection(
        std::unique_ptr<QuicConnection> connection) = 0;
  };

  explicit QuicConnectionFactoryServer(TaskRunner& task_runner);
  ~QuicConnectionFactoryServer() override;

  // UdpSocket::Client overrides.
  void OnRead(UdpSocket* socket, ErrorOr<UdpPacket> packet) override;

  // QuicConnectionFactoryBase overrides.
  void OnConnectionClosed(QuicConnection* connection) override;

  virtual void SetServerDelegate(ServerDelegate* delegate,
                                 const std::vector<IPEndpoint>& endpoints);
  ServerDelegate* server_delegate() { return server_delegate_; }

 private:
  std::unique_ptr<quic::QuicCryptoServerConfig> crypto_server_config_;
  ServerDelegate* server_delegate_ = nullptr;

  // New entry is added when an UdpSocket is created and the corresponding
  // QuicDispatcherImpl is responsible for processing UDP packets.
  // An entry is removed when no remaining connections reference the UdpSocket
  // and the UdpSocket is closed.
  std::vector<std::pair<std::unique_ptr<UdpSocket>,
                        std::unique_ptr<QuicDispatcherImpl>>>
      dispatchers_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_CONNECTION_FACTORY_SERVER_H_
