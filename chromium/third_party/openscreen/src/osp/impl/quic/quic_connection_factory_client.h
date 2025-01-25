// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_CONNECTION_FACTORY_CLIENT_H_
#define OSP_IMPL_QUIC_QUIC_CONNECTION_FACTORY_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "osp/impl/quic/quic_connection_factory_base.h"
#include "platform/base/error.h"
#include "quiche/quic/core/crypto/quic_crypto_client_config.h"

namespace openscreen::osp {

class QuicConnectionFactoryClient : public QuicConnectionFactoryBase {
 public:
  struct ConnectData {
    std::string instance_name;
    std::string fingerprint;
  };

  explicit QuicConnectionFactoryClient(TaskRunner& task_runner);
  ~QuicConnectionFactoryClient() override;

  // UdpSocket::Client overrides.
  void OnRead(UdpSocket* socket, ErrorOr<UdpPacket> packet) override;

  // QuicConnectionFactoryBase overrides.
  void OnConnectionClosed(QuicConnection* connection) override;

  virtual ErrorOr<std::unique_ptr<QuicConnection>> Connect(
      const IPEndpoint& local_endpoint,
      const IPEndpoint& remote_endpoint,
      const ConnectData& connect_data,
      QuicConnection::Delegate* connection_delegate);

 private:
  std::vector<std::unique_ptr<UdpSocket>> sockets_;
  std::unique_ptr<quic::QuicCryptoClientConfig> crypto_client_config_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_CONNECTION_FACTORY_CLIENT_H_
