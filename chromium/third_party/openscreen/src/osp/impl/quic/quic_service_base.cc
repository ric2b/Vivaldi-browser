// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_service_base.h"

#include "util/osp_logging.h"

namespace openscreen::osp {

// static
QuicAgentCertificate& QuicServiceBase::GetAgentCertificate() {
  static QuicAgentCertificate agent_certificate;
  return agent_certificate;
}

QuicServiceBase::QuicServiceBase(
    const ServiceConfig& config,
    std::unique_ptr<QuicConnectionFactoryBase> connection_factory,
    ProtocolConnectionServiceObserver& observer,
    InstanceRequestIds::Role role,
    ClockNowFunctionPtr now_function,
    TaskRunner& task_runner,
    size_t buffer_limit)
    : instance_request_ids_(role),
      demuxer_(now_function, buffer_limit),
      connection_factory_(std::move(connection_factory)),
      connection_endpoints_(config.connection_endpoints),
      now_function_(now_function),
      task_runner_(task_runner),
      observer_(observer) {}

QuicServiceBase::~QuicServiceBase() {
  CloseAllConnections();
}

uint64_t QuicServiceBase::OnCryptoHandshakeComplete(
    std::string_view instance_name) {
  if (state_ != ProtocolConnectionEndpoint::State::kRunning) {
    return 0;
  }

  auto pending_entry = pending_connections_.find(instance_name);
  if (pending_entry == pending_connections_.end()) {
    return 0;
  }

  ServiceConnectionData connection_data = std::move(pending_entry->second.data);
  auto callbacks = std::move(pending_entry->second.callbacks);
  pending_connections_.erase(pending_entry);
  uint64_t instance_id = next_instance_id_++;
  instance_map_.emplace(instance_name, instance_id);
  connection_data.stream_manager->set_quic_connection(
      connection_data.connection.get());
  connections_.emplace(instance_id, std::move(connection_data));

  // `callbacks` is empty for QuicServer, so this only works for QuicClient.
  for (auto& request : callbacks) {
    request.second->OnConnectSucceed(request.first, instance_id);
  }

  return instance_id;
}

void QuicServiceBase::OnIncomingStream(uint64_t instance_id,
                                       QuicStream* stream) {
  if (state_ != ProtocolConnectionEndpoint::State::kRunning) {
    return;
  }

  auto connection_entry = connections_.find(instance_id);
  if (connection_entry == connections_.end()) {
    return;
  }

  std::unique_ptr<QuicProtocolConnection> connection =
      connection_entry->second.stream_manager->OnIncomingStream(stream);
  observer_.OnIncomingConnection(std::move(connection));
}

void QuicServiceBase::OnConnectionClosed(std::string_view instance_name) {
  if (state_ != ProtocolConnectionEndpoint::State::kRunning) {
    return;
  }

  auto pending_entry = pending_connections_.find(instance_name);
  if (pending_entry == pending_connections_.end()) {
    auto instance_entry = instance_map_.find(instance_name);
    if (instance_entry == instance_map_.end()) {
      return;
    } else {
      uint64_t instance_id = instance_entry->second;
      auto connection_entry = connections_.find(instance_id);
      if (connection_entry == connections_.end()) {
        return;
      } else {
        instance_request_ids_.ResetRequestId(instance_id);
      }
    }
  }

  ScheduleCleanup(instance_name);
}

QuicStream::Delegate& QuicServiceBase::GetStreamDelegate(uint64_t instance_id) {
  auto connection_entry = connections_.find(instance_id);
  OSP_CHECK(connection_entry != connections_.end());

  return *(connection_entry->second.stream_manager);
}

void QuicServiceBase::OnClientCertificates(
    std::string_view instance_name,
    const std::vector<std::string>& certs) {
  OSP_NOTREACHED();
}

void QuicServiceBase::OnDataReceived(uint64_t instance_id,
                                     uint64_t protocol_connection_id,
                                     ByteView bytes) {
  if (state_ != ProtocolConnectionEndpoint::State::kRunning) {
    return;
  }

  demuxer_.OnStreamData(instance_id, protocol_connection_id, bytes.data(),
                        bytes.size());
}

void QuicServiceBase::OnClose(uint64_t instance_id,
                              uint64_t protocol_connection_id) {
  if (state_ != ProtocolConnectionEndpoint::State::kRunning) {
    return;
  }

  demuxer_.OnStreamClose(instance_id, protocol_connection_id);
}

QuicServiceBase::ServiceConnectionData::ServiceConnectionData(
    std::unique_ptr<QuicConnection> connection,
    std::unique_ptr<QuicStreamManager> manager)
    : connection(std::move(connection)), stream_manager(std::move(manager)) {}

QuicServiceBase::ServiceConnectionData::ServiceConnectionData(
    ServiceConnectionData&&) noexcept = default;

QuicServiceBase::ServiceConnectionData&
QuicServiceBase::ServiceConnectionData::operator=(
    ServiceConnectionData&&) noexcept = default;

QuicServiceBase::ServiceConnectionData::~ServiceConnectionData() = default;

QuicServiceBase::PendingConnectionData::PendingConnectionData(
    ServiceConnectionData&& data)
    : data(std::move(data)) {}

QuicServiceBase::PendingConnectionData::PendingConnectionData(
    PendingConnectionData&&) noexcept = default;

QuicServiceBase::PendingConnectionData&
QuicServiceBase::PendingConnectionData::operator=(
    PendingConnectionData&&) noexcept = default;

QuicServiceBase::PendingConnectionData::~PendingConnectionData() = default;

bool QuicServiceBase::StartImpl() {
  if (state_ != ProtocolConnectionEndpoint::State::kStopped) {
    return false;
  }

  state_ = ProtocolConnectionEndpoint::State::kRunning;
  observer_.OnRunning();
  return true;
}

bool QuicServiceBase::StopImpl() {
  if (state_ != ProtocolConnectionEndpoint::State::kRunning &&
      state_ != ProtocolConnectionEndpoint::State::kSuspended) {
    return false;
  }

  CloseAllConnections();
  state_ = ProtocolConnectionEndpoint::State::kStopped;
  observer_.OnStopped();
  return true;
}

bool QuicServiceBase::SuspendImpl() {
  // TODO(btolsch): QuicStreams should either buffer or reject writes.
  if (state_ != ProtocolConnectionEndpoint::State::kRunning) {
    return false;
  }

  state_ = ProtocolConnectionEndpoint::State::kSuspended;
  observer_.OnSuspended();
  return true;
}

bool QuicServiceBase::ResumeImpl() {
  if (state_ != ProtocolConnectionEndpoint::State::kSuspended) {
    return false;
  }

  state_ = ProtocolConnectionEndpoint::State::kRunning;
  observer_.OnRunning();
  return true;
}

std::unique_ptr<ProtocolConnection>
QuicServiceBase::CreateProtocolConnectionImpl(uint64_t instance_id) {
  if (state_ != ProtocolConnectionEndpoint::State::kRunning) {
    return nullptr;
  }

  auto connection_entry = connections_.find(instance_id);
  if (connection_entry == connections_.end()) {
    return nullptr;
  }

  return QuicProtocolConnection::FromExisting(
      *connection_entry->second.connection,
      *connection_entry->second.stream_manager, instance_id);
}

void QuicServiceBase::CloseAllConnections() {
  for (auto& conn : pending_connections_) {
    conn.second.data.connection->Close();
    // `callbacks` is empty for QuicServer, so this only works for QuicClient.
    for (auto& item : conn.second.callbacks) {
      item.second->OnConnectFailed(item.first);
    }
  }

  for (auto& conn : connections_) {
    conn.second.connection->Close();
  }

  next_instance_id_ = 1;
}

void QuicServiceBase::ScheduleCleanup(std::string_view instance_name) {
  constexpr Clock::duration kQuicCleanupDelay = std::chrono::milliseconds(500);
  auto result = cleanup_alarms_.emplace(
      instance_name, std::make_unique<Alarm>(now_function_, task_runner_));
  if (result.second) {
    result.first->second->ScheduleFromNow(
        [this, instance_name] { Cleanup(instance_name); }, kQuicCleanupDelay);
  }
}

void QuicServiceBase::Cleanup(std::string_view instance_name) {
  cleanup_alarms_.erase(std::string(instance_name));

  auto pending_entry = pending_connections_.find(instance_name);
  if (pending_entry != pending_connections_.end()) {
    connection_factory_->OnConnectionClosed(
        pending_entry->second.data.connection.get());
    pending_connections_.erase(pending_entry);
  }

  auto instance_entry = instance_map_.find(instance_name);
  if (instance_entry != instance_map_.end()) {
    uint64_t instance_id = instance_entry->second;
    auto connection_entry = connections_.find(instance_id);
    if (connection_entry != connections_.end()) {
      connection_factory_->OnConnectionClosed(
          connection_entry->second.connection.get());
      instance_map_.erase(instance_entry);
      connections_.erase(connection_entry);
    }
  }
}

}  // namespace openscreen::osp
