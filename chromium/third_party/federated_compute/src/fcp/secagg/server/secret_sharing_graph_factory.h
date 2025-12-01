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

#ifndef FCP_SECAGG_SERVER_SECRET_SHARING_GRAPH_FACTORY_H_
#define FCP_SECAGG_SERVER_SECRET_SHARING_GRAPH_FACTORY_H_

#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>

#include "fcp/base/monitoring.h"
#include "fcp/secagg/server/secret_sharing_complete_graph.h"
#include "fcp/secagg/server/secret_sharing_graph.h"
#include "fcp/secagg/server/secret_sharing_harary_graph.h"
#include "fcp/secagg/server/ssl_bit_gen.h"

namespace fcp {
namespace secagg {

// Factory class that constructs non-copyable instances of children classes of
// SecretSharingGraph.
class SecretSharingGraphFactory {
 public:
  // Creates a SecretSharingCompleteGraph.
  static std::unique_ptr<SecretSharingCompleteGraph> CreateCompleteGraph(
      int num_nodes, int threshold) {
    FCP_CHECK(num_nodes >= 1)
        << "num_nodes must be >= 1, given value was " << num_nodes;
    FCP_CHECK(threshold >= 1)
        << "threshold must be >= 1, given value was " << threshold;
    FCP_CHECK(threshold <= num_nodes)
        << "threshold must be <= num_nodes, given values were " << threshold
        << ", " << num_nodes;
    return absl::WrapUnique(
        new SecretSharingCompleteGraph(num_nodes, threshold));
  }

  // Creates a SecretSharingHararyGraph.
  static std::unique_ptr<SecretSharingHararyGraph> CreateHararyGraph(
      int num_nodes, int degree, int threshold, bool is_random = true) {
    FCP_CHECK(num_nodes >= 1)
        << "num_nodes must be >= 1, given value was " << num_nodes;
    FCP_CHECK(degree <= num_nodes)
        << "degree must be <= num_nodes, given values were " << num_nodes
        << ", " << degree;
    FCP_CHECK(degree % 2 == 1)
        << "degree must be odd, given value was " << degree;
    FCP_CHECK(threshold >= 1)
        << "threshold must be >= 1, given value was " << threshold;
    FCP_CHECK(threshold <= degree)
        << "threshold must be <= degree, given values were " << threshold
        << ", " << degree;
    auto permutation = std::vector<int>(num_nodes);
    for (int i = 0; i < num_nodes; ++i) {
      permutation[i] = i;
    }
    if (is_random) {
      std::shuffle(permutation.begin(), permutation.end(), SslBitGen());
    }
    return absl::WrapUnique(new SecretSharingHararyGraph(
        degree, threshold, std::move(permutation)));
  }
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SERVER_SECRET_SHARING_GRAPH_FACTORY_H_
