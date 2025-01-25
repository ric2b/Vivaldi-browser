// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_server.h"

#include <functional>
#include <random>
#include <utility>

#include "quiche/quic/core/quic_utils.h"
#include "util/base64.h"
#include "util/osp_logging.h"

namespace openscreen::osp {

QuicServer::QuicServer(
    const ServiceConfig& config,
    std::unique_ptr<QuicConnectionFactoryServer> connection_factory,
    ProtocolConnectionServiceObserver& observer,
    ClockNowFunctionPtr now_function,
    TaskRunner& task_runner,
    size_t buffer_limit)
    : QuicServiceBase(config,
                      std::move(connection_factory),
                      observer,
                      InstanceRequestIds::Role::kServer,
                      now_function,
                      task_runner,
                      buffer_limit),
      instance_name_(config.instance_name) {
  auth_token_ = GenerateToken(16);
}

QuicServer::~QuicServer() = default;

bool QuicServer::Start() {
  bool result = StartImpl();
  if (result) {
    static_cast<QuicConnectionFactoryServer*>(connection_factory_.get())
        ->SetServerDelegate(this, connection_endpoints_);
  }
  return result;
}

bool QuicServer::Stop() {
  bool result = StopImpl();
  if (result) {
    static_cast<QuicConnectionFactoryServer*>(connection_factory_.get())
        ->SetServerDelegate(nullptr, {});
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

std::string QuicServer::GetAuthToken() {
  return auth_token_;
}

void QuicServer::OnClientCertificates(std::string_view instance_name,
                                      const std::vector<std::string>& certs) {
  fingerprint_map_.emplace(instance_name,
                           base64::Encode(quic::RawSha256(certs[0])));
}

void QuicServer::OnIncomingConnection(
    std::unique_ptr<QuicConnection> connection) {
  if (state_ != State::kRunning) {
    return;
  }

  const std::string& instance_name = connection->instance_name();
  pending_connections_.emplace(
      instance_name,
      PendingConnectionData(ServiceConnectionData(
          std::move(connection), std::make_unique<QuicStreamManager>(*this))));
}

std::string QuicServer::GenerateToken(int length) {
  constexpr char characters[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::random_device dev;
  std::mt19937 gen(dev());
  std::uniform_int_distribution<> dis(0, strlen(characters) - 1);
  std::string token;
  token.resize(length);
  for (int i = 0; i < length; i++) {
    token[i] = characters[dis(gen)];
  }

  return token;
}

}  // namespace openscreen::osp
