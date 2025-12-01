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

#include "fcp/secagg/client/secagg_client_r0_advertise_keys_input_set_state.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/node_hash_map.h"
#include "fcp/base/monitoring.h"
#include "fcp/secagg/client/secagg_client_aborted_state.h"
#include "fcp/secagg/client/secagg_client_completed_state.h"
#include "fcp/secagg/client/secagg_client_r1_share_keys_input_set_state.h"
#include "fcp/secagg/client/secagg_client_state.h"
#include "fcp/secagg/client/send_to_server_interface.h"
#include "fcp/secagg/client/state_transition_listener_interface.h"
#include "fcp/secagg/shared/aes_prng_factory.h"
#include "fcp/secagg/shared/ecdh_key_agreement.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/secagg/shared/secagg_vector.h"

namespace fcp {
namespace secagg {

SecAggClientR0AdvertiseKeysInputSetState::
    SecAggClientR0AdvertiseKeysInputSetState(
        uint32_t max_neighbors_expected,
        uint32_t minimum_surviving_neighbors_for_reconstruction,
        std::unique_ptr<SecAggVectorMap> input_map,
        std::unique_ptr<std::vector<InputVectorSpecification> >
            input_vector_specs,
        std::unique_ptr<SecurePrng> prng,
        std::unique_ptr<SendToServerInterface> sender,
        std::unique_ptr<StateTransitionListenerInterface> transition_listener,

        std::unique_ptr<AesPrngFactory> prng_factory, AsyncAbort* async_abort)
    : SecAggClientAliveBaseState(std::move(sender),
                                 std::move(transition_listener),
                                 ClientState::R0_ADVERTISE_KEYS, async_abort),
      max_neighbors_expected_(max_neighbors_expected),
      minimum_surviving_neighbors_for_reconstruction_(
          minimum_surviving_neighbors_for_reconstruction),
      input_map_(std::move(input_map)),
      input_vector_specs_(std::move(input_vector_specs)),
      prng_(std::move(prng)),
      prng_factory_(std::move(prng_factory)) {}

SecAggClientR0AdvertiseKeysInputSetState::
    ~SecAggClientR0AdvertiseKeysInputSetState() = default;

StatusOr<std::unique_ptr<SecAggClientState> >
SecAggClientR0AdvertiseKeysInputSetState::Start() {
  auto enc_key_agreement = EcdhKeyAgreement::CreateFromRandomKeys().value();
  auto prng_key_agreement = EcdhKeyAgreement::CreateFromRandomKeys().value();

  ClientToServerWrapperMessage message;
  PairOfPublicKeys* public_keys =
      message.mutable_advertise_keys()->mutable_pair_of_public_keys();
  public_keys->set_enc_pk(enc_key_agreement->PublicKey().AsString());
  public_keys->set_noise_pk(prng_key_agreement->PublicKey().AsString());

  sender_->Send(&message);
  return {std::make_unique<SecAggClientR1ShareKeysInputSetState>(
      max_neighbors_expected_, minimum_surviving_neighbors_for_reconstruction_,
      std::move(enc_key_agreement), std::move(input_map_),
      std::move(input_vector_specs_), std::move(prng_),
      std::move(prng_key_agreement), std::move(sender_),
      std::move(transition_listener_), std::move(prng_factory_), async_abort_)};
}

StatusOr<std::unique_ptr<SecAggClientState> >
SecAggClientR0AdvertiseKeysInputSetState::HandleMessage(
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

std::string SecAggClientR0AdvertiseKeysInputSetState::StateName() const {
  return "R0_ADVERTISE_KEYS_INPUT_SET";
}

}  // namespace secagg
}  // namespace fcp
