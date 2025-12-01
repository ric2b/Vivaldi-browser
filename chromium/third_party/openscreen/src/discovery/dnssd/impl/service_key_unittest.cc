// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/service_key.h"

#include <unordered_map>

#include "discovery/dnssd/testing/fake_dns_record_factory.h"
#include "gtest/gtest.h"

namespace openscreen::discovery {

TEST(DnsSdServiceKeyTest, TestServiceKeyEquals) {
  ServiceKey key1("_service._udp", "domain");
  ServiceKey key2("_service._udp", "domain");
  EXPECT_TRUE(key1 == key2);
  EXPECT_FALSE(key1 != key2);

  key1 = ServiceKey("_service2._udp", "domain");
  EXPECT_FALSE(key1 == key2);
  EXPECT_TRUE(key1 != key2);
  key2 = ServiceKey("_service2._udp", "domain");
  EXPECT_TRUE(key1 == key2);
  EXPECT_FALSE(key1 != key2);

  key1 = ServiceKey("_service._udp", "domain2");
  EXPECT_FALSE(key1 == key2);
  EXPECT_TRUE(key1 != key2);
  key2 = ServiceKey("_service._udp", "domain2");
  EXPECT_TRUE(key1 == key2);
  EXPECT_FALSE(key1 != key2);
}

TEST(DnsSdServiceKeyTest, CreateFromRecordTest) {
  MdnsRecord record = FakeDnsRecordFactory::CreateFullyPopulatedSrvRecord();
  ServiceKey key(record);
  EXPECT_EQ(key.service_id(), FakeDnsRecordFactory::kServiceName);
  EXPECT_EQ(key.domain_id(), FakeDnsRecordFactory::kDomainName);
}

}  // namespace openscreen::discovery
