// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/network_service_manager.h"

#include "util/osp_logging.h"

namespace {

openscreen::osp::NetworkServiceManager* g_network_service_manager_instance =
    nullptr;

}  //  namespace

namespace openscreen::osp {

// static
NetworkServiceManager* NetworkServiceManager::Create(
    std::unique_ptr<ServiceListener> service_listener,
    std::unique_ptr<ServicePublisher> service_publisher,
    std::unique_ptr<ProtocolConnectionClient> connection_client,
    std::unique_ptr<ProtocolConnectionServer> connection_server) {
  OSP_CHECK(!g_network_service_manager_instance);
  g_network_service_manager_instance = new NetworkServiceManager(
      std::move(service_listener), std::move(service_publisher),
      std::move(connection_client), std::move(connection_server));
  return g_network_service_manager_instance;
}

// static
NetworkServiceManager* NetworkServiceManager::Get() {
  OSP_CHECK(g_network_service_manager_instance);
  return g_network_service_manager_instance;
}

// static
void NetworkServiceManager::Dispose() {
  OSP_CHECK(g_network_service_manager_instance);
  delete g_network_service_manager_instance;
  g_network_service_manager_instance = nullptr;
}

ServiceListener* NetworkServiceManager::GetServiceListener() {
  OSP_CHECK(service_listener_);
  return service_listener_.get();
}

ServicePublisher* NetworkServiceManager::GetServicePublisher() {
  OSP_CHECK(service_publisher_);
  return service_publisher_.get();
}

ProtocolConnectionClient* NetworkServiceManager::GetProtocolConnectionClient() {
  OSP_CHECK(connection_client_);
  return connection_client_.get();
}

ProtocolConnectionServer* NetworkServiceManager::GetProtocolConnectionServer() {
  OSP_CHECK(connection_server_);
  return connection_server_.get();
}

NetworkServiceManager::NetworkServiceManager(
    std::unique_ptr<ServiceListener> service_listener,
    std::unique_ptr<ServicePublisher> service_publisher,
    std::unique_ptr<ProtocolConnectionClient> connection_client,
    std::unique_ptr<ProtocolConnectionServer> connection_server)
    : service_listener_(std::move(service_listener)),
      service_publisher_(std::move(service_publisher)),
      connection_client_(std::move(connection_client)),
      connection_server_(std::move(connection_server)) {}

NetworkServiceManager::~NetworkServiceManager() = default;

}  // namespace openscreen::osp
