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

#include "fcp/secagg/client/secagg_client_r2_masked_input_coll_base_state.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/container/node_hash_map.h"
#include "absl/strings/str_cat.h"
#include "fcp/base/monitoring.h"
#include "fcp/secagg/client/other_client_state.h"
#include "fcp/secagg/client/secagg_client_aborted_state.h"
#include "fcp/secagg/client/secagg_client_alive_base_state.h"
#include "fcp/secagg/client/secagg_client_completed_state.h"
#include "fcp/secagg/client/secagg_client_r3_unmasking_state.h"
#include "fcp/secagg/client/secagg_client_state.h"
#include "fcp/secagg/client/send_to_server_interface.h"
#include "fcp/secagg/shared/aes_gcm_encryption.h"
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

SecAggClientR2MaskedInputCollBaseState::SecAggClientR2MaskedInputCollBaseState(
    std::unique_ptr<SendToServerInterface> sender,
    std::unique_ptr<StateTransitionListenerInterface> transition_listener,
    AsyncAbort* async_abort)
    : SecAggClientAliveBaseState(std::move(sender),
                                 std::move(transition_listener),
                                 ClientState::R2_MASKED_INPUT, async_abort) {}

SecAggClientR2MaskedInputCollBaseState::
    ~SecAggClientR2MaskedInputCollBaseState() = default;

std::unique_ptr<SecAggVectorMap>
SecAggClientR2MaskedInputCollBaseState::HandleMaskedInputCollectionRequest(
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
    std::vector<ShamirShare>* self_key_shares, std::string* error_message) {
  if (request.encrypted_key_shares_size() !=
      static_cast<int>(number_of_clients)) {
    *error_message =
        "The number of encrypted shares sent by the server does not match "
        "the number of clients.";
    return nullptr;
  }

  // Parse the request, decrypt and store the key shares from other clients.
  AesGcmEncryption decryptor;
  std::string plaintext;

  for (int i = 0; i < static_cast<int>(number_of_clients); ++i) {
    if (async_abort_ && async_abort_->Signalled()) {
      *error_message = async_abort_->Message();
      return nullptr;
    }
    if (i == static_cast<int>(client_id)) {
      // this client
      pairwise_key_shares->push_back({""});  // this will never be needed
      self_key_shares->push_back(own_self_key_share);
    } else if ((*other_client_states)[i] != OtherClientState::kAlive) {
      if (!request.encrypted_key_shares(i).empty()) {
        // A client who was considered aborted sent key shares.
        *error_message =
            "Received encrypted key shares from an aborted client.";
        return nullptr;
      } else {
        pairwise_key_shares->push_back({""});
        self_key_shares->push_back({""});
      }
    } else if (request.encrypted_key_shares(i).empty()) {
      // A client who was considered alive dropped out. Mark it as dead.
      (*other_client_states)[i] = OtherClientState::kDeadAtRound2;
      pairwise_key_shares->push_back({""});
      self_key_shares->push_back({""});
      --(*number_of_alive_clients);
    } else {
      // A living client sent encrypted key shares, so we decrypt and store
      // them.
      auto decrypted = decryptor.Decrypt(other_client_enc_keys[i],
                                         request.encrypted_key_shares(i));
      if (!decrypted.ok()) {
        *error_message = "Authentication of encrypted data failed.";
        return nullptr;
      } else {
        plaintext = decrypted.value();
      }

      PairOfKeyShares pairwise_and_self_key_shares;
      if (!pairwise_and_self_key_shares.ParseFromString(plaintext)) {
        *error_message = "Unable to parse decrypted pair of key shares.";
        return nullptr;
      }
      pairwise_key_shares->push_back(
          {pairwise_and_self_key_shares.noise_sk_share()});
      self_key_shares->push_back({pairwise_and_self_key_shares.prf_sk_share()});
    }
  }

  if (*number_of_alive_clients <
      minimum_surviving_neighbors_for_reconstruction) {
    *error_message =
        "There are not enough clients to complete this protocol session. "
        "Aborting.";
    return nullptr;
  }

  // Compute the map of masks using the other clients' keys.
  std::vector<AesKey> prng_keys_to_add;
  std::vector<AesKey> prng_keys_to_subtract;

  prng_keys_to_add.push_back(self_prng_key);

  for (int i = 0; i < static_cast<int>(number_of_clients); ++i) {
    if (async_abort_ && async_abort_->Signalled()) {
      *error_message = async_abort_->Message();
      return nullptr;
    }
    if (i == static_cast<int>(client_id) ||
        (*other_client_states)[i] != OtherClientState::kAlive) {
      continue;
    } else if (i < static_cast<int>(client_id)) {
      prng_keys_to_add.push_back(other_client_prng_keys[i]);
    } else {
      prng_keys_to_subtract.push_back(other_client_prng_keys[i]);
    }
  }

  std::unique_ptr<SecAggVectorMap> map =
      MapOfMasks(prng_keys_to_add, prng_keys_to_subtract, input_vector_specs,
                 session_id, prng_factory, async_abort_);
  if (!map) {
    *error_message = async_abort_->Message();
    return nullptr;
  }
  return map;
}

// TODO(team): Add two SecAggVector values more efficiently, without
// having to unpack both vectors and convert the result back into the
// packed form.
SecAggVector AddSecAggVectors(SecAggVector v1, SecAggVector v2) {
  FCP_CHECK(v1.modulus() == v2.modulus());
  uint64_t modulus = v1.modulus();

  // The code below moves v1 and v2 to temp instances to "consume" and destroy
  // the original vectors as soon as possible in order to minimize the number of
  // concurrent copies of the data in memory.
  std::vector<uint64_t> vec1 = SecAggVector(std::move(v1)).GetAsUint64Vector();

  {
    // Keep vec2 scoped so that it is destroyed as soon as it is no longer used
    // and before creating the SecAggVector instance below.
    std::vector<uint64_t> vec2 =
        SecAggVector(std::move(v2)).GetAsUint64Vector();

    // Add the two vectors in place assigning the values back into vec1.
    FCP_CHECK(vec1.size() == vec2.size());
    for (int i = 0; i < static_cast<int>(vec1.size()); ++i) {
      vec1[i] = ((vec1[i] + vec2[i]) % modulus);
    }
  }

  return SecAggVector(vec1, modulus);
}

void SecAggClientR2MaskedInputCollBaseState::SendMaskedInput(
    std::unique_ptr<SecAggVectorMap> input_map,
    std::unique_ptr<SecAggVectorMap> map_of_masks) {
  ClientToServerWrapperMessage to_send;
  for (auto& pair : *input_map) {
    // SetInput should already have guaranteed these
    FCP_CHECK(map_of_masks->find(pair.first) != map_of_masks->end());
    SecAggVector& mask = map_of_masks->at(pair.first);
    SecAggVector sum =
        AddSecAggVectors(std::move(pair.second), std::move(mask));
    MaskedInputVector sum_vec_proto;
    sum_vec_proto.set_encoded_vector(std::move(sum).TakePackedBytes());
    (*to_send.mutable_masked_input_response()->mutable_vectors())[pair.first] =
        std::move(sum_vec_proto);
  }
  sender_->Send(&to_send);
}

}  // namespace secagg
}  // namespace fcp
