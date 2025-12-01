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

#include "fcp/secagg/client/secagg_client_r2_masked_input_coll_input_not_set_state.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/node_hash_map.h"
#include "fcp/base/monitoring.h"
#include "fcp/secagg/client/other_client_state.h"
#include "fcp/secagg/client/secagg_client_aborted_state.h"
#include "fcp/secagg/client/secagg_client_completed_state.h"
#include "fcp/secagg/client/secagg_client_r2_masked_input_coll_base_state.h"
#include "fcp/secagg/client/secagg_client_r2_masked_input_coll_input_set_state.h"
#include "fcp/secagg/client/secagg_client_r2_masked_input_coll_waiting_for_input_state.h"
#include "fcp/secagg/client/secagg_client_state.h"
#include "fcp/secagg/client/send_to_server_interface.h"
#include "fcp/secagg/client/state_transition_listener_interface.h"
#include "fcp/secagg/shared/aes_key.h"
#include "fcp/secagg/shared/aes_prng_factory.h"
#include "fcp/secagg/shared/compute_session_id.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/secagg/shared/secagg_vector.h"
#include "fcp/secagg/shared/shamir_secret_sharing.h"

namespace fcp {
namespace secagg {

SecAggClientR2MaskedInputCollInputNotSetState::
    SecAggClientR2MaskedInputCollInputNotSetState(
        uint32_t client_id,
        uint32_t minimum_surviving_neighbors_for_reconstruction,
        uint32_t number_of_alive_neighbors, uint32_t number_of_neighbors,
        std::unique_ptr<std::vector<InputVectorSpecification> >
            input_vector_specs,
        std::unique_ptr<std::vector<OtherClientState> > other_client_states,
        std::unique_ptr<std::vector<AesKey> > other_client_enc_keys,
        std::unique_ptr<std::vector<AesKey> > other_client_prng_keys,
        std::unique_ptr<ShamirShare> own_self_key_share,
        std::unique_ptr<AesKey> self_prng_key,
        std::unique_ptr<SendToServerInterface> sender,
        std::unique_ptr<StateTransitionListenerInterface> transition_listener,

        std::unique_ptr<SessionId> session_id,
        std::unique_ptr<AesPrngFactory> prng_factory, AsyncAbort* async_abort)
    : SecAggClientR2MaskedInputCollBaseState(
          std::move(sender), std::move(transition_listener), async_abort),
      client_id_(client_id),
      minimum_surviving_neighbors_for_reconstruction_(
          minimum_surviving_neighbors_for_reconstruction),
      number_of_alive_neighbors_(number_of_alive_neighbors),
      number_of_neighbors_(number_of_neighbors),
      input_vector_specs_(std::move(input_vector_specs)),
      other_client_states_(std::move(other_client_states)),
      other_client_enc_keys_(std::move(other_client_enc_keys)),
      other_client_prng_keys_(std::move(other_client_prng_keys)),
      own_self_key_share_(std::move(own_self_key_share)),
      self_prng_key_(std::move(self_prng_key)),
      session_id_(std::move(session_id)),
      prng_factory_(std::move(prng_factory)) {
  FCP_CHECK(client_id_ >= 0)
      << "Client id must not be negative but was " << client_id_;
}

SecAggClientR2MaskedInputCollInputNotSetState::
    ~SecAggClientR2MaskedInputCollInputNotSetState() = default;

StatusOr<std::unique_ptr<SecAggClientState> >
SecAggClientR2MaskedInputCollInputNotSetState::HandleMessage(
    const ServerToClientWrapperMessage& message) {
  // Handle abort messages or masked input requests only.
  if (message.has_abort()) {
    if (message.abort().early_success()) {
      return {std::make_unique<SecAggClientCompletedState>(
          std::move(sender_), std::move(transition_listener_))};
    } else {
      return {std::make_unique<SecAggClientAbortedState>(
          "Aborting because of abort message from the server.",
          std::move(sender_), std::move(transition_listener_))};
    }
  } else if (!message.has_masked_input_request()) {
    // Returns an error indicating that the message is of invalid type.
    return SecAggClientState::HandleMessage(message);
  }

  const MaskedInputCollectionRequest& request = message.masked_input_request();
  std::string error_message;
  auto pairwise_key_shares = std::make_unique<std::vector<ShamirShare> >();
  auto self_key_shares = std::make_unique<std::vector<ShamirShare> >();

  std::unique_ptr<SecAggVectorMap> map_of_masks =
      HandleMaskedInputCollectionRequest(
          request, client_id_, *input_vector_specs_,
          minimum_surviving_neighbors_for_reconstruction_, number_of_neighbors_,
          *other_client_enc_keys_, *other_client_prng_keys_,
          *own_self_key_share_, *self_prng_key_, *session_id_, *prng_factory_,
          &number_of_alive_neighbors_, other_client_states_.get(),
          pairwise_key_shares.get(), self_key_shares.get(), &error_message);

  if (!map_of_masks) {
    return AbortAndNotifyServer(error_message);
  }

  return {std::make_unique<SecAggClientR2MaskedInputCollWaitingForInputState>(
      client_id_, minimum_surviving_neighbors_for_reconstruction_,
      number_of_alive_neighbors_, number_of_neighbors_,
      std::move(input_vector_specs_), std::move(map_of_masks),
      std::move(other_client_states_), std::move(pairwise_key_shares),
      std::move(self_key_shares), std::move(sender_),
      std::move(transition_listener_), async_abort_)};
}

StatusOr<std::unique_ptr<SecAggClientState> >
SecAggClientR2MaskedInputCollInputNotSetState::SetInput(
    std::unique_ptr<SecAggVectorMap> input_map) {
  if (!ValidateInput(*input_map, *input_vector_specs_)) {
    return FCP_STATUS(INVALID_ARGUMENT)
           << "The input to SetInput does not match the "
              "InputVectorSpecification.";
  }

  return {std::make_unique<SecAggClientR2MaskedInputCollInputSetState>(
      client_id_, minimum_surviving_neighbors_for_reconstruction_,
      number_of_alive_neighbors_, number_of_neighbors_, std::move(input_map),
      std::move(input_vector_specs_), std::move(other_client_states_),
      std::move(other_client_enc_keys_), std::move(other_client_prng_keys_),
      std::move(own_self_key_share_), std::move(self_prng_key_),
      std::move(sender_), std::move(transition_listener_),
      std::move(session_id_), std::move(prng_factory_), async_abort_)};
}

std::string SecAggClientR2MaskedInputCollInputNotSetState::StateName() const {
  return "R2_MASKED_INPUT_COLL_INPUT_NOT_SET";
}

}  // namespace secagg
}  // namespace fcp
