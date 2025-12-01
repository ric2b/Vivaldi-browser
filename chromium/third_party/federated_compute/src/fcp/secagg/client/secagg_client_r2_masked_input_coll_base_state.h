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

#ifndef FCP_SECAGG_CLIENT_SECAGG_CLIENT_R2_MASKED_INPUT_COLL_BASE_STATE_H_
#define FCP_SECAGG_CLIENT_SECAGG_CLIENT_R2_MASKED_INPUT_COLL_BASE_STATE_H_

#include <cstdint>
#include <memory>
#include <set>
#include <string>

#include "absl/container/node_hash_map.h"
#include "fcp/base/monitoring.h"
#include "fcp/secagg/client/other_client_state.h"
#include "fcp/secagg/client/secagg_client_alive_base_state.h"
#include "fcp/secagg/client/secagg_client_state.h"
#include "fcp/secagg/client/send_to_server_interface.h"
#include "fcp/secagg/client/state_transition_listener_interface.h"
#include "fcp/secagg/shared/aes_key.h"
#include "fcp/secagg/shared/aes_prng_factory.h"
#include "fcp/secagg/shared/compute_session_id.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/map_of_masks.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/secagg/shared/secagg_vector.h"
#include "fcp/secagg/shared/shamir_secret_sharing.h"

namespace fcp {
namespace secagg {

// This is an abstract class which is the parent of three possible state classes
// representing the states that the client may be in at Round 2: Masked Input
// Collection. It should never be instantiated directly, but defines code that
// will be used by multiple concrete Round 2 classes.

class SecAggClientR2MaskedInputCollBaseState
    : public SecAggClientAliveBaseState {
 public:
  ~SecAggClientR2MaskedInputCollBaseState() override;

 protected:
  // SecAggClientR2MaskedInputCollBaseState should never be instantiated
  // directly.
  explicit SecAggClientR2MaskedInputCollBaseState(
      std::unique_ptr<SendToServerInterface> sender,
      std::unique_ptr<StateTransitionListenerInterface> transition_listener,

      AsyncAbort* async_abort = nullptr);

  // Handles the logic associated with receiving a MaskedInputCollectionRequest.
  // Adds the recovered key shares to pairwise_key_shares and self_key_shares.
  // Updates the other_client_states and number_of_alive_clients based on
  // dropouts recorded in the request.
  //
  // The return value is computed map of masks if everything succeeed.
  // If there was a failure, the return value is nullptr, and error_message is
  // set to a non-empty string.
  std::unique_ptr<SecAggVectorMap> HandleMaskedInputCollectionRequest(
      const MaskedInputCollectionRequest& request, uint32_t client_id,
      const std::vector<InputVectorSpecification>& input_vector_specs,
      uint32_t minimum_surviving_neighbors_for_reconstruction,
      uint32_t number_of_clients,
      const std::vector<AesKey>& other_client_enc_keys,
      const std::vector<AesKey>& other_client_prng_keys,
      const ShamirShare& own_self_key_share, const AesKey& self_prng_key,
      const SessionId& session_id, const AesPrngFactory& prng_factory,
      uint32_t* number_of_alive_clients,
      std::vector<OtherClientState>* other_client_states,
      std::vector<ShamirShare>* pairwise_key_shares,
      std::vector<ShamirShare>* self_key_shares, std::string* error_message);

  // Consumes a map of masks to the input map and sends the result of adding
  // the two to the server.
  void SendMaskedInput(std::unique_ptr<SecAggVectorMap> input_map,
                       std::unique_ptr<SecAggVectorMap> map_of_masks);
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_CLIENT_SECAGG_CLIENT_R2_MASKED_INPUT_COLL_BASE_STATE_H_
