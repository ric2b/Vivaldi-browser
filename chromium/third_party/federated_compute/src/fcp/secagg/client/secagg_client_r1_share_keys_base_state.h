/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FCP_SECAGG_CLIENT_SECAGG_CLIENT_R1_SHARE_KEYS_BASE_STATE_H_
#define FCP_SECAGG_CLIENT_SECAGG_CLIENT_R1_SHARE_KEYS_BASE_STATE_H_

#include <memory>
#include <string>
#include <vector>

#include "fcp/secagg/client/other_client_state.h"
#include "fcp/secagg/client/secagg_client_alive_base_state.h"
#include "fcp/secagg/client/send_to_server_interface.h"
#include "fcp/secagg/client/state_transition_listener_interface.h"
#include "fcp/secagg/shared/aes_key.h"
#include "fcp/secagg/shared/compute_session_id.h"
#include "fcp/secagg/shared/ecdh_key_agreement.h"
#include "fcp/secagg/shared/prng.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/secagg/shared/shamir_secret_sharing.h"

namespace fcp {
namespace secagg {

// This is an abstract class which is the parent of two possible state classes
// representing the states that the client may be in at Round 1: Share Keys.
// It should never be instantiated directly, but contains code that will be used
// by both concrete Round 1 client classes.

class SecAggClientR1ShareKeysBaseState : public SecAggClientAliveBaseState {
 public:
  ~SecAggClientR1ShareKeysBaseState() override = default;

 protected:
  // SecAggClientR1ShareKeysBaseState should never be instantiated directly.
  explicit SecAggClientR1ShareKeysBaseState(
      std::unique_ptr<SendToServerInterface> sender,
      std::unique_ptr<StateTransitionListenerInterface> transition_listener,

      AsyncAbort* async_abort = nullptr);

  void SetUpShares(int threshold, int n, const Key& agreement_key,
                   const Key& self_prng_key,
                   std::vector<ShamirShare>* self_prng_key_shares,
                   std::vector<ShamirShare>* pairwise_prng_key_shares);

  // Handles the logic associated with receiving a ShareKeysRequest. Uses the
  // ECDH public keys of other clients to compute shared secrets with other
  // clients, and shares its own private keys to send to the server.
  //
  // The arguments following prng are outputs. The vectors should be empty prior
  // to calling this method.
  //
  // The output will be false if an error was detected; this error will be
  // stored in *error_message. If the protocol should proceed, the output will
  // be true and *error_message will be an empty string.
  bool HandleShareKeysRequest(
      const ShareKeysRequest& request,
      const EcdhKeyAgreement& enc_key_agreement,
      uint32_t max_neighbors_expected,
      uint32_t minimum_surviving_neighbors_for_reconstruction,
      const EcdhKeyAgreement& prng_key_agreement, const AesKey& self_prng_key,
      SecurePrng* prng, uint32_t* client_id, std::string* error_message,
      uint32_t* number_of_alive_clients, uint32_t* number_of_clients,
      std::vector<AesKey>* other_client_enc_keys,
      std::vector<AesKey>* other_client_prng_keys,
      std::vector<OtherClientState>* other_client_states,
      std::vector<ShamirShare>* self_prng_key_shares,
      std::vector<ShamirShare>* pairwise_prng_key_shares,
      SessionId* session_id);

  // Individually encrypts each pair of key shares with the agreed-upon key for
  // the client that share is for, and then sends the encrypted keys to the
  // server. Dropped-out clients and this client are represented by empty
  // strings.  Returns true if successful, false if aborted by client.
  bool EncryptAndSendResponse(
      const std::vector<AesKey>& other_client_enc_keys,
      const std::vector<ShamirShare>& pairwise_prng_key_shares,
      const std::vector<ShamirShare>& self_prng_key_shares,
      SendToServerInterface* sender);
};

}  // namespace secagg
}  // namespace fcp
#endif  // FCP_SECAGG_CLIENT_SECAGG_CLIENT_R1_SHARE_KEYS_BASE_STATE_H_
