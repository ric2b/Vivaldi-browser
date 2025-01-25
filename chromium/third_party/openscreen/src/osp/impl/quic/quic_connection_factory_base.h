// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_CONNECTION_FACTORY_BASE_H_
#define OSP_IMPL_QUIC_QUIC_CONNECTION_FACTORY_BASE_H_

#include <map>
#include <memory>

#include "osp/impl/quic/quic_connection.h"
#include "platform/api/task_runner.h"
#include "platform/api/udp_socket.h"
#include "platform/base/ip_address.h"
#include "quiche/quic/core/deterministic_connection_id_generator.h"
#include "quiche/quic/core/quic_alarm_factory.h"
#include "quiche/quic/core/quic_config.h"
#include "quiche/quic/core/quic_connection.h"
#include "quiche/quic/core/quic_versions.h"

namespace openscreen::osp {

// This class holds the common functionality for QuicConnectionFactoryClient and
// QuicConnectionFactoryServer.
class QuicConnectionFactoryBase : public UdpSocket::Client {
 public:
  struct OpenConnection {
    QuicConnection* connection = nullptr;
    UdpSocket* socket = nullptr;  // References one of the owned `sockets_`.
  };

  explicit QuicConnectionFactoryBase(TaskRunner& task_runner);
  QuicConnectionFactoryBase(const QuicConnectionFactoryBase&) = delete;
  QuicConnectionFactoryBase& operator=(const QuicConnectionFactoryBase&) =
      delete;
  QuicConnectionFactoryBase(QuicConnectionFactoryBase&&) noexcept = delete;
  QuicConnectionFactoryBase& operator=(QuicConnectionFactoryBase&&) noexcept =
      delete;
  virtual ~QuicConnectionFactoryBase();

  // UdpSocket::Client overrides.
  void OnError(UdpSocket* socket, const Error& error) override;
  void OnSendError(UdpSocket* socket, const Error& error) override;

  // This is called when the `connection` is totally closed (The underlying
  // QUIC implementation should have completed the connection close process
  // after waiting for an event loop). We can delete related socket at this time
  // if needed.
  virtual void OnConnectionClosed(QuicConnection* connection) = 0;

  std::map<IPEndpoint, OpenConnection>& connection() { return connections_; }

 protected:
  std::unique_ptr<quic::QuicConnectionHelperInterface> helper_;
  std::unique_ptr<quic::QuicAlarmFactory> alarm_factory_;
  quic::ParsedQuicVersionVector supported_versions_{
      quic::ParsedQuicVersion::RFCv1()};
  quic::DeterministicConnectionIdGenerator connection_id_generator_{
      /*expected_connection_id_length=*/0u};
  quic::QuicConfig config_;

  std::map<IPEndpoint, OpenConnection> connections_;

  TaskRunner& task_runner_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_CONNECTION_FACTORY_BASE_H_
