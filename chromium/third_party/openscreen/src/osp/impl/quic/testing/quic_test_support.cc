// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/testing/quic_test_support.h"

#include <memory>
#include <utility>
#include <vector>

#include "osp/impl/quic/quic_client.h"
#include "osp/impl/quic/quic_server.h"
#include "osp/public/message_demuxer.h"
#include "osp/public/service_config.h"
#include "platform/test/fake_task_runner.h"

namespace openscreen::osp {

FakeQuicBridge::FakeQuicBridge(FakeTaskRunner& task_runner,
                               ClockNowFunctionPtr now_function)
    : task_runner_(task_runner) {
  fake_bridge_ =
      std::make_unique<FakeQuicConnectionFactoryBridge>(kControllerEndpoint);

  auto fake_client_factory = std::make_unique<FakeClientQuicConnectionFactory>(
      task_runner, fake_bridge_.get());
  client_socket_ = std::make_unique<FakeUdpSocket>(fake_client_factory.get());
  ServiceConfig client_config = {.connection_endpoints = {kControllerEndpoint}};
  quic_client_ = std::make_unique<QuicClient>(
      client_config, std::move(fake_client_factory), mock_client_observer_,
      now_function, task_runner, MessageDemuxer::kDefaultBufferLimit);
  quic_client_->instance_infos_.emplace(
      kInstanceName,
      QuicClient::InstanceInfo{kFingerprint, kAuthToken, kReceiverEndpoint});

  auto fake_server_factory = std::make_unique<FakeServerQuicConnectionFactory>(
      task_runner, fake_bridge_.get());
  server_socket_ = std::make_unique<FakeUdpSocket>(fake_server_factory.get());
  ServiceConfig server_config = {.connection_endpoints = {kReceiverEndpoint},
                                 .instance_name = {kInstanceName}};
  quic_server_ = std::make_unique<QuicServer>(
      server_config, std::move(fake_server_factory), mock_server_observer_,
      now_function, task_runner, MessageDemuxer::kDefaultBufferLimit);

  quic_client_->Start();
  quic_server_->Start();
}

FakeQuicBridge::~FakeQuicBridge() {
  if (use_network_service_manager_) {
    NetworkServiceManager::Dispose();
  }
}

void FakeQuicBridge::CreateNetworkServiceManager(
    std::unique_ptr<ServiceListener> service_listener,
    std::unique_ptr<ServicePublisher> service_publisher) {
  NetworkServiceManager::Create(
      std::move(service_listener), std::move(service_publisher),
      std::move(quic_client_), std::move(quic_server_));
  use_network_service_manager_ = true;
}

QuicClient* FakeQuicBridge::GetQuicClient() {
  if (use_network_service_manager_) {
    return static_cast<QuicClient*>(
        NetworkServiceManager::Get()->GetProtocolConnectionClient());
  } else {
    return quic_client_.get();
  }
}

QuicServer* FakeQuicBridge::GetQuicServer() {
  if (use_network_service_manager_) {
    return static_cast<QuicServer*>(
        NetworkServiceManager::Get()->GetProtocolConnectionServer());
  } else {
    return quic_server_.get();
  }
}

MessageDemuxer& FakeQuicBridge::GetControllerDemuxer() {
  return GetQuicClient()->GetMessageDemuxer();
}

MessageDemuxer& FakeQuicBridge::GetReceiverDemuxer() {
  return GetQuicServer()->GetMessageDemuxer();
}

void FakeQuicBridge::RunTasksUntilIdle() {
  PostClientPacket();
  PostServerPacket();
  task_runner_.PostTask(std::bind(&FakeQuicBridge::PostPacketsUntilIdle, this));
  task_runner_.RunTasksUntilIdle();
}

void FakeQuicBridge::PostClientPacket() {
  UdpPacket packet;
  client_socket_->MockReceivePacket(std::move(packet));
}

void FakeQuicBridge::PostServerPacket() {
  UdpPacket packet;
  server_socket_->MockReceivePacket(std::move(packet));
}

void FakeQuicBridge::PostPacketsUntilIdle() {
  bool client_idle = fake_bridge_->client_idle();
  bool server_idle = fake_bridge_->server_idle();
  if (!client_idle || !server_idle) {
    PostClientPacket();
    PostServerPacket();
    task_runner_.PostTask([this]() { this->PostPacketsUntilIdle(); });
  }
}

}  // namespace openscreen::osp
