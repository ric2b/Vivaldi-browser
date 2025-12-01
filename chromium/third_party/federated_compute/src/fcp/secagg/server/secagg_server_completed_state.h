/*
 * Copyright 2018 Google LLC
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

#ifndef FCP_SECAGG_SERVER_SECAGG_SERVER_COMPLETED_STATE_H_
#define FCP_SECAGG_SERVER_SECAGG_SERVER_COMPLETED_STATE_H_

#include <memory>
#include <string>

#include "fcp/secagg/server/secagg_server_state.h"
#include "fcp/secagg/shared/secagg_vector.h"

namespace fcp {
namespace secagg {

// This class is the State for the SecAggServer after it has successfully
// completed the protocol. The server cannot transition out of this state; a new
// SecAggServer object will be needed to start a new run of the protocol.
// This state stores information about the final state of the protocol, such as
// the number of inputs included in the output.

class SecAggServerCompletedState : public SecAggServerState {
 public:
  SecAggServerCompletedState(
      std::unique_ptr<SecAggServerProtocolImpl> impl,
      int number_of_clients_failed_after_sending_masked_input,
      int number_of_clients_failed_before_sending_masked_input,
      int number_of_clients_terminated_without_unmasking);

  ~SecAggServerCompletedState() override;

  // Returns true.
  bool IsCompletedSuccessfully() const override;

  int NumberOfIncludedInputs() const override;

  bool IsNumberOfIncludedInputsCommitted() const override;

  StatusOr<std::unique_ptr<SecAggVectorMap> > Result() override;
};
}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SERVER_SECAGG_SERVER_COMPLETED_STATE_H_
