// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_TESTING_FAKE_NETWORK_INTERFACE_CONFIG_H_
#define DISCOVERY_DNSSD_TESTING_FAKE_NETWORK_INTERFACE_CONFIG_H_

#include <utility>

#include "discovery/dnssd/impl/network_interface_config.h"

namespace openscreen::discovery {

class FakeNetworkInterfaceConfig : public NetworkInterfaceConfig {
 public:
  FakeNetworkInterfaceConfig()
      : NetworkInterfaceConfig(kInvalidNetworkInterfaceIndex,
                               IPAddress::kAnyV4(),
                               IPAddress::kAnyV6()) {}

  void set_address_v4(IPAddress address) { address_v4_ = std::move(address); }

  void set_address_v6(IPAddress address) { address_v6_ = std::move(address); }
};

}  // namespace openscreen::discovery

#endif  // DISCOVERY_DNSSD_TESTING_FAKE_NETWORK_INTERFACE_CONFIG_H_
