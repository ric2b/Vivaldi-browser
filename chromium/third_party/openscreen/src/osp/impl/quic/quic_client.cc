// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_client.h"

#include <algorithm>
#include <functional>

#include "util/osp_logging.h"

namespace openscreen::osp {

QuicClient::QuicClient(
    const ServiceConfig& config,
    MessageDemuxer& demuxer,
    std::unique_ptr<QuicConnectionFactoryClient> connection_factory,
    ProtocolConnectionServiceObserver& observer,
    ClockNowFunctionPtr now_function,
    TaskRunner& task_runner)
    : QuicServiceBase(config,
                      demuxer,
                      observer,
                      InstanceRequestIds::Role::kClient,
                      now_function,
                      task_runner),
      connection_factory_(std::move(connection_factory)) {}

QuicClient::~QuicClient() {
  CloseAllConnections();
}

bool QuicClient::Start() {
  return StartImpl();
}

bool QuicClient::Stop() {
  return StopImpl();
}

// NOTE: Currently we do not support Suspend()/Resume() for the connection
// client.  Add those if we can define behavior for the OSP protocol and QUIC
// for those operations.
// See: https://github.com/webscreens/openscreenprotocol/issues/108
bool QuicClient::Suspend() {
  OSP_NOTREACHED();
}

bool QuicClient::Resume() {
  OSP_NOTREACHED();
}

ProtocolConnectionEndpoint::State QuicClient::GetState() {
  return state_;
}

MessageDemuxer& QuicClient::GetMessageDemuxer() {
  return demuxer_;
}

InstanceRequestIds& QuicClient::GetInstanceRequestIds() {
  return instance_request_ids_;
}

std::unique_ptr<ProtocolConnection> QuicClient::CreateProtocolConnection(
    uint64_t instance_id) {
  return CreateProtocolConnectionImpl(instance_id);
}

bool QuicClient::Connect(std::string_view instance_name,
                         ConnectRequest& request,
                         ConnectionRequestCallback* request_callback) {
  if (state_ != State::kRunning) {
    request_callback->OnConnectionFailed(0);
    OSP_LOG_ERROR << "QuicClient connect failed: QuicClient is not running.";
    return false;
  }

  auto instance_entry = instance_map_.find(instance_name);
  if (instance_entry != instance_map_.end()) {
    auto immediate_result = CreateProtocolConnection(instance_entry->second);
    OSP_CHECK(immediate_result);
    uint64_t request_id = next_request_id_++;
    request = ConnectRequest(this, request_id);
    request_callback->OnConnectionOpened(request_id,
                                         std::move(immediate_result));
    return true;
  }

  return CreatePendingConnection(instance_name, request, request_callback);
}

uint64_t QuicClient::OnCryptoHandshakeComplete(std::string_view instance_name) {
  OSP_CHECK_EQ(state_, ProtocolConnectionEndpoint::State::kRunning);

  auto pending_entry = pending_connections_.find(instance_name);
  if (pending_entry == pending_connections_.end()) {
    return 0;
  }

  ServiceConnectionData connection_data = std::move(pending_entry->second.data);
  auto* connection = connection_data.connection.get();
  auto* stream_manager = connection_data.stream_manager.get();
  uint64_t instance_id = next_instance_id_++;
  instance_map_.emplace(instance_name, instance_id);
  stream_manager->set_quic_connection(connection);
  connections_.emplace(instance_id, std::move(connection_data));

  for (auto& request : pending_entry->second.callbacks) {
    std::unique_ptr<QuicProtocolConnection> pc =
        QuicProtocolConnection::FromExisting(*this, *connection,
                                             *stream_manager, instance_id);
    request.second->OnConnectionOpened(request.first, std::move(pc));
  }
  pending_connections_.erase(pending_entry);
  return instance_id;
}

void QuicClient::OnConnectionClosed(uint64_t instance_id) {
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

QuicClient::PendingConnectionData::PendingConnectionData(
    ServiceConnectionData&& data)
    : data(std::move(data)) {}

QuicClient::PendingConnectionData::PendingConnectionData(
    PendingConnectionData&&) noexcept = default;

QuicClient::PendingConnectionData& QuicClient::PendingConnectionData::operator=(
    PendingConnectionData&&) noexcept = default;

QuicClient::PendingConnectionData::~PendingConnectionData() = default;

void QuicClient::OnStarted() {}
void QuicClient::OnStopped() {}
void QuicClient::OnSuspended() {}
void QuicClient::OnSearching() {}

void QuicClient::OnReceiverAdded(const ServiceInfo& info) {
  instance_infos_.insert(std::make_pair(
      info.instance_name,
      InstanceInfo{info.fingerprint, info.v4_endpoint, info.v6_endpoint}));
}

void QuicClient::OnReceiverChanged(const ServiceInfo& info) {
  instance_infos_[info.instance_name] =
      InstanceInfo{info.fingerprint, info.v4_endpoint, info.v6_endpoint};
}

void QuicClient::OnReceiverRemoved(const ServiceInfo& info) {
  instance_infos_.erase(info.instance_name);
}

void QuicClient::OnAllReceiversRemoved() {
  instance_infos_.clear();
}

void QuicClient::OnError(const Error&) {}
void QuicClient::OnMetrics(ServiceListener::Metrics) {}

void QuicClient::CloseAllConnections() {
  for (auto& conn : pending_connections_) {
    conn.second.data.connection->Close();
    connection_factory_->OnConnectionClosed(conn.second.data.connection.get());
    for (auto& item : conn.second.callbacks) {
      item.second->OnConnectionFailed(item.first);
    }
  }
  pending_connections_.clear();

  for (auto& conn : connections_) {
    conn.second.connection->Close();
    connection_factory_->OnConnectionClosed(conn.second.connection.get());
  }
  connections_.clear();

  instance_map_.clear();
  next_instance_id_ = 1;
  instance_request_ids_.Reset();
}

bool QuicClient::CreatePendingConnection(
    std::string_view instance_name,
    ConnectRequest& request,
    ConnectionRequestCallback* request_callback) {
  auto pending_entry = pending_connections_.find(instance_name);
  if (pending_entry == pending_connections_.end()) {
    uint64_t request_id =
        StartConnectionRequest(instance_name, request_callback);
    if (request_id) {
      request = ConnectRequest(this, request_id);
      return true;
    } else {
      return false;
    }
  } else {
    uint64_t request_id = next_request_id_++;
    pending_entry->second.callbacks.emplace_back(request_id, request_callback);
    request = ConnectRequest(this, request_id);
    return true;
  }
}

uint64_t QuicClient::StartConnectionRequest(
    std::string_view instance_name,
    ConnectionRequestCallback* request_callback) {
  auto instance_entry = instance_infos_.find(instance_name);
  if (instance_entry == instance_infos_.end()) {
    request_callback->OnConnectionFailed(0);
    OSP_LOG_ERROR << "QuicClient connect failed: can't find information for "
                  << instance_name;
    return 0;
  }

  IPEndpoint endpoint = instance_entry->second.v4_endpoint
                            ? instance_entry->second.v4_endpoint
                            : instance_entry->second.v6_endpoint;
  QuicConnectionFactoryClient::ConnectData connect_data = {
      .instance_name = std::string(instance_name),
      .fingerprint = instance_entry->second.fingerprint};
  ErrorOr<std::unique_ptr<QuicConnection>> connection =
      connection_factory_->Connect(connection_endpoints_[0], endpoint,
                                   connect_data, this);
  if (!connection) {
    request_callback->OnConnectionFailed(0);
    OSP_LOG_ERROR << "Factory connect failed: " << connection.error();
    return 0;
  }

  auto pending_result = pending_connections_.emplace(
      instance_name, PendingConnectionData(ServiceConnectionData(
                         std::move(connection.value()),
                         std::make_unique<QuicStreamManager>(*this))));
  uint64_t request_id = next_request_id_++;
  pending_result.first->second.callbacks.emplace_back(request_id,
                                                      request_callback);
  return request_id;
}

void QuicClient::CancelConnectRequest(uint64_t request_id) {
  for (auto it = pending_connections_.begin(); it != pending_connections_.end();
       ++it) {
    auto& callbacks = it->second.callbacks;
    auto size_before_delete = callbacks.size();
    callbacks.erase(
        std::remove_if(
            callbacks.begin(), callbacks.end(),
            [request_id](const std::pair<uint64_t, ConnectionRequestCallback*>&
                             callback) {
              return request_id == callback.first;
            }),
        callbacks.end());

    if (callbacks.empty()) {
      pending_connections_.erase(it);
      return;
    }

    // If the size of the callbacks vector has changed, we have found the entry
    // and can break out of the loop.
    if (size_before_delete > callbacks.size()) {
      return;
    }
  }
}

}  // namespace openscreen::osp
