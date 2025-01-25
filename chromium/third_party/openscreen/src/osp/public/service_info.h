// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_PUBLIC_SERVICE_INFO_H_
#define OSP_PUBLIC_SERVICE_INFO_H_

#include <cstdint>
#include <string>

#include "platform/api/network_interface.h"
#include "platform/base/ip_address.h"

namespace openscreen::osp {

// This contains canonical information about a specific Open Screen service
// found on the network via our discovery mechanism (mDNS).
struct ServiceInfo {
  bool operator==(const ServiceInfo& other) const;
  bool operator!=(const ServiceInfo& other) const;

  bool Update(const std::string& friendly_name,
              const std::string& fingerprint,
              const std::string& auth_token,
              NetworkInterfaceIndex network_interface_index,
              const IPEndpoint& v4_endpoint,
              const IPEndpoint& v6_endpoint);

  std::string ToString() const;

  // Unique name identifying the Open Screen service.
  std::string instance_name;

  // User visible name of the Open Screen service in UTF-8.
  std::string friendly_name;

  // Agent fingerprint.
  std::string fingerprint;

  // Token for authentication.
  std::string auth_token;

  // The index of the network interface that the screen was discovered on.
  NetworkInterfaceIndex network_interface_index = kInvalidNetworkInterfaceIndex;

  // The network endpoints to create a new connection to the Open Screen
  // service.
  IPEndpoint v4_endpoint;
  IPEndpoint v6_endpoint;
};

}  // namespace openscreen::osp

#endif  // OSP_PUBLIC_SERVICE_INFO_H_
