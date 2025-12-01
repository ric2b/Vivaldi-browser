/*
 * Copyright 2020 Google LLC
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

#include "fcp/secagg/server/secagg_server_protocol_impl.h"

#include <string>
#include <utility>

#include "absl/container/node_hash_map.h"
#include "absl/status/status.h"
#include "absl/time/time.h"
#include "fcp/base/monitoring.h"
#include "fcp/secagg/server/tracing_schema.h"
#include "fcp/secagg/shared/compute_session_id.h"
#include "fcp/tracing/tracing_span.h"

namespace {

// Defines an experiments object with no experiments enabled
class EmptyExperiment : public fcp::secagg::ExperimentsInterface {
 public:
  bool IsEnabled(absl::string_view experiment_name) override { return false; }
};

}  // namespace

namespace fcp {
namespace secagg {

SecAggServerProtocolImpl::SecAggServerProtocolImpl(
    std::unique_ptr<SecretSharingGraph> graph,
    int minimum_number_of_clients_to_proceed,
    std::unique_ptr<SecAggServerMetricsListener> metrics,
    std::unique_ptr<AesPrngFactory> prng_factory,
    SendToClientsInterface* sender, std::unique_ptr<SecAggScheduler> scheduler,
    std::vector<ClientStatus> client_statuses,
    std::unique_ptr<ExperimentsInterface> experiments)
    : secret_sharing_graph_(std::move(graph)),
      minimum_number_of_clients_to_proceed_(
          minimum_number_of_clients_to_proceed),
      metrics_(std::move(metrics)),
      prng_factory_(std::move(prng_factory)),
      sender_(sender),
      scheduler_(std::move(scheduler)),
      total_number_of_clients_(client_statuses.size()),
      client_statuses_(std::move(client_statuses)),
      experiments_(experiments ? std::move(experiments)
                               : std::unique_ptr<ExperimentsInterface>(
                                     new EmptyExperiment())),
      pairwise_public_keys_(total_number_of_clients()),
      pairs_of_public_keys_(total_number_of_clients()),
      encrypted_shares_(total_number_of_clients(),
                        std::vector<std::string>(number_of_neighbors())) {}

void SecAggServerProtocolImpl::SetResult(
    std::unique_ptr<SecAggVectorMap> result) {
  FCP_CHECK(!result_) << "Result can't be set twice";
  result_ = std::move(result);
}

std::unique_ptr<SecAggVectorMap> SecAggServerProtocolImpl::TakeResult() {
  return std::move(result_);
}

// -----------------------------------------------------------------------------
// Round 0 methods
// -----------------------------------------------------------------------------

Status SecAggServerProtocolImpl::HandleAdvertiseKeys(
    uint32_t client_id, const AdvertiseKeys& advertise_keys) {
  const auto& pair_of_public_keys = advertise_keys.pair_of_public_keys();
  if ((pair_of_public_keys.enc_pk().size() != EcdhPublicKey::kSize &&
       (pair_of_public_keys.enc_pk().size() <
            EcdhPublicKey::kUncompressedSize ||
        pair_of_public_keys.noise_pk().size() <
            EcdhPublicKey::kUncompressedSize)) ||
      pair_of_public_keys.enc_pk().size() !=
          pair_of_public_keys.noise_pk().size()) {
    return ::absl::InvalidArgumentError(
        "A public key sent by the client was not the correct size.");
  }

  if (pair_of_public_keys.noise_pk().size() == EcdhPublicKey::kSize) {
    pairwise_public_keys_[client_id] =
        EcdhPublicKey(reinterpret_cast<const uint8_t*>(
            pair_of_public_keys.noise_pk().c_str()));
  } else {
    // Strip off the header, if any, and use the uncompressed ECDH key.
    size_t key_size_with_header = pair_of_public_keys.noise_pk().size();
    pairwise_public_keys_[client_id] = EcdhPublicKey(
        reinterpret_cast<const uint8_t*>(
            pair_of_public_keys.noise_pk()
                .substr(key_size_with_header - EcdhPublicKey::kUncompressedSize)
                .c_str()),
        EcdhPublicKey::kUncompressed);
  }

  pairs_of_public_keys_[client_id] = pair_of_public_keys;
  return ::absl::OkStatus();
}

void SecAggServerProtocolImpl::ErasePublicKeysForClient(uint32_t client_id) {
  pairwise_public_keys_[client_id] = EcdhPublicKey();
  pairs_of_public_keys_[client_id] = PairOfPublicKeys();
}

void SecAggServerProtocolImpl::ComputeSessionId() {
  // This message contains all keys, and is only built for the purpose
  // of deriving the session key from it
  ShareKeysRequest share_keys_request;
  for (int i = 0; i < total_number_of_clients(); ++i) {
    *(share_keys_request.add_pairs_of_public_keys()) = pairs_of_public_keys_[i];
  }
  set_session_id(std::make_unique<SessionId>(
      fcp::secagg::ComputeSessionId(share_keys_request)));
}

void SecAggServerProtocolImpl::PrepareShareKeysRequestForClient(
    uint32_t client_id, ShareKeysRequest* request) const {
  request->clear_pairs_of_public_keys();
  for (int j = 0; j < secret_sharing_graph()->GetDegree(); ++j) {
    *request->add_pairs_of_public_keys() =
        pairs_of_public_keys_[secret_sharing_graph()->GetNeighbor(client_id,
                                                                  j)];
  }
}

void SecAggServerProtocolImpl::ClearPairsOfPublicKeys() {
  pairs_of_public_keys_.clear();
}

// -----------------------------------------------------------------------------
// Round 1 methods
// -----------------------------------------------------------------------------

Status SecAggServerProtocolImpl::HandleShareKeysResponse(
    uint32_t client_id, const ShareKeysResponse& share_keys_response) {
  // Verify that the message has the expected fields set before accepting it.
  if (share_keys_response.encrypted_key_shares().size() !=
      number_of_neighbors()) {
    return ::absl::InvalidArgumentError(
        "The ShareKeysResponse does not contain the expected number of "
        "encrypted pairs of key shares.");
  }

  for (uint32_t i = 0; i < number_of_neighbors(); ++i) {
    bool i_is_empty = share_keys_response.encrypted_key_shares(i).empty();
    int neighbor_id = GetNeighbor(client_id, i);
    bool i_should_be_empty = (neighbor_id == client_id) ||
                             (client_status(neighbor_id) ==
                              ClientStatus::DEAD_BEFORE_SENDING_ANYTHING);
    if (i_is_empty && !i_should_be_empty) {
      return ::absl::InvalidArgumentError(
          "Client omitted a key share that was expected.");
    }
    if (i_should_be_empty && !i_is_empty) {
      return ::absl::InvalidArgumentError(
          "Client sent a key share that was not expected.");
    }
  }

  // Client sent a valid message.
  for (int i = 0; i < number_of_neighbors(); ++i) {
    int neighbor_id = GetNeighbor(client_id, i);
    // neighbor_id and client_id are neighbors, and thus index_in_neighbors is
    // in [0, number_neighbors()-1]
    int index_in_neighbor = GetNeighborIndexOrDie(neighbor_id, client_id);
    encrypted_shares_[neighbor_id][index_in_neighbor] =
        share_keys_response.encrypted_key_shares(i);
  }

  return ::absl::OkStatus();
}

void SecAggServerProtocolImpl::EraseShareKeysForClient(uint32_t client_id) {
  for (int i = 0; i < number_of_neighbors(); ++i) {
    int neighbor_id = GetNeighbor(client_id, i);
    int index_in_neighbor = GetNeighborIndexOrDie(neighbor_id, client_id);
    encrypted_shares_[neighbor_id][index_in_neighbor].clear();
  }
}

void SecAggServerProtocolImpl::PrepareMaskedInputCollectionRequestForClient(
    uint32_t client_id, MaskedInputCollectionRequest* request) const {
  request->clear_encrypted_key_shares();
  for (int j = 0; j < number_of_neighbors(); ++j) {
    request->add_encrypted_key_shares(encrypted_shares_[client_id][j]);
  }
}

void SecAggServerProtocolImpl::ClearShareKeys() { encrypted_shares_.clear(); }

// -----------------------------------------------------------------------------
// Round 3 methods
// -----------------------------------------------------------------------------

// This enum and the following function relates the client status to whether
// or not its pairwise mask, its self mask, or neither will appear in the
// summed masked input.
enum class ClientMask { kPairwiseMask, kSelfMask, kNoMask };

// Returns the type of mask the server expects to receive a share for, for a
// give client status.
inline ClientMask ClientMaskType(const ClientStatus& client_status) {
  switch (client_status) {
    case ClientStatus::SHARE_KEYS_RECEIVED:
    case ClientStatus::DEAD_AFTER_SHARE_KEYS_RECEIVED:
      return ClientMask::kPairwiseMask;
      break;
    case ClientStatus::MASKED_INPUT_RESPONSE_RECEIVED:
    case ClientStatus::DEAD_AFTER_MASKED_INPUT_RESPONSE_RECEIVED:
    case ClientStatus::UNMASKING_RESPONSE_RECEIVED:
    case ClientStatus::DEAD_AFTER_UNMASKING_RESPONSE_RECEIVED:
      return ClientMask::kSelfMask;
      break;
    case ClientStatus::READY_TO_START:
    case ClientStatus::DEAD_BEFORE_SENDING_ANYTHING:
    case ClientStatus::ADVERTISE_KEYS_RECEIVED:
    case ClientStatus::DEAD_AFTER_ADVERTISE_KEYS_RECEIVED:
    default:
      return ClientMask::kNoMask;
  }
}

void SecAggServerProtocolImpl::SetUpShamirSharesTables() {
  pairwise_shamir_share_table_ = std::make_unique<
      absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>();
  self_shamir_share_table_ = std::make_unique<
      absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>();

  // Prepare the share tables with rows for clients we expect to have shares for
  for (uint32_t i = 0; i < total_number_of_clients(); ++i) {
    auto mask_type = ClientMaskType(client_status(i));
    if (mask_type == ClientMask::kPairwiseMask) {
      pairwise_shamir_share_table_->emplace(i, number_of_neighbors());
    } else if (mask_type == ClientMask::kSelfMask) {
      self_shamir_share_table_->emplace(i, number_of_neighbors());
    }
  }
}

Status SecAggServerProtocolImpl::HandleUnmaskingResponse(
    uint32_t client_id, const UnmaskingResponse& unmasking_response) {
  FCP_CHECK(pairwise_shamir_share_table_ != nullptr &&
            self_shamir_share_table_ != nullptr)
      << "Shamir Shares Tables haven't been initialized";

  // Verify the client sent all the right types of shares.
  for (uint32_t i = 0; i < number_of_neighbors(); ++i) {
    int ith_neighbor = GetNeighbor(client_id, i);
    switch (ClientMaskType(client_status(ith_neighbor))) {
      case ClientMask::kPairwiseMask:
        if (unmasking_response.noise_or_prf_key_shares(i).oneof_shares_case() !=
            NoiseOrPrfKeyShare::OneofSharesCase::kNoiseSkShare) {
          return ::absl::InvalidArgumentError(
              "Client did not include the correct type of key share.");
        }
        break;
      case ClientMask::kSelfMask:
        if (unmasking_response.noise_or_prf_key_shares(i).oneof_shares_case() !=
            NoiseOrPrfKeyShare::OneofSharesCase::kPrfSkShare) {
          return ::absl::InvalidArgumentError(
              "Client did not include the correct type of key share.");
        }
        break;
      case ClientMask::kNoMask:
      default:
        if (unmasking_response.noise_or_prf_key_shares(i).oneof_shares_case() !=
            NoiseOrPrfKeyShare::OneofSharesCase::ONEOF_SHARES_NOT_SET) {
          return ::absl::InvalidArgumentError(
              "Client included a key share for which none was expected.");
        }
    }
  }
  // Prepare the received key shares for reconstruction by inserting them into
  // the tables.
  for (int i = 0; i < number_of_neighbors(); ++i) {
    // Find the index of client_id in the list of neighbors of the ith
    // neighbor of client_id
    int ith_neighbor = GetNeighbor(client_id, i);
    int index = GetNeighborIndexOrDie(ith_neighbor, client_id);
    if (unmasking_response.noise_or_prf_key_shares(i).oneof_shares_case() ==
        NoiseOrPrfKeyShare::OneofSharesCase::kNoiseSkShare) {
      (*pairwise_shamir_share_table_)[ith_neighbor][index].data =
          unmasking_response.noise_or_prf_key_shares(i).noise_sk_share();
    } else if (unmasking_response.noise_or_prf_key_shares(i)
                   .oneof_shares_case() ==
               NoiseOrPrfKeyShare::OneofSharesCase::kPrfSkShare) {
      (*self_shamir_share_table_)[ith_neighbor][index].data =
          unmasking_response.noise_or_prf_key_shares(i).prf_sk_share();
    }
  }
  return ::absl::OkStatus();
}

// -----------------------------------------------------------------------------
// PRNG computation methods
// -----------------------------------------------------------------------------

StatusOr<SecAggServerProtocolImpl::ShamirReconstructionResult>
SecAggServerProtocolImpl::HandleShamirReconstruction() {
  FCP_CHECK(pairwise_shamir_share_table_ != nullptr &&
            self_shamir_share_table_ != nullptr)
      << "Shamir Shares Tables haven't been initialized";

  ShamirReconstructionResult result;
  ShamirSecretSharing reconstructor;

  for (const auto& item : *pairwise_shamir_share_table_) {
    FCP_ASSIGN_OR_RETURN(std::string reconstructed_key,
                         reconstructor.Reconstruct(
                             minimum_surviving_neighbors_for_reconstruction(),
                             item.second, EcdhPrivateKey::kSize));
    auto key_agreement = EcdhKeyAgreement::CreateFromPrivateKey(EcdhPrivateKey(
        reinterpret_cast<const uint8_t*>(reconstructed_key.c_str())));
    if (!key_agreement.ok()) {
      // The server was unable to reconstruct the private key, probably
      // because some client(s) sent invalid key shares. The only way out is
      // to abort.
      return ::absl::InvalidArgumentError(
          "Unable to reconstruct aborted client's private key from shares");
    }
    result.aborted_client_key_agreements.try_emplace(
        item.first, std::move(*(key_agreement.value())));
  }

  for (const auto& item : *self_shamir_share_table_) {
    FCP_ASSIGN_OR_RETURN(
        AesKey reconstructed,
        AesKey::CreateFromShares(
            item.second, minimum_surviving_neighbors_for_reconstruction()));
    result.self_keys.try_emplace(item.first, reconstructed);
  }

  return std::move(result);
}

StatusOr<SecAggServerProtocolImpl::PrngWorkItems>
SecAggServerProtocolImpl::InitializePrng(
    const ShamirReconstructionResult& shamir_reconstruction_result) const {
  PrngWorkItems work_items;

  for (uint32_t i = 0; i < total_number_of_clients(); ++i) {
    // Although clients who are DEAD_AFTER_MASKED_INPUT_RESPONSE_RECEIVED and
    // kDeadAfterUnmaskingResponseReceived have they did so after sending
    // their masked input. Therefore, it is possible to include their
    // contribution to the aggregate sum. So we treat them here as if they had
    // completed the protocol correctly.
    auto status = client_status(i);
    if (status != ClientStatus::UNMASKING_RESPONSE_RECEIVED &&
        status != ClientStatus::DEAD_AFTER_UNMASKING_RESPONSE_RECEIVED &&
        status != ClientStatus::DEAD_AFTER_MASKED_INPUT_RESPONSE_RECEIVED) {
      continue;
    }

    // Since client i's value will be included in the sum, the server must
    // remove its self mask.
    auto it = shamir_reconstruction_result.self_keys.find(i);
    FCP_CHECK(it != shamir_reconstruction_result.self_keys.end());
    work_items.prng_keys_to_subtract.push_back(it->second);

    // For clients that aborted, client i's sum contains an un-canceled
    // pairwise mask generated between the two clients. The server must remove
    // this pairwise mask from the sum.
    for (const auto& item :
         shamir_reconstruction_result.aborted_client_key_agreements) {
      if (!AreNeighbors(i, item.first)) {
        continue;
      }
      auto shared_key =
          item.second.ComputeSharedSecret(pairwise_public_keys(i));
      if (!shared_key.ok()) {
        // Should not happen; invalid public keys should already be detected.
        // But if it does happen, abort.
        return ::absl::InvalidArgumentError(
            "Invalid public key from client detected");
      }
      if (IsOutgoingNeighbor(i, item.first)) {
        work_items.prng_keys_to_add.push_back(shared_key.value());
      } else {
        work_items.prng_keys_to_subtract.push_back(shared_key.value());
      }
    }
  }

  return std::move(work_items);
}

}  // namespace secagg
}  // namespace fcp
