// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_TESTING_FAKE_DNS_RECORD_FACTORY_H_
#define DISCOVERY_DNSSD_TESTING_FAKE_DNS_RECORD_FACTORY_H_

#include <stdint.h>

#include <chrono>

#include "discovery/dnssd/impl/constants.h"
#include "discovery/mdns/public/mdns_records.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen::discovery {

class FakeDnsRecordFactory {
 public:
  static constexpr uint16_t kPortNum = 80;
  static constexpr uint8_t kV4AddressOctets[4] = {192, 168, 0, 0};
  static constexpr uint16_t kV6AddressHextets[8] = {
      0x0102, 0x0304, 0x0506, 0x0708, 0x090a, 0x0b0c, 0x0d0e, 0x0f10};
  static constexpr char kInstanceName[] = "instance";
  static constexpr char kServiceName[] = "_srv-name._udp";
  static constexpr char kServiceNameProtocolPart[] = "_udp";
  static constexpr char kServiceNameServicePart[] = "_srv-name";
  static constexpr char kDomainName[] = "local";

  static MdnsRecord CreateFullyPopulatedSrvRecord(uint16_t port = kPortNum);
};

}  // namespace openscreen::discovery

#endif  // DISCOVERY_DNSSD_TESTING_FAKE_DNS_RECORD_FACTORY_H_
