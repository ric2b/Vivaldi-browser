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

#ifndef FCP_SECAGG_SERVER_SECRET_SHARING_GRAPH_H_
#define FCP_SECAGG_SERVER_SECRET_SHARING_GRAPH_H_

#include "fcp/base/monitoring.h"

namespace fcp {
namespace secagg {

// Abstract class representing a regular directed graph.
// Nodes are integers in [0..NumNodes - 1]. For each node i (representing client
// with id i), the graph specifies the set of other nodes with which i shares
// its keys and computes pairwise masks. The graph is directed, and the
// neighbors of each node are ordered (i.e. we have a notion of the 1st neighbor
// of client i, the second neighbor, etc).

// The intuitive way to visualize the graph is by means of a complete mapping
// between nodes and the *ordered* lists of their neighbors (of length the
// degree k):

// 0 - > 1st_neighbor_of_0, ..., kth_neighbor_of_0
// 1 - > 1st_neighbor_of_1, ..., kth_neighbor_of_1
//     ...
// n-1 - > 1st_neighbor_of_n-1, ..., kth_neighbor_of_n-1

// For every node i, its list of neighbors includes i, because as mentioned
// above *each node has a self loop*.

// The direction of each edge adjacent to node i is given implicitly by the
// order of the neighbors of i: nodes occurring in the list of neighbors of i
// *strictly* after i itself are called *outgoing* neighbors, and nodes ocurring
// before i (including i itself) are called *incoming* neighbors.

// The SecretSharingGraph class includes functions to (a) retrieve the index of
// a neighbor of a node in the list of neighbors , (b) retrieve the neighbor of
// a node at a given index, and (c) check if a nodes are neighbors, and of which
// kind (i.e. incoming vs outgoing).

// There are multiple subclasses of SecretSharingGraph. The complete graph
// variant implemented as the SecretSharingCompleteGraph subclass, and the
// (random) Harary graph variant implemented as the SecretSharingCompleteGraph
// subclass.
class SecretSharingGraph {
 public:
  virtual ~SecretSharingGraph() = default;

  // Returns the number of nodes in the graph.
  virtual int GetNumNodes() const = 0;

  // Returns the degree of the graph.
  virtual int GetDegree() const = 0;

  // Returns the threshold of the secret sharing
  virtual int GetThreshold() const = 0;

  // Returns curr_node's ith neighbor.
  // This function assumes that 0 <= i < GetDegree() and will throw a runtime
  // error if that's not the case
  virtual int GetNeighbor(int curr_node, int i) const = 0;

  // Returns the index of node_2 in the list of neighbors of node_1, if present
  virtual std::optional<int> GetNeighborIndex(int node_1, int node_2) const = 0;

  // Returns true if node_1 and node_2 are neighbors, else false.
  virtual bool AreNeighbors(int node_1, int node_2) const = 0;

  // Returns true if node_1 is an outgoing neighbor of node_2, else false.
  virtual bool IsOutgoingNeighbor(int node_1, int node_2) const = 0;
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SERVER_SECRET_SHARING_GRAPH_H_
