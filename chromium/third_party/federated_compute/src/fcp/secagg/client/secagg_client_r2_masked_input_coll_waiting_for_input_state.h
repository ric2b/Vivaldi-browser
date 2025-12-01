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

#ifndef FCP_SECAGG_CLIENT_SECAGG_CLIENT_R2_MASKED_INPUT_COLL_WAITING_FOR_INPUT_STATE_H_
#define FCP_SECAGG_CLIENT_SECAGG_CLIENT_R2_MASKED_INPUT_COLL_WAITING_FOR_INPUT_STATE_H_

#include <cstdint>
#include <memory>
#include <set>
#include <string>

#include "absl/container/node_hash_map.h"
#include "fcp/base/monitoring.h"
#include "fcp/secagg/client/other_client_state.h"
#include "fcp/secagg/client/secagg_client_r2_masked_input_coll_base_state.h"
#include "fcp/secagg/client/secagg_client_state.h"
#include "fcp/secagg/client/send_to_server_interface.h"
#include "fcp/secagg/client/state_transition_listener_interface.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/secagg/shared/shamir_secret_sharing.h"

namespace fcp {
namespace secagg {

// This class represents the client's Round 2: Masked Input Collection state
// where the client has already received a message from the server, but is
// waiting for the external protocol to set the client's input before replying.

// This state should transition to the Round 3: Unmasking state, but can also
// transition directly to the Completed or Aborted states.

class SecAggClientR2MaskedInputCollWaitingForInputState
    : public SecAggClientR2MaskedInputCollBaseState {
 public:
  SecAggClientR2MaskedInputCollWaitingForInputState(
      uint32_t client_id,
      uint32_t minimum_surviving_neighbors_for_reconstruction,
      uint32_t number_of_alive_neighbors, uint32_t number_of_neighbors,
      std::unique_ptr<std::vector<InputVectorSpecification> >
          input_vector_specs,
      std::unique_ptr<SecAggVectorMap> map_of_masks,
      std::unique_ptr<std::vector<OtherClientState> > other_client_states,
      std::unique_ptr<std::vector<ShamirShare> > pairwise_key_shares,
      std::unique_ptr<std::vector<ShamirShare> > self_key_shares,
      std::unique_ptr<SendToServerInterface> sender,
      std::unique_ptr<StateTransitionListenerInterface> transition_listener,

      AsyncAbort* async_abort = nullptr);

  ~SecAggClientR2MaskedInputCollWaitingForInputState() override;

  // This state handles only abort/early success messages. All others raise an
  // error status.
  StatusOr<std::unique_ptr<SecAggClientState> > HandleMessage(
      const ServerToClientWrapperMessage& message) override;

  StatusOr<std::unique_ptr<SecAggClientState> > SetInput(
      std::unique_ptr<SecAggVectorMap> input_map) override;

  // Returns the name of this state, "R2_MASKED_INPUT_COLL_WAITING_FOR_INPUT".
  ABSL_MUST_USE_RESULT std::string StateName() const override;

 private:
  const uint32_t client_id_;
  const uint32_t minimum_surviving_neighbors_for_reconstruction_;
  uint32_t number_of_alive_neighbors_;
  const uint32_t number_of_neighbors_;
  std::unique_ptr<std::vector<InputVectorSpecification> > input_vector_specs_;
  std::unique_ptr<SecAggVectorMap> map_of_masks_;
  std::unique_ptr<std::vector<OtherClientState> > other_client_states_;
  std::unique_ptr<std::vector<ShamirShare> > pairwise_key_shares_;
  std::unique_ptr<std::vector<ShamirShare> > self_key_shares_;
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_CLIENT_SECAGG_CLIENT_R2_MASKED_INPUT_COLL_WAITING_FOR_INPUT_STATE_H_
