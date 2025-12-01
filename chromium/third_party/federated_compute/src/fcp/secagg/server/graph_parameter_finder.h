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

#ifndef FCP_SECAGG_SERVER_GRAPH_PARAMETER_FINDER_H_
#define FCP_SECAGG_SERVER_GRAPH_PARAMETER_FINDER_H_

#include "fcp/base/monitoring.h"
#include "fcp/secagg/server/secagg_server_enums.pb.h"
#include "fcp/secagg/server/secagg_server_messages.pb.h"

namespace fcp {
namespace secagg {

// Represents the parameters that define a SecretSharingHararyGraph: its
// size, its degree (as Harary graphs are regular), and the threshold for
// reconstruction
struct HararyGraphParameters {
  int number_of_nodes;
  int degree;
  int threshold;
};

// Returns the HararyGraphParameters that result in an instance of
// subgraph-secagg with statistical security [kSecurityParameter] and failure
// probability less that 2**(-[kCorrectnessParameter]), assuming
// [number_of_clients_] participants and the threat model (adversarial rate,
// dropout rate, and adversary class) defined in [threat_model].
StatusOr<HararyGraphParameters> ComputeHararyGraphParameters(
    int number_of_clients, SecureAggregationRequirements threat_model);

// Check if the provided threshold [threshold] results in a secure protocol with
// [number_of_clients] clients and the parameters and adversary specified in
// [threat_model]
Status CheckFullGraphParameters(int number_of_clients, int threshold,
                                SecureAggregationRequirements threat_model);

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SERVER_GRAPH_PARAMETER_FINDER_H_
