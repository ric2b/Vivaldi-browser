/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <memory>

#include "absl/status/status.h"
#include "fcp/secagg/server/secret_sharing_graph.h"
#include "fcp/secagg/server/secret_sharing_graph_factory.h"
#include "fcp/testing/testing.h"

namespace fcp {
namespace secagg {
namespace {

static constexpr int kNumNodes = 10;
static constexpr int kThreshold = 5;

TEST(SecretSharingCompleteGraphTest, GetNumNodes) {
  SecretSharingGraphFactory factory;
  auto graph = factory.CreateCompleteGraph(kNumNodes, kThreshold);
  EXPECT_EQ(graph->GetNumNodes(), kNumNodes);
}

TEST(SecretSharingCompleteGraphTest, GetDegree) {
  SecretSharingGraphFactory factory;
  auto graph = factory.CreateCompleteGraph(kNumNodes, kThreshold);
  EXPECT_EQ(graph->GetDegree(), kNumNodes);
}

TEST(SecretSharingCompleteGraphTest, GetThreshold_Valid) {
  SecretSharingGraphFactory factory;
  auto graph = factory.CreateCompleteGraph(kNumNodes, kThreshold);
  EXPECT_EQ(graph->GetThreshold(), kThreshold);
}

TEST(SecretSharingCompleteGraphTest, Threshold_OutOfRange) {
  SecretSharingGraphFactory factory;
  EXPECT_DEATH(factory.CreateCompleteGraph(kNumNodes, kNumNodes + 1), "");
}

TEST(SecretSharingCompleteGraphTest, GetNeighbor_Valid) {
  SecretSharingGraphFactory factory;
  auto graph = factory.CreateCompleteGraph(kNumNodes, kThreshold);
  for (int i = 0; i < graph->GetDegree(); i++) {
    EXPECT_EQ(graph->GetNeighbor(0, i), i);
  }
}

TEST(SecretSharingCompleteGraphTest, GetNeighbor_OutOfRange) {
  SecretSharingGraphFactory factory;
  auto graph = factory.CreateCompleteGraph(kNumNodes, kThreshold);
  EXPECT_DEATH(graph->GetNeighbor(0, -1), "");
  EXPECT_DEATH(graph->GetNeighbor(0, kNumNodes), "");
}

TEST(SecretSharingCompleteGraphTest, AreNeighbors_Valid) {
  SecretSharingGraphFactory factory;
  auto graph = factory.CreateCompleteGraph(kNumNodes, kThreshold);
  for (int i = 0; i < graph->GetDegree(); i++) {
    EXPECT_TRUE(graph->AreNeighbors(0, i));
  }
}

TEST(SecretSharingCompleteGraphTest, AreNeighbors_OutOfRange) {
  SecretSharingGraphFactory factory;
  auto graph = factory.CreateCompleteGraph(kNumNodes, kThreshold);
  EXPECT_DEATH(graph->AreNeighbors(0, -1), "");
  EXPECT_DEATH(graph->AreNeighbors(0, kNumNodes), "");
}

TEST(SecretSharingCompleteGraphTest, GetNeighborIndex) {
  SecretSharingGraphFactory factory;
  auto graph = factory.CreateCompleteGraph(kNumNodes, kThreshold);
  for (int i = 0; i < graph->GetNumNodes(); i++) {
    for (int j = 0; j < graph->GetDegree(); j++) {
      EXPECT_EQ(graph->GetNeighborIndex(i, j), j);
    }
  }
}

TEST(SecretSharingCompleteGraphTest, IsOutgoingNeighbor) {
  SecretSharingGraphFactory factory;
  auto graph = factory.CreateCompleteGraph(kNumNodes, kThreshold);
  for (int i = 0; i < graph->GetNumNodes(); i++) {
    for (int j = 0; j < graph->GetDegree(); j++) {
      EXPECT_EQ(graph->IsOutgoingNeighbor(i, j), i <= j);
    }
  }
}

}  // namespace
}  // namespace secagg
}  // namespace fcp
