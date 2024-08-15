// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/testing/fake_quic_connection_factory.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "platform/base/span.h"
#include "util/osp_logging.h"

namespace openscreen::osp {

FakeQuicConnectionFactoryBridge::FakeQuicConnectionFactoryBridge(
    const IPEndpoint& controller_endpoint)
    : controller_endpoint_(controller_endpoint) {}

void FakeQuicConnectionFactoryBridge::OnConnectionClosed(
    QuicConnection* connection) {
  if (connection == connections_.controller) {
    connections_.controller = nullptr;
    return;
  } else if (connection == connections_.receiver) {
    connections_.receiver = nullptr;
    return;
  }
  OSP_CHECK(false) << "reporting an unknown connection as closed";
}

void FakeQuicConnectionFactoryBridge::OnOutgoingStream(
    QuicConnection* connection,
    QuicStream* stream) {
  FakeQuicConnection* remote_connection = nullptr;
  if (connection == connections_.controller) {
    remote_connection = connections_.receiver;
  } else if (connection == connections_.receiver) {
    remote_connection = connections_.controller;
  }

  if (remote_connection) {
    remote_connection->delegate().OnIncomingStream(
        remote_connection->id(), remote_connection->MakeIncomingStream());
  }
}

void FakeQuicConnectionFactoryBridge::SetServerDelegate(
    QuicConnectionFactoryServer::ServerDelegate* delegate,
    const IPEndpoint& endpoint) {
  OSP_CHECK(!delegate_ || !delegate);
  delegate_ = delegate;
  receiver_endpoint_ = endpoint;
}

void FakeQuicConnectionFactoryBridge::RunTasks(bool is_client) {
  bool* idle_flag = is_client ? &client_idle_ : &server_idle_;
  *idle_flag = true;

  if (!connections_.controller || !connections_.receiver) {
    return;
  }

  if (connections_pending_) {
    *idle_flag = false;
    connections_.receiver->delegate().OnCryptoHandshakeComplete(
        connections_.receiver->id());
    connections_.controller->delegate().OnCryptoHandshakeComplete(
        connections_.controller->id());
    connections_pending_ = false;
    return;
  }

  const size_t num_streams = connections_.controller->streams().size();
  OSP_CHECK_EQ(num_streams, connections_.receiver->streams().size());

  auto stream_it_pair =
      std::make_pair(connections_.controller->streams().begin(),
                     connections_.receiver->streams().begin());

  for (size_t i = 0; i < num_streams; ++i) {
    auto* controller_stream = stream_it_pair.first->second.get();
    auto* receiver_stream = stream_it_pair.second->second.get();

    std::vector<uint8_t> written_data = controller_stream->TakeWrittenData();
    OSP_CHECK(controller_stream->TakeReceivedData().empty());

    if (!written_data.empty()) {
      *idle_flag = false;
      receiver_stream->delegate().OnReceived(
          receiver_stream, ByteView(written_data.data(), written_data.size()));
    }

    written_data = receiver_stream->TakeWrittenData();
    OSP_CHECK(receiver_stream->TakeReceivedData().empty());

    if (written_data.size()) {
      *idle_flag = false;
      controller_stream->delegate().OnReceived(
          controller_stream,
          ByteView(written_data.data(), written_data.size()));
    }

    // Close the read end for closed write ends
    if (controller_stream->write_end_closed()) {
      receiver_stream->CloseReadEnd();
    }
    if (receiver_stream->write_end_closed()) {
      controller_stream->CloseReadEnd();
    }

    if (controller_stream->both_ends_closed() &&
        receiver_stream->both_ends_closed()) {
      controller_stream->delegate().OnClose(controller_stream->GetStreamId());
      receiver_stream->delegate().OnClose(receiver_stream->GetStreamId());

      controller_stream->delegate().OnReceived(controller_stream,
                                               ByteView(nullptr, size_t(0)));
      receiver_stream->delegate().OnReceived(receiver_stream,
                                             ByteView(nullptr, size_t(0)));

      stream_it_pair.first =
          connections_.controller->streams().erase(stream_it_pair.first);
      stream_it_pair.second =
          connections_.receiver->streams().erase(stream_it_pair.second);
    } else {
      // The stream pair must always be advanced at the same time.
      ++stream_it_pair.first;
      ++stream_it_pair.second;
    }
  }
}

ErrorOr<std::unique_ptr<QuicConnection>>
FakeQuicConnectionFactoryBridge::Connect(
    const IPEndpoint& endpoint,
    QuicConnection::Delegate* connection_delegate) {
  if (endpoint != receiver_endpoint_) {
    return Error::Code::kParameterInvalid;
  }

  OSP_CHECK(!connections_.controller);
  OSP_CHECK(!connections_.receiver);
  auto controller_connection = std::make_unique<FakeQuicConnection>(
      *this, std::to_string(next_connection_id_++), *connection_delegate);
  connections_.controller = controller_connection.get();

  auto receiver_connection = std::make_unique<FakeQuicConnection>(
      *this, std::to_string(next_connection_id_++),
      *delegate_->NextConnectionDelegate(controller_endpoint_));
  connections_.receiver = receiver_connection.get();
  delegate_->OnIncomingConnection(std::move(receiver_connection));
  return ErrorOr<std::unique_ptr<QuicConnection>>(
      std::move(controller_connection));
}

FakeClientQuicConnectionFactory::FakeClientQuicConnectionFactory(
    TaskRunner& task_runner,
    FakeQuicConnectionFactoryBridge* bridge)
    : QuicConnectionFactoryClient(task_runner), bridge_(bridge) {}
FakeClientQuicConnectionFactory::~FakeClientQuicConnectionFactory() = default;

ErrorOr<std::unique_ptr<QuicConnection>>
FakeClientQuicConnectionFactory::Connect(
    const IPEndpoint& local_endpoint,
    const IPEndpoint& remote_endpoint,
    const std::string& fingerprint,
    QuicConnection::Delegate* connection_delegate) {
  return bridge_->Connect(remote_endpoint, connection_delegate);
}

void FakeClientQuicConnectionFactory::OnError(UdpSocket* socket,
                                              const Error& error) {
  OSP_UNIMPLEMENTED();
}

void FakeClientQuicConnectionFactory::OnSendError(UdpSocket* socket,
                                                  const Error& error) {
  OSP_UNIMPLEMENTED();
}

void FakeClientQuicConnectionFactory::OnRead(UdpSocket* socket,
                                             ErrorOr<UdpPacket> packet) {
  bridge_->RunTasks(true);
  idle_ = bridge_->client_idle();
}

FakeServerQuicConnectionFactory::FakeServerQuicConnectionFactory(
    TaskRunner& task_runner,
    FakeQuicConnectionFactoryBridge* bridge)
    : QuicConnectionFactoryServer(task_runner), bridge_(bridge) {}
FakeServerQuicConnectionFactory::~FakeServerQuicConnectionFactory() = default;

void FakeServerQuicConnectionFactory::SetServerDelegate(
    ServerDelegate* delegate,
    const std::vector<IPEndpoint>& endpoints) {
  if (delegate) {
    OSP_CHECK_EQ(1u, endpoints.size())
        << "fake bridge doesn't support multiple server endpoints";
  }
  bridge_->SetServerDelegate(delegate,
                             endpoints.empty() ? IPEndpoint{} : endpoints[0]);
}

void FakeServerQuicConnectionFactory::OnError(UdpSocket* socket,
                                              const Error& error) {
  OSP_UNIMPLEMENTED();
}

void FakeServerQuicConnectionFactory::OnSendError(UdpSocket* socket,
                                                  const Error& error) {
  OSP_UNIMPLEMENTED();
}

void FakeServerQuicConnectionFactory::OnRead(UdpSocket* socket,
                                             ErrorOr<UdpPacket> packet) {
  bridge_->RunTasks(false);
  idle_ = bridge_->server_idle();
}

}  // namespace openscreen::osp
