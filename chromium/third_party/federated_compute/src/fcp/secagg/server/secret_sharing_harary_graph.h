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

#ifndef FCP_SECAGG_SERVER_SECRET_SHARING_HARARY_GRAPH_H_
#define FCP_SECAGG_SERVER_SECRET_SHARING_HARARY_GRAPH_H_

#include <vector>

#include "fcp/secagg/server/secret_sharing_graph.h"

namespace fcp {
namespace secagg {

// This class represents a regular undirected graph specifying, for each client,
// among which other clients it shares its keys and computes pairwise masks.
// The construction of the graph is randomized, by permuting node ids, which is
// crucial for security. More concretely, the graph is a (degree-1,
// num_nodes)-Harary graph with randomly permuted node labels and an additional
// self-edge in each node. For simplicity of the construction we require that
// the degree is odd.

// A SecretSharingHararyGraph(num_nodes, degree) graph can be constructed by
// putting all num_nodes nodes in a circle and, for each node: (i) adding
// degree/2 edges to the immediately preceding nodes, (ii) adding degree/2 edges
// to the immediately successive nodes, and (iii) adding a self edge. Finally,
// nodes are given ids in [0..num_nodes - 1] uniformaly at random (or
// equivalently one applies a permutation to the original ids 0...num_nodes -
// 1).

// For example, if degree = 5 and num_nodes = 10, the adjacency list of a
// (num_nodes, degree)-SecretSharingHararyGraph (before permuting ids) is:
//
// 0 -> 8, 9, 0, 1, 2
// 1 -> 9, 0, 1, 2, 3
// 2 -> 0, 1, 2, 3, 4
// 3 -> 1, 2, 3, 4, 5
// 4 -> 2, 3, 4, 5, 6
// 5 -> 3, 4, 5, 6, 7
// 6 -> 4, 5, 6, 7, 8
// 7 -> 5, 6, 7, 8, 9
// 8 -> 6, 7, 8, 9, 0
// 9 -> 7, 8, 9, 0, 1
//
//
// SecretSharingHararyGraph additionally have permuted node ids (iff is_random
// == true) according to a uniformly random permutation. For example, if that
// permutation was (3, 2, 5, 4, 1, 8, 9, 0, 6, 7) then the resulting
// SecretSharingHararyGraph is the result of applying the permutation (aka node
// renaming) to the above adjacency list:

// 3 -> 6, 7, 3, 2, 5
// 2 -> 7, 3, 2, 5, 4
// 5 -> 3, 2, 5, 4, 1
// 4 -> 2, 5, 4, 1, 8
// 1 -> 5, 4, 1, 8, 9
// 8 -> 4, 1, 8, 9, 0
// 9 -> 1, 8, 9, 0, 6
// 0 -> 8, 9, 0, 6, 7
// 6 -> 9, 0, 6, 7, 3
// 7 -> 0, 6, 7, 3, 1

// Thus, the outgoing neighbors of 3 are 6, 7, 3, The outgoing neighbors of 2
// are 7, 3, 2, and so on.

// Although the above example aludes to an adjacency list based representation
// of the graph, this is only for clarity, as this is not stored explicitly.
// Instead, storing the random permutation (that is (3, 2, 5, 4, 1, 8, 9, 0, 6,
// 7) in the above example) and its inverse (which is (7, 4, 1, 0, 3, 2,
// 8, 9, 5, 6)) leads to a more space efficient implementation with constant
// time cost for all class functions.

// This class must be instantiated through SecretSharingGraphFactory.
class SecretSharingHararyGraph : public SecretSharingGraph {
 public:
  SecretSharingHararyGraph(const SecretSharingHararyGraph&) = delete;
  SecretSharingHararyGraph& operator=(const SecretSharingHararyGraph&) = delete;
  ~SecretSharingHararyGraph() override = default;

  int GetNumNodes() const override { return number_of_nodes_; }

  int GetDegree() const override { return degree_; }

  int GetThreshold() const override { return threshold_; }

  int GetNeighbor(int curr_node, int neighbor_index) const override;

  std::optional<int> GetNeighborIndex(int node_1, int node_2) const override;

  bool AreNeighbors(int node_1, int node_2) const override;

  bool IsOutgoingNeighbor(int node_1, int node_2) const override;

  // Returns the permutation that was applied to the nodes in the construction.
  // This function is only used for testing purposes.
  std::vector<int> GetPermutationForTesting() const { return permutation_; }

 private:
  int number_of_nodes_;
  int degree_;
  int threshold_;
  // random permutation applied to the node ids in the SecretSharingHararyGraph
  // construction.
  const std::vector<int> permutation_;
  // Inverse of the above permutation.
  std::vector<int> inverse_permutation_;
  SecretSharingHararyGraph(int degree, int threshold,
                           std::vector<int> permutation);
  friend class SecretSharingGraphFactory;

  bool IsValidNode(int node) const {
    return 0 <= node && node < number_of_nodes_;
  }

  bool IsValidNeighborIndex(int index) const {
    return 0 <= index && index < degree_;
  }
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SERVER_SECRET_SHARING_HARARY_GRAPH_H_
