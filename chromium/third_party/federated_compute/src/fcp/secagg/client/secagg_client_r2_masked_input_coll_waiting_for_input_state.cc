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

#include "fcp/secagg/client/secagg_client_r2_masked_input_coll_waiting_for_input_state.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/container/node_hash_map.h"
#include "fcp/base/monitoring.h"
#include "fcp/secagg/client/other_client_state.h"
#include "fcp/secagg/client/secagg_client_aborted_state.h"
#include "fcp/secagg/client/secagg_client_completed_state.h"
#include "fcp/secagg/client/secagg_client_r2_masked_input_coll_base_state.h"
#include "fcp/secagg/client/secagg_client_r3_unmasking_state.h"
#include "fcp/secagg/client/secagg_client_state.h"
#include "fcp/secagg/client/send_to_server_interface.h"
#include "fcp/secagg/client/state_transition_listener_interface.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/secagg/shared/secagg_vector.h"
#include "fcp/secagg/shared/shamir_secret_sharing.h"

namespace fcp {
namespace secagg {

SecAggClientR2MaskedInputCollWaitingForInputState::
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

        AsyncAbort* async_abort)
    : SecAggClientR2MaskedInputCollBaseState(
          std::move(sender), std::move(transition_listener), async_abort),
      client_id_(client_id),
      minimum_surviving_neighbors_for_reconstruction_(
          minimum_surviving_neighbors_for_reconstruction),
      number_of_alive_neighbors_(number_of_alive_neighbors),
      number_of_neighbors_(number_of_neighbors),
      input_vector_specs_(std::move(input_vector_specs)),
      map_of_masks_(std::move(map_of_masks)),
      other_client_states_(std::move(other_client_states)),
      pairwise_key_shares_(std::move(pairwise_key_shares)),
      self_key_shares_(std::move(self_key_shares)) {
  FCP_CHECK(client_id_ >= 0)
      << "Client id must not be negative but was " << client_id_;
}

SecAggClientR2MaskedInputCollWaitingForInputState::
    ~SecAggClientR2MaskedInputCollWaitingForInputState() = default;

StatusOr<std::unique_ptr<SecAggClientState> >
SecAggClientR2MaskedInputCollWaitingForInputState::HandleMessage(
    const ServerToClientWrapperMessage& message) {
  // Handle abort messages only.
  if (message.has_abort()) {
    if (message.abort().early_success()) {
      return {std::make_unique<SecAggClientCompletedState>(
          std::move(sender_), std::move(transition_listener_))};
    } else {
      return {std::make_unique<SecAggClientAbortedState>(
          "Aborting because of abort message from the server.",
          std::move(sender_), std::move(transition_listener_))};
    }
  } else {
    // Returns an error indicating that the message is of invalid type.
    return SecAggClientState::HandleMessage(message);
  }
}

StatusOr<std::unique_ptr<SecAggClientState> >
SecAggClientR2MaskedInputCollWaitingForInputState::SetInput(
    std::unique_ptr<SecAggVectorMap> input_map) {
  // Only need to do 3 things: Validate input, send message to server, and
  // return the new state.
  if (!ValidateInput(*input_map, *input_vector_specs_)) {
    return FCP_STATUS(INVALID_ARGUMENT)
           << "The input to SetInput does not match the "
              "InputVectorSpecification.";
  }

  SendMaskedInput(std::move(input_map), std::move(map_of_masks_));

  return {std::make_unique<SecAggClientR3UnmaskingState>(
      client_id_, number_of_alive_neighbors_,
      minimum_surviving_neighbors_for_reconstruction_, number_of_neighbors_,
      std::move(other_client_states_), std::move(pairwise_key_shares_),
      std::move(self_key_shares_), std::move(sender_),
      std::move(transition_listener_), async_abort_)};
}

std::string SecAggClientR2MaskedInputCollWaitingForInputState::StateName()
    const {
  return "R2_MASKED_INPUT_COLL_WAITING_FOR_INPUT";
}

}  // namespace secagg
}  // namespace fcp
