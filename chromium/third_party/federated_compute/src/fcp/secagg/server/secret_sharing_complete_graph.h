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

#ifndef FCP_SECAGG_SERVER_SECRET_SHARING_COMPLETE_GRAPH_H_
#define FCP_SECAGG_SERVER_SECRET_SHARING_COMPLETE_GRAPH_H_

#include "absl/strings/str_cat.h"
#include "fcp/base/monitoring.h"
#include "fcp/secagg/server/secret_sharing_graph.h"

namespace fcp {
namespace secagg {

// SecretSharingGraph built from a complete (directed) graph.
// For example, in a SecretSharingCompleteGraph with 4
// nodes the list of neighbors of each node are:
// 0 -> 0, 1, 2, 3
// 1 -> 0, 1, 2, 3
// 2 -> 0, 1, 2, 3
// 3 -> 0, 1, 2, 3
// Thus, the (single) outgoing neighbor of 0 is 0.
// The outgoing neighbors of 1 are 0, 1.
// The outgoing neighbors of 2 are 0, 1, 2.
// The outgoing neighbors of 3 are 0, 1, 2, 3.

// SecretSharingCompleteGraph must be instantiated via
// SecretSharingGraphFactory.
class SecretSharingCompleteGraph : public SecretSharingGraph {
 public:
  SecretSharingCompleteGraph(const SecretSharingCompleteGraph&) = delete;
  SecretSharingCompleteGraph& operator=(const SecretSharingCompleteGraph&) =
      delete;
  ~SecretSharingCompleteGraph() override = default;

  int GetNumNodes() const override { return num_nodes_; }

  int GetDegree() const override {
    // All nodes have degree num_nodes.
    return num_nodes_;
  }

  int GetThreshold() const override { return threshold_; }

  int GetNeighbor(int curr_node, int i) const override {
    FCP_CHECK(IsValidNode(curr_node));
    FCP_CHECK(IsValidNode(i));  // i must be in [0, num_nodes)
    // Each node has all other nodes as a neighbor, including itself.
    return i;
  }

  std::optional<int> GetNeighborIndex(int node_1, int node_2) const override {
    // Lists of neighbors are sorted by client id
    FCP_CHECK(IsValidNode(node_1));
    FCP_CHECK(IsValidNode(node_2));
    return node_2;
  }

  bool AreNeighbors(int node_1, int node_2) const override {
    FCP_CHECK(IsValidNode(node_1));
    FCP_CHECK(IsValidNode(node_2));
    return true;
  }

  bool IsOutgoingNeighbor(int node_1, int node_2) const override {
    FCP_CHECK(IsValidNode(node_1));
    FCP_CHECK(IsValidNode(node_2));
    return node_2 >= node_1;
  }

 private:
  // Number of nodes in the graph, with indices [0, num_nodes).
  int num_nodes_;
  int threshold_;
  explicit SecretSharingCompleteGraph(int num_nodes, int threshold)
      : num_nodes_(num_nodes), threshold_(threshold) {}
  friend class SecretSharingGraphFactory;

  bool IsValidNode(int node) const { return 0 <= node && node < num_nodes_; }
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SERVER_SECRET_SHARING_COMPLETE_GRAPH_H_
