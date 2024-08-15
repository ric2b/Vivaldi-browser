// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_client.h"

#include <algorithm>
#include <functional>
#include <memory>

#include "platform/api/task_runner.h"
#include "platform/api/time.h"
#include "util/osp_logging.h"

namespace openscreen::osp {

QuicClient::QuicClient(
    const EndpointConfig& config,
    MessageDemuxer& demuxer,
    std::unique_ptr<QuicConnectionFactoryClient> connection_factory,
    ProtocolConnectionServiceObserver& observer,
    ClockNowFunctionPtr now_function,
    TaskRunner& task_runner)
    : ProtocolConnectionClient(demuxer, observer),
      connection_factory_(std::move(connection_factory)),
      connection_endpoints_(config.connection_endpoints),
      cleanup_alarm_(now_function, task_runner) {}

QuicClient::~QuicClient() {
  CloseAllConnections();
}

bool QuicClient::Start() {
  if (state_ == State::kRunning)
    return false;
  state_ = State::kRunning;
  Cleanup();  // Start periodic clean-ups.
  observer_.OnRunning();
  return true;
}

bool QuicClient::Stop() {
  if (state_ == State::kStopped)
    return false;
  CloseAllConnections();
  state_ = State::kStopped;
  Cleanup();  // Final clean-up.
  observer_.OnStopped();
  return true;
}

void QuicClient::Cleanup() {
  for (auto& entry : connections_) {
    entry.second.delegate->DestroyClosedStreams();
    if (!entry.second.delegate->has_streams())
      entry.second.connection->Close();
  }

  for (uint64_t endpoint_id : delete_connections_) {
    auto it = connections_.find(endpoint_id);
    if (it != connections_.end()) {
      connections_.erase(it);
    }
  }
  delete_connections_.clear();

  constexpr Clock::duration kQuicCleanupPeriod = std::chrono::milliseconds(500);
  if (state_ != State::kStopped) {
    cleanup_alarm_.ScheduleFromNow([this] { Cleanup(); }, kQuicCleanupPeriod);
  }
}

QuicClient::ConnectRequest QuicClient::Connect(
    const IPEndpoint& endpoint,
    ConnectionRequestCallback* request) {
  if (state_ != State::kRunning)
    return ConnectRequest(this, 0);
  auto endpoint_entry = endpoint_map_.find(endpoint);
  if (endpoint_entry != endpoint_map_.end()) {
    auto immediate_result = CreateProtocolConnection(endpoint_entry->second);
    OSP_CHECK(immediate_result);
    request->OnConnectionOpened(0, std::move(immediate_result));
    return ConnectRequest(this, 0);
  }

  return CreatePendingConnection(endpoint, request);
}

std::unique_ptr<ProtocolConnection> QuicClient::CreateProtocolConnection(
    uint64_t endpoint_id) {
  if (state_ != State::kRunning)
    return nullptr;
  auto connection_entry = connections_.find(endpoint_id);
  if (connection_entry == connections_.end())
    return nullptr;
  return QuicProtocolConnection::FromExisting(
      *this, connection_entry->second.connection.get(),
      connection_entry->second.delegate.get(), endpoint_id);
}

void QuicClient::OnConnectionDestroyed(QuicProtocolConnection* connection) {
  if (!connection->stream())
    return;

  auto connection_entry = connections_.find(connection->endpoint_id());
  if (connection_entry == connections_.end())
    return;

  connection_entry->second.delegate->DropProtocolConnection(connection);
}

uint64_t QuicClient::OnCryptoHandshakeComplete(
    ServiceConnectionDelegate* delegate,
    std::string connection_id) {
  const IPEndpoint& endpoint = delegate->endpoint();
  auto pending_entry = pending_connections_.find(endpoint);
  if (pending_entry == pending_connections_.end())
    return 0;

  ServiceConnectionData connection_data = std::move(pending_entry->second.data);
  auto* connection = connection_data.connection.get();
  uint64_t endpoint_id = next_endpoint_id_++;
  endpoint_map_[endpoint] = endpoint_id;
  connections_.emplace(endpoint_id, std::move(connection_data));

  for (auto& request : pending_entry->second.callbacks) {
    request_map_.erase(request.first);
    std::unique_ptr<QuicProtocolConnection> pc =
        QuicProtocolConnection::FromExisting(*this, connection, delegate,
                                             endpoint_id);
    request_map_.erase(request.first);
    request.second->OnConnectionOpened(request.first, std::move(pc));
  }
  pending_connections_.erase(pending_entry);
  return endpoint_id;
}

void QuicClient::OnIncomingStream(
    std::unique_ptr<QuicProtocolConnection> connection) {
  // TODO(jophba): Change to just use OnIncomingConnection when the observer
  // is properly set up.
  connection->CloseWriteEnd();
  connection.reset();
}

void QuicClient::OnConnectionClosed(uint64_t endpoint_id,
                                    std::string connection_id) {
  // TODO(btolsch): Is this how handshake failure is communicated to the
  // delegate?
  auto connection_entry = connections_.find(endpoint_id);
  if (connection_entry == connections_.end())
    return;
  delete_connections_.push_back(endpoint_id);

  // TODO(crbug.com/openscreen/42): If we reset request IDs when a connection is
  // closed, we might end up re-using request IDs when a new connection is
  // created to the same endpoint.
  endpoint_request_ids_.ResetRequestId(endpoint_id);
}

void QuicClient::OnDataReceived(uint64_t endpoint_id,
                                uint64_t protocol_connection_id,
                                const ByteView& bytes) {
  demuxer_.OnStreamData(endpoint_id, protocol_connection_id, bytes.data(),
                        bytes.size());
}

QuicClient::PendingConnectionData::PendingConnectionData(
    ServiceConnectionData&& data)
    : data(std::move(data)) {}
QuicClient::PendingConnectionData::PendingConnectionData(
    PendingConnectionData&&) noexcept = default;
QuicClient::PendingConnectionData::~PendingConnectionData() = default;
QuicClient::PendingConnectionData& QuicClient::PendingConnectionData::operator=(
    PendingConnectionData&&) noexcept = default;

void QuicClient::OnStarted() {}
void QuicClient::OnStopped() {}
void QuicClient::OnSuspended() {}
void QuicClient::OnSearching() {}

void QuicClient::OnReceiverAdded(const ServiceInfo& info) {
  fingerprints_.emplace(
      info.v4_endpoint.port ? info.v4_endpoint : info.v6_endpoint,
      info.fingerprint);
}

void QuicClient::OnReceiverChanged(const ServiceInfo& info) {
  fingerprints_[info.v4_endpoint.port ? info.v4_endpoint : info.v6_endpoint] =
      info.fingerprint;
}

void QuicClient::OnReceiverRemoved(const ServiceInfo& info) {
  fingerprints_.erase(info.v4_endpoint.port ? info.v4_endpoint
                                            : info.v6_endpoint);
}

void QuicClient::OnAllReceiversRemoved() {
  fingerprints_.clear();
}

void QuicClient::OnError(const Error&) {}
void QuicClient::OnMetrics(ServiceListener::Metrics) {}

QuicClient::ConnectRequest QuicClient::CreatePendingConnection(
    const IPEndpoint& endpoint,
    ConnectionRequestCallback* request) {
  auto pending_entry = pending_connections_.find(endpoint);
  if (pending_entry == pending_connections_.end()) {
    uint64_t request_id = StartConnectionRequest(endpoint, request);
    return ConnectRequest(this, request_id);
  } else {
    uint64_t request_id = next_request_id_++;
    pending_entry->second.callbacks.emplace_back(request_id, request);
    return ConnectRequest(this, request_id);
  }
}

uint64_t QuicClient::StartConnectionRequest(
    const IPEndpoint& endpoint,
    ConnectionRequestCallback* request) {
  auto fingerprint_entry = fingerprints_.find(endpoint);
  if (fingerprint_entry == fingerprints_.end()) {
    request->OnConnectionFailed(0);
    OSP_LOG_ERROR
        << "QuicClient connect failed: can't find usable fingerprint.";
    return 0;
  }

  auto delegate = std::make_unique<ServiceConnectionDelegate>(*this, endpoint);
  ErrorOr<std::unique_ptr<QuicConnection>> connection =
      connection_factory_->Connect(connection_endpoints_[0], endpoint,
                                   fingerprint_entry->second, delegate.get());
  if (!connection) {
    request->OnConnectionFailed(0);
    OSP_LOG_ERROR << "Factory connect failed: " << connection.error();
    return 0;
  }
  auto pending_result = pending_connections_.emplace(
      endpoint, PendingConnectionData(ServiceConnectionData(
                    std::move(connection.value()), std::move(delegate))));
  uint64_t request_id = next_request_id_++;
  pending_result.first->second.callbacks.emplace_back(request_id, request);
  return request_id;
}

void QuicClient::CloseAllConnections() {
  for (auto& conn : pending_connections_)
    conn.second.data.connection->Close();

  pending_connections_.clear();
  for (auto& conn : connections_)
    conn.second.connection->Close();

  connections_.clear();
  endpoint_map_.clear();
  next_endpoint_id_ = 0;
  endpoint_request_ids_.Reset();
  for (auto& request : request_map_) {
    request.second.second->OnConnectionFailed(request.first);
  }
  request_map_.clear();
}

void QuicClient::CancelConnectRequest(uint64_t request_id) {
  auto request_entry = request_map_.find(request_id);
  if (request_entry == request_map_.end())
    return;

  auto pending_entry = pending_connections_.find(request_entry->second.first);
  if (pending_entry != pending_connections_.end()) {
    auto& callbacks = pending_entry->second.callbacks;
    callbacks.erase(
        std::remove_if(
            callbacks.begin(), callbacks.end(),
            [request_id](const std::pair<uint64_t, ConnectionRequestCallback*>&
                             callback) {
              return request_id == callback.first;
            }),
        callbacks.end());
    if (callbacks.empty())
      pending_connections_.erase(pending_entry);
  }
  request_map_.erase(request_entry);
}

}  // namespace openscreen::osp
