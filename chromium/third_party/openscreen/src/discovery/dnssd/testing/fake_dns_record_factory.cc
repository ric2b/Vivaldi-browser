// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/testing/fake_dns_record_factory.h"

#include <utility>

namespace openscreen::discovery {

// static
MdnsRecord FakeDnsRecordFactory::CreateFullyPopulatedSrvRecord(uint16_t port) {
  const DomainName kTarget{kInstanceName, "_srv-name", "_udp", kDomainName};
  constexpr auto kType = DnsType::kSRV;
  constexpr auto kClazz = DnsClass::kIN;
  constexpr auto kRecordType = RecordType::kUnique;
  constexpr auto kTtl = std::chrono::seconds(0);
  SrvRecordRdata srv(0, 0, port, kTarget);
  return MdnsRecord(kTarget, kType, kClazz, kRecordType, kTtl, std::move(srv));
}

}  // namespace openscreen::discovery
