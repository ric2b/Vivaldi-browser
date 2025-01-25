// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/service_info.h"

#include <algorithm>
#include <utility>

#include "util/osp_logging.h"

namespace openscreen::osp {

bool ServiceInfo::operator==(const ServiceInfo& other) const {
  return (instance_name == other.instance_name &&
          friendly_name == other.friendly_name &&
          fingerprint == other.fingerprint && auth_token == other.auth_token &&
          network_interface_index == other.network_interface_index &&
          v4_endpoint == other.v4_endpoint && v6_endpoint == other.v6_endpoint);
}

bool ServiceInfo::operator!=(const ServiceInfo& other) const {
  return !(*this == other);
}

bool ServiceInfo::Update(const std::string& new_friendly_name,
                         const std::string& new_fingerprint,
                         const std::string& new_auth_token,
                         NetworkInterfaceIndex new_network_interface_index,
                         const IPEndpoint& new_v4_endpoint,
                         const IPEndpoint& new_v6_endpoint) {
  OSP_CHECK(!new_v4_endpoint.address ||
            IPAddress::Version::kV4 == new_v4_endpoint.address.version());
  OSP_CHECK(!new_v6_endpoint.address ||
            IPAddress::Version::kV6 == new_v6_endpoint.address.version());
  const bool changed =
      (friendly_name != new_friendly_name) ||
      (fingerprint != new_fingerprint) || (auth_token != new_auth_token) ||
      (network_interface_index != new_network_interface_index) ||
      (v4_endpoint != new_v4_endpoint) || (v6_endpoint != new_v6_endpoint);

  friendly_name = new_friendly_name;
  fingerprint = new_fingerprint;
  auth_token = new_auth_token;
  network_interface_index = new_network_interface_index;
  v4_endpoint = new_v4_endpoint;
  v6_endpoint = new_v6_endpoint;
  return changed;
}

std::string ServiceInfo::ToString() const {
  std::stringstream ss;
  ss << "ServiceInfo{instance_name=\"" << instance_name
     << "\", friendly_name=\"" << friendly_name << "\", fingerprint=\""
     << fingerprint << "\", auth_token=\"" << auth_token
     << "\", network_interface_index=" << network_interface_index
     << ", v4_endpoint=\"" << v4_endpoint.ToString() << "\", v6_endpoint=\""
     << v6_endpoint.ToString() << "\"}";
  return ss.str();
}

}  // namespace openscreen::osp
