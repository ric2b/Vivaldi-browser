// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_client.h"

#include <algorithm>
#include <functional>

#include "osp/public/connect_request.h"
#include "util/osp_logging.h"

namespace openscreen::osp {

QuicClient::QuicClient(
    const ServiceConfig& config,
    std::unique_ptr<QuicConnectionFactoryClient> connection_factory,
    ProtocolConnectionServiceObserver& observer,
    ClockNowFunctionPtr now_function,
    TaskRunner& task_runner,
    size_t buffer_limit)
    : QuicServiceBase(config,
                      std::move(connection_factory),
                      observer,
                      InstanceRequestIds::Role::kClient,
                      now_function,
                      task_runner,
                      buffer_limit) {}

QuicClient::~QuicClient() = default;

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
                         ConnectRequestCallback* request_callback) {
  if (state_ != State::kRunning) {
    request_callback->OnConnectFailed(0);
    OSP_LOG_ERROR << "QuicClient connect failed: QuicClient is not running.";
    return false;
  }

  auto instance_entry = instance_map_.find(instance_name);
  // If there is a `instance_entry` for `instance_name`, it means there is an
  // available connection. Otherwise, it means there is no available connection
  // or the connection is still in the process of QUIC handshake.
  if (instance_entry != instance_map_.end()) {
    uint64_t request_id = next_request_id_++;
    request = ConnectRequest(this, request_id);
    request_callback->OnConnectSucceed(request_id, instance_entry->second);
    return true;
  } else {
    auto pending_connection = pending_connections_.find(instance_name);
    if (pending_connection != pending_connections_.end()) {
      uint64_t request_id = next_request_id_++;
      pending_connection->second.callbacks.emplace_back(request_id,
                                                        request_callback);
      request = ConnectRequest(this, request_id);
      return true;
    } else {
      return StartConnectionRequest(instance_name, request, request_callback);
    }
  }
}

void QuicClient::OnStarted() {}
void QuicClient::OnStopped() {}
void QuicClient::OnSuspended() {}
void QuicClient::OnSearching() {}

void QuicClient::OnReceiverAdded(const ServiceInfo& info) {
  instance_infos_.insert(std::make_pair(
      info.instance_name, InstanceInfo{info.fingerprint, info.auth_token,
                                       info.v4_endpoint, info.v6_endpoint}));
}

void QuicClient::OnReceiverChanged(const ServiceInfo& info) {
  instance_infos_[info.instance_name] = InstanceInfo{
      info.fingerprint, info.auth_token, info.v4_endpoint, info.v6_endpoint};
}

void QuicClient::OnReceiverRemoved(const ServiceInfo& info) {
  instance_infos_.erase(info.instance_name);
}

void QuicClient::OnAllReceiversRemoved() {
  instance_infos_.clear();
}

void QuicClient::OnError(const Error&) {}
void QuicClient::OnMetrics(ServiceListener::Metrics) {}

bool QuicClient::StartConnectionRequest(
    std::string_view instance_name,
    ConnectRequest& request,
    ConnectRequestCallback* request_callback) {
  auto instance_entry = instance_infos_.find(instance_name);
  if (instance_entry == instance_infos_.end()) {
    request_callback->OnConnectFailed(0);
    OSP_LOG_ERROR << "QuicClient connect failed: can't find information for "
                  << instance_name;
    return false;
  }

  IPEndpoint endpoint = instance_entry->second.v4_endpoint
                            ? instance_entry->second.v4_endpoint
                            : instance_entry->second.v6_endpoint;
  QuicConnectionFactoryClient::ConnectData connect_data = {
      .instance_name = std::string(instance_name),
      .fingerprint = instance_entry->second.fingerprint};
  ErrorOr<std::unique_ptr<QuicConnection>> connection =
      static_cast<QuicConnectionFactoryClient*>(connection_factory_.get())
          ->Connect(connection_endpoints_[0], endpoint, connect_data, this);
  if (!connection) {
    request_callback->OnConnectFailed(0);
    OSP_LOG_ERROR << "Factory connect failed: " << connection.error();
    return false;
  }

  auto pending_result = pending_connections_.emplace(
      instance_name, PendingConnectionData(ServiceConnectionData(
                         std::move(connection.value()),
                         std::make_unique<QuicStreamManager>(*this))));
  uint64_t request_id = next_request_id_++;
  request = ConnectRequest(this, request_id);
  pending_result.first->second.callbacks.emplace_back(request_id,
                                                      request_callback);
  return true;
}

void QuicClient::CancelConnectRequest(uint64_t request_id) {
  for (auto it = pending_connections_.begin(); it != pending_connections_.end();
       ++it) {
    auto& callbacks = it->second.callbacks;
    auto size_before_delete = callbacks.size();
    callbacks.erase(
        std::remove_if(
            callbacks.begin(), callbacks.end(),
            [request_id](
                const std::pair<uint64_t, ConnectRequestCallback*>& callback) {
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
