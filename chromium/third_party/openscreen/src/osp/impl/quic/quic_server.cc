// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_server.h"

#include <functional>
#include <utility>

#include "util/osp_logging.h"

namespace openscreen::osp {

// static
QuicAgentCertificate& QuicServer::GetAgentCertificate() {
  static QuicAgentCertificate agent_certificate;
  return agent_certificate;
}

QuicServer::QuicServer(
    const ServiceConfig& config,
    MessageDemuxer& demuxer,
    std::unique_ptr<QuicConnectionFactoryServer> connection_factory,
    ProtocolConnectionServiceObserver& observer,
    ClockNowFunctionPtr now_function,
    TaskRunner& task_runner)
    : QuicServiceBase(config,
                      demuxer,
                      observer,
                      InstanceRequestIds::Role::kServer,
                      now_function,
                      task_runner),
      instance_name_(config.instance_name),
      connection_factory_(std::move(connection_factory)) {}

QuicServer::~QuicServer() {
  CloseAllConnections();
}

bool QuicServer::Start() {
  bool result = StartImpl();
  if (result) {
    connection_factory_->SetServerDelegate(this, connection_endpoints_);
  }
  return result;
}

bool QuicServer::Stop() {
  bool result = StopImpl();
  if (result) {
    connection_factory_->SetServerDelegate(nullptr, {});
  }
  return result;
}

bool QuicServer::Suspend() {
  return SuspendImpl();
}

bool QuicServer::Resume() {
  return ResumeImpl();
}

ProtocolConnectionEndpoint::State QuicServer::GetState() {
  return state_;
}

MessageDemuxer& QuicServer::GetMessageDemuxer() {
  return demuxer_;
}

InstanceRequestIds& QuicServer::GetInstanceRequestIds() {
  return instance_request_ids_;
}

std::unique_ptr<ProtocolConnection> QuicServer::CreateProtocolConnection(
    uint64_t instance_id) {
  return CreateProtocolConnectionImpl(instance_id);
}

std::string QuicServer::GetAgentFingerprint() {
  return GetAgentCertificate().GetAgentFingerprint();
}

uint64_t QuicServer::OnCryptoHandshakeComplete(std::string_view instance_name) {
  OSP_CHECK_EQ(state_, ProtocolConnectionEndpoint::State::kRunning);

  auto pending_entry = pending_connections_.find(instance_name);
  if (pending_entry == pending_connections_.end()) {
    return 0;
  }

  ServiceConnectionData connection_data = std::move(pending_entry->second);
  pending_connections_.erase(pending_entry);
  uint64_t instance_id = next_instance_id_++;
  instance_map_.emplace(instance_name, instance_id);
  connection_data.stream_manager->set_quic_connection(
      connection_data.connection.get());
  connections_.emplace(instance_id, std::move(connection_data));
  return instance_id;
}

void QuicServer::OnConnectionClosed(uint64_t instance_id) {
  OSP_CHECK_EQ(state_, ProtocolConnectionEndpoint::State::kRunning);

  auto connection_entry = connections_.find(instance_id);
  if (connection_entry == connections_.end()) {
    return;
  }

  connection_factory_->OnConnectionClosed(
      connection_entry->second.connection.get());
  delete_connections_.push_back(instance_id);
  instance_request_ids_.ResetRequestId(instance_id);
}

void QuicServer::CloseAllConnections() {
  for (auto& conn : pending_connections_) {
    conn.second.connection->Close();
    connection_factory_->OnConnectionClosed(conn.second.connection.get());
  }
  pending_connections_.clear();

  for (auto& conn : connections_) {
    conn.second.connection->Close();
    connection_factory_->OnConnectionClosed(conn.second.connection.get());
  }
  connections_.clear();

  instance_map_.clear();
  next_instance_id_ = 1u;
  instance_request_ids_.Reset();
}

void QuicServer::OnIncomingConnection(
    std::unique_ptr<QuicConnection> connection) {
  OSP_CHECK_EQ(state_, State::kRunning);

  const std::string& instance_name = connection->instance_name();
  pending_connections_.emplace(
      instance_name,
      ServiceConnectionData(std::move(connection),
                            std::make_unique<QuicStreamManager>(*this)));
}

}  // namespace openscreen::osp
