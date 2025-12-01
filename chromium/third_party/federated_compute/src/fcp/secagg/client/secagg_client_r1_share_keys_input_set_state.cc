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

#include "fcp/secagg/client/secagg_client_r1_share_keys_input_set_state.h"

#include <cstdint>
#include <string>
#include <utility>

#include "fcp/base/monitoring.h"
#include "fcp/secagg/client/other_client_state.h"
#include "fcp/secagg/client/secagg_client_aborted_state.h"
#include "fcp/secagg/client/secagg_client_completed_state.h"
#include "fcp/secagg/client/secagg_client_r2_masked_input_coll_input_set_state.h"
#include "fcp/secagg/client/secagg_client_state.h"
#include "fcp/secagg/client/send_to_server_interface.h"
#include "fcp/secagg/client/state_transition_listener_interface.h"
#include "fcp/secagg/shared/aes_prng_factory.h"
#include "fcp/secagg/shared/ecdh_key_agreement.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/secagg_vector.h"
#include "fcp/secagg/shared/shamir_secret_sharing.h"

namespace fcp {
namespace secagg {
SecAggClientR1ShareKeysInputSetState::SecAggClientR1ShareKeysInputSetState(
    uint32_t max_neighbors_expected,
    uint32_t minimum_surviving_neighbors_for_reconstruction,
    std::unique_ptr<EcdhKeyAgreement> enc_key_agreement,
    std::unique_ptr<SecAggVectorMap> input_map,
    std::unique_ptr<std::vector<InputVectorSpecification> > input_vector_specs,
    std::unique_ptr<SecurePrng> prng,
    std::unique_ptr<EcdhKeyAgreement> prng_key_agreement,
    std::unique_ptr<SendToServerInterface> sender,
    std::unique_ptr<StateTransitionListenerInterface> transition_listener,
    std::unique_ptr<AesPrngFactory> prng_factory, AsyncAbort* async_abort)
    : SecAggClientR1ShareKeysBaseState(
          std::move(sender), std::move(transition_listener), async_abort),
      max_neighbors_expected_(max_neighbors_expected),
      minimum_surviving_neighbors_for_reconstruction_(
          minimum_surviving_neighbors_for_reconstruction),
      enc_key_agreement_(std::move(enc_key_agreement)),
      input_map_(std::move(input_map)),
      input_vector_specs_(std::move(input_vector_specs)),
      prng_(std::move(prng)),
      prng_key_agreement_(std::move(prng_key_agreement)),
      prng_factory_(std::move(prng_factory)) {}

StatusOr<std::unique_ptr<SecAggClientState> >
SecAggClientR1ShareKeysInputSetState::HandleMessage(
    const ServerToClientWrapperMessage& message) {
  // Handle abort messages or share keys requests only.
  if (message.has_abort()) {
    if (message.abort().early_success()) {
      return {std::make_unique<SecAggClientCompletedState>(
          std::move(sender_), std::move(transition_listener_))};
    } else {
      return {std::make_unique<SecAggClientAbortedState>(
          "Aborting because of abort message from the server.",
          std::move(sender_), std::move(transition_listener_))};
    }
  } else if (!message.has_share_keys_request()) {
    // Returns an error indicating that the message is of invalid type.
    return SecAggClientState::HandleMessage(message);
  }
  uint32_t client_id;
  uint32_t number_of_alive_clients;
  uint32_t number_of_clients;
  std::string error_message;
  auto other_client_enc_keys = std::make_unique<std::vector<AesKey> >();
  auto other_client_prng_keys = std::make_unique<std::vector<AesKey> >();
  auto other_client_states = std::make_unique<std::vector<OtherClientState> >();
  auto own_self_key_share = std::make_unique<ShamirShare>();
  auto session_id = std::make_unique<SessionId>();

  uint8_t self_prng_key_buffer[AesKey::kSize];
  for (uint8_t& i : self_prng_key_buffer) {
    i = prng_->Rand8();
  }
  auto self_prng_key = std::make_unique<AesKey>(self_prng_key_buffer);

  bool success = HandleShareKeysRequest(
      message.share_keys_request(), *enc_key_agreement_,
      max_neighbors_expected_, minimum_surviving_neighbors_for_reconstruction_,
      *prng_key_agreement_, *self_prng_key, prng_.get(), &client_id,
      &error_message, &number_of_alive_clients, &number_of_clients,
      other_client_enc_keys.get(), other_client_prng_keys.get(),
      other_client_states.get(), &self_prng_key_shares_,
      &pairwise_prng_key_shares_, session_id.get());

  if (!success) {
    return AbortAndNotifyServer(error_message);
  }

  if (async_abort_ && async_abort_->Signalled()) {
    return AbortAndNotifyServer(async_abort_->Message());
  }

  if (!EncryptAndSendResponse(*other_client_enc_keys, pairwise_prng_key_shares_,
                              self_prng_key_shares_, sender_.get())) {
    return AbortAndNotifyServer(async_abort_->Message());
  }

  *own_self_key_share = self_prng_key_shares_[client_id];
  return {std::make_unique<SecAggClientR2MaskedInputCollInputSetState>(
      client_id, minimum_surviving_neighbors_for_reconstruction_,
      number_of_alive_clients, number_of_clients, std::move(input_map_),
      std::move(input_vector_specs_), std::move(other_client_states),
      std::move(other_client_enc_keys), std::move(other_client_prng_keys),
      std::move(own_self_key_share), std::move(self_prng_key),
      std::move(sender_), std::move(transition_listener_),
      std::move(session_id), std::move(prng_factory_), async_abort_)};
}

std::string SecAggClientR1ShareKeysInputSetState::StateName() const {
  return "R1_SHARE_KEYS_INPUT_SET";
}

}  // namespace secagg
}  // namespace fcp
