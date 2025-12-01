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

#include "fcp/secagg/client/secagg_client_r1_share_keys_base_state.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "fcp/secagg/client/other_client_state.h"
#include "fcp/secagg/client/secagg_client_alive_base_state.h"
#include "fcp/secagg/client/secagg_client_state.h"
#include "fcp/secagg/client/send_to_server_interface.h"
#include "fcp/secagg/client/state_transition_listener_interface.h"
#include "fcp/secagg/shared/aes_gcm_encryption.h"
#include "fcp/secagg/shared/compute_session_id.h"
#include "fcp/secagg/shared/ecdh_key_agreement.h"
#include "fcp/secagg/shared/prng.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/secagg/shared/shamir_secret_sharing.h"

namespace fcp {
namespace secagg {

SecAggClientR1ShareKeysBaseState::SecAggClientR1ShareKeysBaseState(
    std::unique_ptr<SendToServerInterface> sender,
    std::unique_ptr<StateTransitionListenerInterface> transition_listener,
    AsyncAbort* async_abort)
    : SecAggClientAliveBaseState(std::move(sender),
                                 std::move(transition_listener),
                                 ClientState::R1_SHARE_KEYS, async_abort) {}

void SecAggClientR1ShareKeysBaseState::SetUpShares(
    int threshold, int n, const Key& agreement_key, const Key& self_prng_key,
    std::vector<ShamirShare>* self_prng_key_shares,
    std::vector<ShamirShare>* pairwise_prng_key_shares) {
  // This could be made into an assertion, but that would complicate the tests
  // that call this method to get a "preview" of the shares.
  if (pairwise_prng_key_shares->empty() && self_prng_key_shares->empty()) {
    ShamirSecretSharing sharer;
    *pairwise_prng_key_shares = sharer.Share(threshold, n, agreement_key);
    *self_prng_key_shares = sharer.Share(threshold, n, self_prng_key);
  }
}

bool SecAggClientR1ShareKeysBaseState::HandleShareKeysRequest(
    const ShareKeysRequest& request, const EcdhKeyAgreement& enc_key_agreement,
    uint32_t max_neighbors_expected,
    uint32_t minimum_surviving_neighbors_for_reconstruction,
    const EcdhKeyAgreement& prng_key_agreement, const AesKey& self_prng_key,
    SecurePrng* prng, uint32_t* client_id, std::string* error_message,
    uint32_t* number_of_alive_clients, uint32_t* number_of_clients,
    std::vector<AesKey>* other_client_enc_keys,
    std::vector<AesKey>* other_client_prng_keys,
    std::vector<OtherClientState>* other_client_states,
    std::vector<ShamirShare>* self_prng_key_shares,
    std::vector<ShamirShare>* pairwise_prng_key_shares, SessionId* session_id) {
  transition_listener_->set_execution_session_id(
      request.sec_agg_execution_logging_id());
  if (request.pairs_of_public_keys().size() <
      static_cast<int>(minimum_surviving_neighbors_for_reconstruction)) {
    *error_message =
        "The ShareKeysRequest received does not contain enough participants.";
    return false;
  } else if (request.pairs_of_public_keys().size() >
             static_cast<int>(max_neighbors_expected)) {
    *error_message =
        "The ShareKeysRequest received contains too many participants.";
    return false;
  }

  *number_of_alive_clients = request.pairs_of_public_keys().size();
  *number_of_clients = request.pairs_of_public_keys().size();
  bool client_id_set = false;

  SetUpShares(minimum_surviving_neighbors_for_reconstruction,
              *number_of_clients, prng_key_agreement.PrivateKey(),
              self_prng_key, self_prng_key_shares, pairwise_prng_key_shares);

  if (request.session_id().size() != kSha256Length) {
    *error_message =
        "Session ID is absent in ShareKeysRequest or has an unexpected length.";
    return false;
  }
  session_id->data = request.session_id();

  other_client_states->resize(*number_of_clients, OtherClientState::kAlive);
  other_client_enc_keys->reserve(*number_of_clients);
  other_client_prng_keys->reserve(*number_of_clients);

  EcdhPublicKey self_enc_public_key = enc_key_agreement.PublicKey();
  EcdhPublicKey self_prng_public_key = prng_key_agreement.PublicKey();

  for (uint32_t i = 0; i < *number_of_clients; ++i) {
    if (async_abort_ && async_abort_->Signalled()) {
      *error_message = async_abort_->Message();
      return false;
    }
    const PairOfPublicKeys& keys = request.pairs_of_public_keys(i);
    if (keys.enc_pk().empty() || keys.noise_pk().empty()) {
      // This is an aborted client, or it sent invalid keys.
      other_client_states->at(i) = OtherClientState::kDeadAtRound1;
      --(*number_of_alive_clients);
      other_client_enc_keys->push_back(AesKey());
      other_client_prng_keys->push_back(AesKey());
    } else if (keys.enc_pk().size() != EcdhPublicKey::kSize ||
               keys.noise_pk().size() != EcdhPublicKey::kSize) {
      // The server forwarded an invalid public key.
      *error_message = "Invalid public key in request from server.";
      return false;
    } else {
      EcdhPublicKey enc_pk(
          reinterpret_cast<const uint8_t*>(keys.enc_pk().data()));
      EcdhPublicKey prng_pk(
          reinterpret_cast<const uint8_t*>(keys.noise_pk().data()));
      if (enc_pk == self_enc_public_key && prng_pk == self_prng_public_key) {
        // This is this client.
        if (client_id_set) {
          *error_message =
              "Found this client's keys in the ShareKeysRequest twice somehow.";
          return false;
        }
        *client_id = i;
        client_id_set = true;
        // Add empty entries for own id.
        other_client_enc_keys->push_back(AesKey());
        other_client_prng_keys->push_back(AesKey());
      } else {
        auto shared_enc_key = enc_key_agreement.ComputeSharedSecret(enc_pk);
        auto shared_prng_key = prng_key_agreement.ComputeSharedSecret(prng_pk);
        if (!shared_enc_key.ok() || !shared_prng_key.ok()) {
          // The server forwarded an invalid public key.
          *error_message = "Invalid public key in request from server.";
          return false;
        }
        other_client_enc_keys->push_back(shared_enc_key.value());
        other_client_prng_keys->push_back(shared_prng_key.value());
      }
    }
  }

  if (*number_of_alive_clients <
      minimum_surviving_neighbors_for_reconstruction) {
    *error_message =
        "There are not enough clients to complete this protocol session. "
        "Aborting.";
    return false;
  }
  if (!client_id_set) {
    *error_message =
        "The ShareKeysRequest sent by the server doesn't contain this client's "
        "public keys.";
    return false;
  }
  *error_message = "";
  return true;
}

bool SecAggClientR1ShareKeysBaseState::EncryptAndSendResponse(
    const std::vector<AesKey>& other_client_enc_keys,
    const std::vector<ShamirShare>& pairwise_prng_key_shares,
    const std::vector<ShamirShare>& self_prng_key_shares,
    SendToServerInterface* sender) {
  ClientToServerWrapperMessage message;
  ShareKeysResponse* response = message.mutable_share_keys_response();
  AesGcmEncryption encryptor;

  for (uint32_t i = 0; i < other_client_enc_keys.size(); ++i) {
    if (async_abort_ && async_abort_->Signalled()) return false;
    if (other_client_enc_keys[i].size() == 0) {
      // Add a blank for dropped-out clients and for this client.
      response->add_encrypted_key_shares("");
    } else {
      PairOfKeyShares key_shares_pair;
      key_shares_pair.set_noise_sk_share(pairwise_prng_key_shares[i].data);
      key_shares_pair.set_prf_sk_share(self_prng_key_shares[i].data);
      std::string serialized_pair = key_shares_pair.SerializeAsString();
      response->add_encrypted_key_shares(
          encryptor.Encrypt(other_client_enc_keys[i], serialized_pair));
    }
  }

  sender->Send(&message);
  return true;
}

}  // namespace secagg
}  // namespace fcp
