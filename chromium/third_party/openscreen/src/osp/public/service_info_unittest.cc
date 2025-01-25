// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/service_info.h"

#include <utility>

#include "gtest/gtest.h"
#include "platform/base/error.h"

namespace openscreen::osp {

TEST(ServiceInfoTest, Compare) {
  const ServiceInfo receiver1{"id1",    "name1", "fingerprint1",
                              "token1", 1,       {{192, 168, 1, 10}, 12345},
                              {}};
  const ServiceInfo receiver2{"id2",    "name2", "fingerprint2",
                              "token2", 1,       {{192, 168, 1, 11}, 12346},
                              {}};
  const ServiceInfo receiver1_alt_id{
      "id3", "name1", "fingerprint1", "token1", 1, {{192, 168, 1, 10}, 12345},
      {}};
  const ServiceInfo receiver1_alt_name{
      "id1", "name2", "fingerprint1", "token1", 1, {{192, 168, 1, 10}, 12345},
      {}};
  const ServiceInfo receiver1_alt_fingerprint{
      "id1", "name1", "fingerprint2", "token1", 1, {{192, 168, 1, 10}, 12345},
      {}};
  const ServiceInfo receiver1_alt_token{
      "id1", "name1", "fingerprint1", "token2", 1, {{192, 168, 1, 10}, 12345},
      {}};
  const ServiceInfo receiver1_alt_interface{
      "id1", "name1", "fingerprint1", "token1", 2, {{192, 168, 1, 10}, 12345},
      {}};
  const ServiceInfo receiver1_alt_ip{
      "id3", "name1", "fingerprint1", "token1", 1, {{192, 168, 1, 12}, 12345},
      {}};
  const ServiceInfo receiver1_alt_port{
      "id3", "name1", "fingerprint1", "token1", 1, {{192, 168, 1, 10}, 12645},
      {}};
  ServiceInfo receiver1_ipv6{"id3", "name1", "fingerprint1", "token1", 1,
                             {},    {}};

  ErrorOr<IPAddress> address = IPAddress::Parse("::12:34");
  ASSERT_TRUE(address);
  receiver1_ipv6.v6_endpoint.address = address.value();

  EXPECT_EQ(receiver1, receiver1);
  EXPECT_EQ(receiver2, receiver2);
  EXPECT_NE(receiver1, receiver2);
  EXPECT_NE(receiver2, receiver1);

  EXPECT_NE(receiver1, receiver1_alt_id);
  EXPECT_NE(receiver1, receiver1_alt_name);
  EXPECT_NE(receiver1, receiver1_alt_fingerprint);
  EXPECT_NE(receiver1, receiver1_alt_token);
  EXPECT_NE(receiver1, receiver1_alt_interface);
  EXPECT_NE(receiver1, receiver1_alt_ip);
  EXPECT_NE(receiver1, receiver1_alt_port);
  EXPECT_NE(receiver1, receiver1_ipv6);
}

TEST(ServiceInfoTest, Update) {
  ServiceInfo original{"foo",    "baz", "fingerprint1",
                       "token1", 1,     {{192, 168, 1, 10}, 12345},
                       {}};
  const ServiceInfo updated{"foo",    "buzz", "fingerprint2",
                            "token2", 1,      {{193, 169, 2, 11}, 1234},
                            {}};
  original.Update("buzz", "fingerprint2", "token2", 1,
                  {{193, 169, 2, 11}, 1234}, {});
  EXPECT_EQ(original, updated);
}
}  // namespace openscreen::osp
