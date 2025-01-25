// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_TESTING_FAKE_QUIC_CONNECTION_FACTORY_H_
#define OSP_IMPL_QUIC_TESTING_FAKE_QUIC_CONNECTION_FACTORY_H_

#include <memory>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "osp/impl/quic/quic_connection_factory_client.h"
#include "osp/impl/quic/quic_connection_factory_server.h"
#include "osp/impl/quic/testing/fake_quic_connection.h"
#include "osp/public/message_demuxer.h"
#include "platform/api/task_runner.h"

namespace openscreen::osp {

class FakeQuicConnectionFactoryBridge {
 public:
  explicit FakeQuicConnectionFactoryBridge(
      const IPEndpoint& controller_endpoint);

  bool server_idle() const { return server_idle_; }
  bool client_idle() const { return client_idle_; }

  void OnConnectionClosed(QuicConnection* connection);
  void OnOutgoingStream(QuicConnection* connection, QuicStream* stream);

  void SetServerDelegate(QuicConnectionFactoryServer::ServerDelegate* delegate,
                         const IPEndpoint& endpoint);
  void RunTasks(bool is_client);
  ErrorOr<std::unique_ptr<QuicConnection>> Connect(
      const IPEndpoint& local_endpoint,
      const IPEndpoint& remote_endpoint,
      const std::string& instance_name,
      QuicConnection::Delegate* connection_delegate);

 private:
  struct ConnectionPair {
    FakeQuicConnection* controller;
    FakeQuicConnection* receiver;
  };

  const IPEndpoint controller_endpoint_;
  IPEndpoint receiver_endpoint_;
  bool client_idle_ = true;
  bool server_idle_ = true;
  bool connections_pending_ = true;
  ConnectionPair connections_ = {};
  QuicConnectionFactoryServer::ServerDelegate* delegate_ = nullptr;
};

class FakeClientQuicConnectionFactory final
    : public QuicConnectionFactoryClient {
 public:
  FakeClientQuicConnectionFactory(TaskRunner& task_runner,
                                  FakeQuicConnectionFactoryBridge* bridge);
  ~FakeClientQuicConnectionFactory() override;

  // UdpSocket::Client overrides.
  void OnError(UdpSocket* socket, const Error& error) override;
  void OnSendError(UdpSocket* socket, const Error& error) override;
  void OnRead(UdpSocket* socket, ErrorOr<UdpPacket> packet) override;

  // QuicConnectionFactoryClient overrides.
  ErrorOr<std::unique_ptr<QuicConnection>> Connect(
      const IPEndpoint& local_endpoint,
      const IPEndpoint& remote_endpoint,
      const ConnectData& connect_data,
      QuicConnection::Delegate* connection_delegate) override;
  void OnConnectionClosed(QuicConnection* connection) override;

  bool idle() const { return idle_; }

  std::unique_ptr<UdpSocket> socket_;

 private:
  FakeQuicConnectionFactoryBridge* bridge_;
  bool idle_ = true;
};

class FakeServerQuicConnectionFactory final
    : public QuicConnectionFactoryServer {
 public:
  FakeServerQuicConnectionFactory(TaskRunner& task_runner,
                                  FakeQuicConnectionFactoryBridge* bridge);
  ~FakeServerQuicConnectionFactory() override;

  // UdpSocket::Client overrides.
  void OnError(UdpSocket* socket, const Error& error) override;
  void OnSendError(UdpSocket* socket, const Error& error) override;
  void OnRead(UdpSocket* socket, ErrorOr<UdpPacket> packet) override;

  // QuicConnectionFactoryServer overrides.
  void SetServerDelegate(ServerDelegate* delegate,
                         const std::vector<IPEndpoint>& endpoints) override;
  void OnConnectionClosed(QuicConnection* connection) override;

  bool idle() const { return idle_; }

 private:
  FakeQuicConnectionFactoryBridge* bridge_;
  bool idle_ = true;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_TESTING_FAKE_QUIC_CONNECTION_FACTORY_H_
