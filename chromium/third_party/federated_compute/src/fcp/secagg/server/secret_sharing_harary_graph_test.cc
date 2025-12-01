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

#include <algorithm>
#include <memory>
#include <string>

#include "absl/status/status.h"
#include "fcp/secagg/server/secret_sharing_graph.h"
#include "fcp/secagg/server/secret_sharing_graph_factory.h"
#include "fcp/testing/testing.h"

namespace fcp {
namespace secagg {
namespace {

namespace secret_sharing_harary_graph_test_internal {
// Auxiliary function returning the index of j in the list of neighbors of i,
// in a graph g represented as an adjacency list
std::optional<int> GetNeighborIndexFromAdjacencyList(
    const std::vector<std::vector<int>>& g, int i, int j) {
  auto index = std::find(std::begin(g[i]), std::end(g[i]), j);
  if (index != std::end(g[i])) {
    return *index;
  }
  return {};
}
}  // namespace secret_sharing_harary_graph_test_internal

static constexpr int kNumNodes = 10;
static constexpr int kDegree = 5;
static constexpr int kThreshold = 2;

TEST(SecretSharingHararyGraphTest, GetPermutationReturnsPermutation) {
  SecretSharingGraphFactory factory;
  auto graph = factory.CreateHararyGraph(kNumNodes, kDegree, kThreshold);
  std::vector<int> permutation = graph->GetPermutationForTesting();
  EXPECT_EQ(permutation.size(), kNumNodes);
  std::vector<int> counters(kNumNodes, 0);
  for (int i = 0; i < permutation.size(); ++i) {
    counters[permutation[i]]++;
  }
  for (auto x : counters) {
    EXPECT_EQ(x, 1);
  }
}

TEST(SecretSharingHararyGraphTest,
     GetPermutationInDeterministicHararyGraphReturnsIdentityPermutation) {
  SecretSharingGraphFactory factory;
  auto graph = factory.CreateHararyGraph(kNumNodes, kDegree, kThreshold, false);
  std::vector<int> permutation = graph->GetPermutationForTesting();
  for (int i = 0; i < permutation.size(); ++i) {
    EXPECT_EQ(permutation[i], i);
  }
}

TEST(SecretSharingHararyGraphTest,
     GetPermutationInRandomHararyGraphDoesNotReturnIdentityPermutation) {
  SecretSharingGraphFactory factory;
  // We use a larger number of nodes so that the probability of getting the
  // identity permutation by change is negligible
  int larger_num_nodes = 100;
  auto graph = factory.CreateHararyGraph(larger_num_nodes, kDegree, kThreshold);
  std::vector<int> permutation = graph->GetPermutationForTesting();
  // Find j so that permutation[j] != j. This will be the case for most i's (all
  // but one in expectation), but one is sufficient in this test.
  bool found = false;
  for (int i = 0; i < permutation.size(); ++i) {
    found = found || (i != permutation[i]);
  }
  EXPECT_EQ(found, true);
}

TEST(SecretSharingHararyGraphTest, AreNeighborsIsCorrectInRandomHararyGraph) {
  SecretSharingGraphFactory factory;
  auto graph = factory.CreateHararyGraph(kNumNodes, kDegree, kThreshold);

  EXPECT_EQ(graph->GetNumNodes(), kNumNodes);
  EXPECT_EQ(graph->GetDegree(), kDegree);
  EXPECT_EQ(graph->GetThreshold(), kThreshold);

  std::vector<int> p = graph->GetPermutationForTesting();
  std::vector<std::vector<int>> adjacency_list(kNumNodes,
                                               std::vector<int>(kDegree));
  adjacency_list[p[0]] = {p[8], p[9], p[0], p[1], p[2]};
  adjacency_list[p[1]] = {p[9], p[0], p[1], p[2], p[3]};
  adjacency_list[p[2]] = {p[0], p[1], p[2], p[3], p[4]};
  adjacency_list[p[3]] = {p[1], p[2], p[3], p[4], p[5]};
  adjacency_list[p[4]] = {p[2], p[3], p[4], p[5], p[6]};
  adjacency_list[p[5]] = {p[3], p[4], p[5], p[6], p[7]};
  adjacency_list[p[6]] = {p[4], p[5], p[6], p[7], p[8]};
  adjacency_list[p[7]] = {p[5], p[6], p[7], p[8], p[9]};
  adjacency_list[p[8]] = {p[6], p[7], p[8], p[9], p[0]};
  adjacency_list[p[9]] = {p[7], p[8], p[9], p[0], p[1]};

  for (int i = 0; i < kNumNodes; ++i) {
    for (int j = 0; j < kNumNodes; ++j) {
      bool are_neighbors =
          secret_sharing_harary_graph_test_internal::
              GetNeighborIndexFromAdjacencyList(adjacency_list, i, j)
                  .value_or(-1) >= 0;
      EXPECT_EQ(graph->AreNeighbors(i, j), are_neighbors) << i << "," << j;
    }
  }
}

TEST(SecretSharingHararyGraphTest,
     AreNeighborsIsCorrectInDeterministicHararyGraph) {
  SecretSharingGraphFactory factory;
  auto graph = factory.CreateHararyGraph(kNumNodes, kDegree, kThreshold, false);

  EXPECT_EQ(graph->GetNumNodes(), kNumNodes);
  EXPECT_EQ(graph->GetDegree(), kDegree);
  EXPECT_EQ(graph->GetThreshold(), kThreshold);

  std::vector<int> p = graph->GetPermutationForTesting();
  std::vector<std::vector<int>> adjacency_list(kNumNodes,
                                               std::vector<int>(kDegree));
  adjacency_list[0] = {8, 9, 0, 1, 2};
  adjacency_list[1] = {9, 0, 1, 2, 3};
  adjacency_list[2] = {0, 1, 2, 3, 4};
  adjacency_list[3] = {1, 2, 3, 4, 5};
  adjacency_list[4] = {2, 3, 4, 5, 6};
  adjacency_list[5] = {3, 4, 5, 6, 7};
  adjacency_list[6] = {4, 5, 6, 7, 8};
  adjacency_list[7] = {5, 6, 7, 8, 9};
  adjacency_list[8] = {6, 7, 8, 9, 0};
  adjacency_list[9] = {7, 8, 9, 0, 1};

  for (int i = 0; i < kNumNodes; ++i) {
    for (int j = 0; j < kNumNodes; ++j) {
      bool are_neighbors =
          secret_sharing_harary_graph_test_internal::
              GetNeighborIndexFromAdjacencyList(adjacency_list, i, j)
                  .value_or(-1) >= 0;
      EXPECT_EQ(graph->AreNeighbors(i, j), are_neighbors) << i << "," << j;
    }
  }
}

TEST(SecretSharingHararyGraphTest, GetNeighborsIsCorrectInRandomHararyGraph) {
  SecretSharingGraphFactory factory;
  auto graph = factory.CreateHararyGraph(kNumNodes, kDegree, kThreshold);

  EXPECT_EQ(graph->GetNumNodes(), kNumNodes);
  EXPECT_EQ(graph->GetDegree(), kDegree);
  EXPECT_EQ(graph->GetThreshold(), kThreshold);

  std::vector<int> p = graph->GetPermutationForTesting();
  std::vector<std::vector<int>> adjacency_list(kNumNodes,
                                               std::vector<int>(kDegree));
  adjacency_list[p[0]] = {p[8], p[9], p[0], p[1], p[2]};
  adjacency_list[p[1]] = {p[9], p[0], p[1], p[2], p[3]};
  adjacency_list[p[2]] = {p[0], p[1], p[2], p[3], p[4]};
  adjacency_list[p[3]] = {p[1], p[2], p[3], p[4], p[5]};
  adjacency_list[p[4]] = {p[2], p[3], p[4], p[5], p[6]};
  adjacency_list[p[5]] = {p[3], p[4], p[5], p[6], p[7]};
  adjacency_list[p[6]] = {p[4], p[5], p[6], p[7], p[8]};
  adjacency_list[p[7]] = {p[5], p[6], p[7], p[8], p[9]};
  adjacency_list[p[8]] = {p[6], p[7], p[8], p[9], p[0]};
  adjacency_list[p[9]] = {p[7], p[8], p[9], p[0], p[1]};

  for (int i = 0; i < kNumNodes; ++i) {
    for (int j = 0; j < kDegree; ++j) {
      auto x = graph->GetNeighbor(i, j);
      EXPECT_EQ(adjacency_list[i][j], x);
    }
  }
}

TEST(SecretSharingHararyGraphTest,
     GetNeighborsIsCorrectInDeterministicHararyGraph) {
  SecretSharingGraphFactory factory;
  auto graph = factory.CreateHararyGraph(kNumNodes, kDegree, kThreshold, false);

  EXPECT_EQ(graph->GetNumNodes(), kNumNodes);
  EXPECT_EQ(graph->GetDegree(), kDegree);
  EXPECT_EQ(graph->GetThreshold(), kThreshold);

  std::vector<int> p = graph->GetPermutationForTesting();
  std::vector<std::vector<int>> adjacency_list(kNumNodes,
                                               std::vector<int>(kDegree));
  adjacency_list[0] = {8, 9, 0, 1, 2};
  adjacency_list[1] = {9, 0, 1, 2, 3};
  adjacency_list[2] = {0, 1, 2, 3, 4};
  adjacency_list[3] = {1, 2, 3, 4, 5};
  adjacency_list[4] = {2, 3, 4, 5, 6};
  adjacency_list[5] = {3, 4, 5, 6, 7};
  adjacency_list[6] = {4, 5, 6, 7, 8};
  adjacency_list[7] = {5, 6, 7, 8, 9};
  adjacency_list[8] = {6, 7, 8, 9, 0};
  adjacency_list[9] = {7, 8, 9, 0, 1};

  for (int i = 0; i < kNumNodes; ++i) {
    for (int j = 0; j < kDegree; ++j) {
      auto x = graph->GetNeighbor(i, j);
      EXPECT_EQ(adjacency_list[i][j], x);
    }
  }
}

struct HararyGraphParams {
  const std::string test_name;
  const int kNumNodes;
  const int kDegree;
  const int kThreshold;
};

class SecretSharingHararyGraphParamTest_Valid
    : public ::testing::TestWithParam<HararyGraphParams> {};

TEST_P(SecretSharingHararyGraphParamTest_Valid,
       GetNeighborIndexIsCorrectInHararyGraph) {
  const HararyGraphParams& graph_params = GetParam();
  SecretSharingGraphFactory factory;
  std::unique_ptr<SecretSharingGraph> graph = factory.CreateHararyGraph(
      graph_params.kNumNodes, graph_params.kDegree, graph_params.kThreshold);

  EXPECT_EQ(graph->GetNumNodes(), graph_params.kNumNodes);
  EXPECT_EQ(graph->GetDegree(), graph_params.kDegree);
  EXPECT_EQ(graph->GetThreshold(), graph_params.kThreshold);

  for (int i = 0; i < graph_params.kNumNodes; ++i) {
    for (int j = 0; j < graph_params.kDegree; ++j) {
      auto x = graph->GetNeighbor(i, j);
      EXPECT_EQ(graph->GetNeighborIndex(i, x), j);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    SecretSharingHararyGraphParamTests, SecretSharingHararyGraphParamTest_Valid,
    ::testing::ValuesIn<HararyGraphParams>({
        {"10_nodes__degree_1", 10, 1, 1},
        {"10_nodes__degree_3", 10, 3, 2},
        {"10_nodes__degree_5", 10, 5, 3},
        {"100_nodes__degree_23", 100, 23, 10},
        {"1000_nodes__degree_43", 1000, 43, 20},
        {"10000_nodes__degree_300", 10000, 301, 100},
    }),
    [](const ::testing::TestParamInfo<
        SecretSharingHararyGraphParamTest_Valid::ParamType>& info) {
      return info.param.test_name;
    });

class SecretSharingHararyGraphParamTest_InvalidDegree
    : public ::testing::TestWithParam<HararyGraphParams> {};

TEST_P(SecretSharingHararyGraphParamTest_InvalidDegree,
       ConstructionFailsOnEvenDegree) {
  const HararyGraphParams& graph_params = GetParam();
  SecretSharingGraphFactory factory;
  EXPECT_DEATH(
      factory.CreateHararyGraph(graph_params.kNumNodes, graph_params.kDegree,
                                graph_params.kThreshold),
      absl::StrCat("degree must be odd, given value was ",
                   graph_params.kDegree));
}

INSTANTIATE_TEST_SUITE_P(
    SecretSharingHararyGraphParamTests,
    SecretSharingHararyGraphParamTest_InvalidDegree,
    ::testing::ValuesIn<HararyGraphParams>({
        {"10_nodes__degree_4", 10, 4, 2},
        {"50_nodes__degree_20", 50, 20, 10},
    }),
    [](const ::testing::TestParamInfo<
        SecretSharingHararyGraphParamTest_InvalidDegree::ParamType>& info) {
      return info.param.test_name;
    });

class SecretSharingHararyGraphParamTest_InvalidThreshold
    : public ::testing::TestWithParam<HararyGraphParams> {};

TEST_P(SecretSharingHararyGraphParamTest_InvalidThreshold,
       ConstructionFailsOnThresholdOutOfObounds) {
  const HararyGraphParams& graph_params = GetParam();
  SecretSharingGraphFactory factory;
  EXPECT_DEATH(
      factory.CreateHararyGraph(graph_params.kNumNodes, graph_params.kDegree,
                                graph_params.kThreshold),
      "");
}

INSTANTIATE_TEST_SUITE_P(
    SecretSharingHararyGraphParamTests,
    SecretSharingHararyGraphParamTest_InvalidThreshold,
    ::testing::ValuesIn<HararyGraphParams>({
        {"10_nodes__degree_4_under", 10, 4, -1},
        {"10_nodes__degree_4_over", 10, 4, 6},
        {"50_nodes__degree_20_under", 50, 20, -1},
        {"50_nodes__degree_20_over", 50, 20, 21},
    }),
    [](const ::testing::TestParamInfo<
        SecretSharingHararyGraphParamTest_InvalidThreshold::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace secagg
}  // namespace fcp
