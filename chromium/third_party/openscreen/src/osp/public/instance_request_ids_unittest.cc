// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/instance_request_ids.h"

#include "gtest/gtest.h"

namespace openscreen::osp {

// These tests validate RequestId generation for two instances with numbers 3
// and 7.

TEST(InstanceRequestIdsTest, StrictlyIncreasingRequestIdSequence) {
  InstanceRequestIds request_ids_client(InstanceRequestIds::Role::kClient);

  EXPECT_EQ(0u, request_ids_client.GetNextRequestId(7));
  EXPECT_EQ(2u, request_ids_client.GetNextRequestId(7));
  EXPECT_EQ(4u, request_ids_client.GetNextRequestId(7));
  EXPECT_EQ(0u, request_ids_client.GetNextRequestId(3));
  EXPECT_EQ(6u, request_ids_client.GetNextRequestId(7));
  EXPECT_EQ(2u, request_ids_client.GetNextRequestId(3));

  InstanceRequestIds request_ids_server(InstanceRequestIds::Role::kServer);
  EXPECT_EQ(1u, request_ids_server.GetNextRequestId(7));
  EXPECT_EQ(3u, request_ids_server.GetNextRequestId(7));
  EXPECT_EQ(5u, request_ids_server.GetNextRequestId(7));
  EXPECT_EQ(1u, request_ids_server.GetNextRequestId(3));
  EXPECT_EQ(7u, request_ids_server.GetNextRequestId(7));
  EXPECT_EQ(3u, request_ids_server.GetNextRequestId(3));
}

TEST(InstanceRequestIdsTest, ResetRequestId) {
  InstanceRequestIds request_ids_client(InstanceRequestIds::Role::kClient);

  EXPECT_EQ(0u, request_ids_client.GetNextRequestId(7));
  EXPECT_EQ(2u, request_ids_client.GetNextRequestId(7));
  request_ids_client.ResetRequestId(7);
  EXPECT_EQ(0u, request_ids_client.GetNextRequestId(7));
  EXPECT_EQ(2u, request_ids_client.GetNextRequestId(7));
  EXPECT_EQ(0u, request_ids_client.GetNextRequestId(3));
  EXPECT_EQ(2u, request_ids_client.GetNextRequestId(3));
  request_ids_client.ResetRequestId(7);
  EXPECT_EQ(0u, request_ids_client.GetNextRequestId(7));
  EXPECT_EQ(2u, request_ids_client.GetNextRequestId(7));
  EXPECT_EQ(4u, request_ids_client.GetNextRequestId(3));
  EXPECT_EQ(6u, request_ids_client.GetNextRequestId(3));

  InstanceRequestIds request_ids_server(InstanceRequestIds::Role::kServer);

  EXPECT_EQ(1u, request_ids_server.GetNextRequestId(7));
  EXPECT_EQ(3u, request_ids_server.GetNextRequestId(7));
  request_ids_server.ResetRequestId(7);
  EXPECT_EQ(1u, request_ids_server.GetNextRequestId(7));
  EXPECT_EQ(3u, request_ids_server.GetNextRequestId(7));
  EXPECT_EQ(1u, request_ids_server.GetNextRequestId(3));
  EXPECT_EQ(3u, request_ids_server.GetNextRequestId(3));
  request_ids_server.ResetRequestId(7);
  EXPECT_EQ(1u, request_ids_server.GetNextRequestId(7));
  EXPECT_EQ(3u, request_ids_server.GetNextRequestId(7));
  EXPECT_EQ(5u, request_ids_server.GetNextRequestId(3));
  EXPECT_EQ(7u, request_ids_server.GetNextRequestId(3));
}

TEST(InstanceRequestIdsTest, ResetAll) {
  InstanceRequestIds request_ids_client(InstanceRequestIds::Role::kClient);

  EXPECT_EQ(0u, request_ids_client.GetNextRequestId(7));
  EXPECT_EQ(2u, request_ids_client.GetNextRequestId(7));
  EXPECT_EQ(0u, request_ids_client.GetNextRequestId(3));
  EXPECT_EQ(2u, request_ids_client.GetNextRequestId(3));
  request_ids_client.Reset();
  EXPECT_EQ(0u, request_ids_client.GetNextRequestId(7));
  EXPECT_EQ(0u, request_ids_client.GetNextRequestId(3));

  InstanceRequestIds request_ids_server(InstanceRequestIds::Role::kServer);

  EXPECT_EQ(1u, request_ids_server.GetNextRequestId(7));
  EXPECT_EQ(3u, request_ids_server.GetNextRequestId(7));
  EXPECT_EQ(1u, request_ids_server.GetNextRequestId(3));
  EXPECT_EQ(3u, request_ids_server.GetNextRequestId(3));
  request_ids_server.Reset();
  EXPECT_EQ(1u, request_ids_server.GetNextRequestId(7));
  EXPECT_EQ(1u, request_ids_server.GetNextRequestId(3));
}

}  // namespace openscreen::osp
