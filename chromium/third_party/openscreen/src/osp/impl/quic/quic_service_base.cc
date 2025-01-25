// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_service_base.h"

#include <utility>

#include "util/osp_logging.h"

namespace openscreen::osp {

QuicServiceBase::QuicServiceBase(const ServiceConfig& config,
                                 MessageDemuxer& demuxer,
                                 ProtocolConnectionServiceObserver& observer,
                                 InstanceRequestIds::Role role,
                                 ClockNowFunctionPtr now_function,
                                 TaskRunner& task_runner)
    : instance_request_ids_(role),
      demuxer_(demuxer),
      observer_(observer),
      connection_endpoints_(config.connection_endpoints),
      cleanup_alarm_(now_function, task_runner) {}

QuicServiceBase::~QuicServiceBase() = default;

void QuicServiceBase::OnIncomingStream(uint64_t instance_id,
                                       QuicStream* stream) {
  OSP_CHECK_EQ(state_, ProtocolConnectionEndpoint::State::kRunning);

  auto connection_entry = connections_.find(instance_id);
  if (connection_entry == connections_.end()) {
    return;
  }

  std::unique_ptr<QuicProtocolConnection> connection =
      connection_entry->second.stream_manager->OnIncomingStream(stream);
  observer_.OnIncomingConnection(std::move(connection));
}

QuicStream::Delegate& QuicServiceBase::GetStreamDelegate(uint64_t instance_id) {
  auto connection_entry = connections_.find(instance_id);
  OSP_CHECK(connection_entry != connections_.end());

  return *(connection_entry->second.stream_manager);
}

void QuicServiceBase::OnConnectionDestroyed(
    QuicProtocolConnection& connection) {
  if (!connection.stream()) {
    return;
  }

  auto connection_entry = connections_.find(connection.instance_id());
  if (connection_entry == connections_.end()) {
    return;
  }

  connection_entry->second.stream_manager->DropProtocolConnection(connection);
}

void QuicServiceBase::OnDataReceived(uint64_t instance_id,
                                     uint64_t protocol_connection_id,
                                     ByteView bytes) {
  OSP_CHECK_EQ(state_, ProtocolConnectionEndpoint::State::kRunning);

  demuxer_.OnStreamData(instance_id, protocol_connection_id, bytes.data(),
                        bytes.size());
}

void QuicServiceBase::OnClose(uint64_t instance_id,
                              uint64_t protocol_connection_id) {
  OSP_CHECK_EQ(state_, ProtocolConnectionEndpoint::State::kRunning);

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

bool QuicServiceBase::StartImpl() {
  if (state_ != ProtocolConnectionEndpoint::State::kStopped) {
    return false;
  }

  state_ = ProtocolConnectionEndpoint::State::kRunning;
  Cleanup();  // Start periodic clean-ups.
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
  Cleanup();  // Final clean-up.
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
      *this, *connection_entry->second.connection,
      *connection_entry->second.stream_manager, instance_id);
}

void QuicServiceBase::Cleanup() {
  for (auto& entry : connections_) {
    entry.second.stream_manager->DestroyClosedStreams();
  }

  for (uint64_t instance_id : delete_connections_) {
    auto it = connections_.find(instance_id);
    if (it != connections_.end()) {
      connections_.erase(it);
    }
  }
  delete_connections_.clear();

  constexpr Clock::duration kQuicCleanupPeriod = std::chrono::milliseconds(500);
  if (state_ != ProtocolConnectionEndpoint::State::kStopped) {
    cleanup_alarm_.ScheduleFromNow([this] { Cleanup(); }, kQuicCleanupPeriod);
  }
}

}  // namespace openscreen::osp
