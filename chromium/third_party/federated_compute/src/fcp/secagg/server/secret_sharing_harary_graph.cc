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

#include "fcp/secagg/server/secret_sharing_harary_graph.h"

#include <algorithm>
#include <utility>

#include "absl/strings/str_cat.h"
#include "fcp/base/monitoring.h"
#include "fcp/secagg/server/secret_sharing_graph.h"

namespace fcp {
namespace secagg {

SecretSharingHararyGraph::SecretSharingHararyGraph(int degree, int threshold,
                                                   std::vector<int> permutation)
    : number_of_nodes_(static_cast<int>(permutation.size())),
      degree_(degree),
      threshold_(threshold),
      permutation_(std::move(permutation)) {
  inverse_permutation_ = std::vector<int>(number_of_nodes_);
  for (int i = 0; i < number_of_nodes_; ++i) {
    inverse_permutation_[permutation_[i]] = i;
  }
}

int SecretSharingHararyGraph::GetNeighbor(int curr_node,
                                          int neighbor_index) const {
  FCP_CHECK(IsValidNode(curr_node));
  FCP_CHECK(IsValidNeighborIndex(neighbor_index));

  int curr_node_before_renaming = inverse_permutation_[curr_node];
  int zeroth_neighbor_before_renaming = curr_node_before_renaming - degree_ / 2;
  // We add number_of_nodes_ as a way to handle negative numbers
  return permutation_[(zeroth_neighbor_before_renaming + neighbor_index +
                       number_of_nodes_) %
                      number_of_nodes_];
}

std::optional<int> SecretSharingHararyGraph::GetNeighborIndex(
    int node_1, int node_2) const {
  FCP_CHECK(IsValidNode(node_1));
  FCP_CHECK(IsValidNode(node_2));
  int sub_before_renaming =
      std::abs(inverse_permutation_[node_1] - inverse_permutation_[node_2]);
  // Compute distance between nodes before applying the permutation.
  // node_1 and node_2 are connected iff, before renaming, they were at modular
  // distance <= degree_ / 2
  int mod_dist_before_renaming =
      std::min(sub_before_renaming, number_of_nodes_ - sub_before_renaming);
  if (mod_dist_before_renaming > degree_ / 2) {
    return {};
  }
  // Check that node_2 occurs before node_1 in the list of neighbors of node_1,
  // i.e. node_2 is an incoming neighbor of node_1
  // We add number_of_nodes_ as a way to handle negative numbers
  if ((inverse_permutation_[node_1] - mod_dist_before_renaming +
       number_of_nodes_) %
          number_of_nodes_ ==
      inverse_permutation_[node_2]) {
    return degree_ / 2 - mod_dist_before_renaming;
  }
  return degree_ / 2 + mod_dist_before_renaming;
}

bool SecretSharingHararyGraph::AreNeighbors(int node_1, int node_2) const {
  FCP_CHECK(IsValidNode(node_1));
  FCP_CHECK(IsValidNode(node_2));
  return GetNeighborIndex(node_1, node_2).value_or(-1) >= 0;
}

bool SecretSharingHararyGraph::IsOutgoingNeighbor(int node_1,
                                                  int node_2) const {
  FCP_CHECK(IsValidNode(node_1));
  FCP_CHECK(IsValidNode(node_2));
  return GetNeighborIndex(node_1, node_2).value_or(-1) >= degree_ / 2;
}

}  // namespace secagg
}  // namespace fcp
