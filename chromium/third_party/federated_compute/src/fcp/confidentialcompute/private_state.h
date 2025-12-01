
// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FCP_CONFIDENTIALCOMPUTE_PRIVATE_STATE_H_
#define FCP_CONFIDENTIALCOMPUTE_PRIVATE_STATE_H_

namespace fcp {
namespace confidential_compute {

// Configuration ID used for private state blobs written in one or more
// WriteConfigurationRequests. See:
// https://github.com/google-parfait/federated-compute/blob/a37f3ed144b93c33b174a26384ba5a0a4c0e5f4d/fcp/protos/confidentialcompute/confidential_transform.proto#L122
constexpr char kPrivateStateConfigId[] = "pipeline_private_state";

}  // namespace confidential_compute
}  // namespace fcp

#endif  // FCP_CONFIDENTIALCOMPUTE_PRIVATE_STATE_H_
