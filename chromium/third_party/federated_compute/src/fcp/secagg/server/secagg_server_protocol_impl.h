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

#ifndef FCP_SECAGG_SERVER_SECAGG_SERVER_PROTOCOL_IMPL_H_
#define FCP_SECAGG_SERVER_SECAGG_SERVER_PROTOCOL_IMPL_H_

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "fcp/secagg/server/experiments_interface.h"
#include "fcp/secagg/server/secagg_scheduler.h"
#include "fcp/secagg/server/secagg_server_enums.pb.h"
#include "fcp/secagg/server/secagg_server_metrics_listener.h"
#include "fcp/secagg/server/secret_sharing_graph.h"
#include "fcp/secagg/server/send_to_clients_interface.h"
#include "fcp/secagg/shared/aes_prng_factory.h"
#include "fcp/secagg/shared/compute_session_id.h"
#include "fcp/secagg/shared/ecdh_key_agreement.h"
#include "fcp/secagg/shared/ecdh_keys.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/secagg/shared/secagg_vector.h"
#include "fcp/secagg/shared/shamir_secret_sharing.h"

namespace fcp {
namespace secagg {

// Interface that describes internal implementation of SecAgg protocol.
//
// The general design is the following
//
// +--------------+    +-------------------+    +--------------------------+
// | SecAggServer |--->| SecAggServerState |--->| SecAggServerProtocolImpl |
// +--------------+    +-------------------+    +--------------------------+
//                                ^                          ^
//                               /-\                        /-\
//                                |                          |
//                     +-------------------+    +------------------------+
//                     | Specific State    |    | Specific protocol impl |
//                     +-------------------+    +------------------------+
//
// Specific states implement logic specific to each logic SecAgg state, such as
// R0AdvertiseKeys or PrngRunning, while specific protocol implementation is
// shared between all states and is responsible for encapsulating the data
// for the protocol and providing methods for manipulating the data.
//

class SecAggServerProtocolImpl {
 public:
  explicit SecAggServerProtocolImpl(
      std::unique_ptr<SecretSharingGraph> graph,
      int minimum_number_of_clients_to_proceed,
      std::unique_ptr<SecAggServerMetricsListener> metrics,
      std::unique_ptr<AesPrngFactory> prng_factory,
      SendToClientsInterface* sender,
      std::unique_ptr<SecAggScheduler> scheduler,
      std::vector<ClientStatus> client_statuses,
      std::unique_ptr<ExperimentsInterface> experiments = nullptr);
  virtual ~SecAggServerProtocolImpl() = default;

  SecAggServerProtocolImpl(const SecAggServerProtocolImpl& other) = delete;
  SecAggServerProtocolImpl& operator=(const SecAggServerProtocolImpl& other) =
      delete;

  // Returns server variant for this protocol implementation.
  virtual ServerVariant server_variant() const = 0;

  // Returns the graph that represents the cohort of clients.
  inline const SecretSharingGraph* secret_sharing_graph() const {
    return secret_sharing_graph_.get();
  }

  // Returns the minimum threshold number of clients that need to send valid
  // responses in order for the protocol to proceed from one round to the next.
  inline int minimum_number_of_clients_to_proceed() const {
    return minimum_number_of_clients_to_proceed_;
  }

  // Returns the callback interface for recording metrics.
  inline SecAggServerMetricsListener* metrics() const { return metrics_.get(); }

  // Returns a reference to an instance of a subclass of AesPrngFactory.
  inline AesPrngFactory* prng_factory() const { return prng_factory_.get(); }

  // Returns the callback interface for sending protocol buffer messages to the
  // client.
  inline SendToClientsInterface* sender() const { return sender_; }

  // Returns the scheduler for scheduling parallel computation tasks and
  // callbacks.
  inline SecAggScheduler* scheduler() const { return scheduler_.get(); }

  // Returns the experiments
  inline ExperimentsInterface* experiments() const {
    return experiments_.get();
  }

  // Getting or setting the protocol result.
  //
  // TODO(team): SetResult should not be needed (except for testing) once
  // PRNG computation is moved into the protocol implementation.
  void SetResult(std::unique_ptr<SecAggVectorMap> result);
  std::unique_ptr<SecAggVectorMap> TakeResult();

  // Gets the client status.
  inline const ClientStatus& client_status(uint32_t client_id) const {
    return client_statuses_.at(client_id);
  }

  // Sets the client status.
  inline void set_client_status(uint32_t client_id, ClientStatus status) {
    client_statuses_[client_id] = status;
  }

  // Gets the number of clients that the protocol starts with.
  inline size_t total_number_of_clients() const {
    return total_number_of_clients_;
  }

  // Returns the number of neighbors of each client.
  inline int number_of_neighbors() const {
    return secret_sharing_graph()->GetDegree();
  }

  // Returns the minimum number of neighbors of a client that must not drop-out
  // for that client's contribution to be included in the sum. This corresponds
  // to the threshold in the shamir secret sharing of self and pairwise masks.
  inline int minimum_surviving_neighbors_for_reconstruction() const {
    return secret_sharing_graph()->GetThreshold();
  }

  // Returns client_id's ith neighbor.
  // This function assumes that 0 <= i < number_of_neighbors() and will throw a
  // runtime error if that's not the case
  inline int GetNeighbor(int client_id, int i) const {
    return secret_sharing_graph()->GetNeighbor(client_id, i);
  }

  // Returns the index of client_id_2 in the list of neighbors of client_id_1,
  // if present
  inline std::optional<int> GetNeighborIndex(int client_id_1,
                                             int client_id_2) const {
    return secret_sharing_graph()->GetNeighborIndex(client_id_1, client_id_2);
  }

  // Returns the index of client_id_2 in the list of neighbors of client_id_1
  // This function assumes that client_id_1 and client_id_2 are neighbors, and
  // will throw a runtime error if that's not the case
  inline int GetNeighborIndexOrDie(int client_id_1, int client_id_2) const {
    auto index =
        secret_sharing_graph()->GetNeighborIndex(client_id_1, client_id_2);
    FCP_CHECK(index.has_value());
    return index.value();
  }

  // Returns true if clients client_id_1 and client_id_1 are neighbors, else
  // false.
  inline bool AreNeighbors(int client_id_1, int client_id_2) const {
    return secret_sharing_graph()->AreNeighbors(client_id_1, client_id_2);
  }

  // Returns true if client_id_1 is an outgoing neighbor of client_id_2, else
  // false.
  inline bool IsOutgoingNeighbor(int client_id_1, int client_id_2) const {
    return secret_sharing_graph()->IsOutgoingNeighbor(client_id_1, client_id_2);
  }

  inline void SetPairwisePublicKeys(uint32_t client_id,
                                    const EcdhPublicKey& pairwise_key) {
    pairwise_public_keys_[client_id] = pairwise_key;
  }

  inline const EcdhPublicKey& pairwise_public_keys(uint32_t client_id) const {
    return pairwise_public_keys_[client_id];
  }

  inline const SessionId& session_id() const {
    FCP_CHECK(session_id_ != nullptr);
    return *session_id_;
  }

  void set_session_id(std::unique_ptr<SessionId> session_id) {
    FCP_CHECK(session_id != nullptr);
    session_id_ = std::move(session_id);
  }

  // TODO(team): Review whether getters and setters below are needed.
  // Most of these fields are needed only for testing.

  void set_pairwise_shamir_share_table(
      std::unique_ptr<absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>
          pairwise_shamir_share_table) {
    pairwise_shamir_share_table_ = std::move(pairwise_shamir_share_table);
  }

  void set_self_shamir_share_table(
      std::unique_ptr<absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>
          self_shamir_share_table) {
    self_shamir_share_table_ = std::move(self_shamir_share_table);
  }

  // ---------------------------------------------------------------------------
  // Round 0 methods
  // ---------------------------------------------------------------------------

  // Sets the public key pairs for a client.
  Status HandleAdvertiseKeys(uint32_t client_id,
                             const AdvertiseKeys& advertise_keys);

  // Erases public key pairs for a client.
  void ErasePublicKeysForClient(uint32_t client_id);

  // Compute session ID based on public key pairs advertised by clients.
  void ComputeSessionId();

  // This method allows a protocol implementation to populate fields that are
  // common to the ShareKeysRequest sent to all clients.
  virtual Status InitializeShareKeysRequest(
      ShareKeysRequest* request) const = 0;

  // Prepares ShareKeysRequest message to send to the client.
  // This method will update fields in the request as needed, but will not clear
  // any fields that are not specific to the share keys request for the specific
  // client.  The caller can therefore set up a single ShareKeysRequest object,
  // populate fields that will be common to all clients, and repeatedly call
  // this method to set the client-specific fields before serializing the
  // message and sending it.
  void PrepareShareKeysRequestForClient(uint32_t client_id,
                                        ShareKeysRequest* request) const;

  // Clears all pairs of public keys.
  void ClearPairsOfPublicKeys();

  // ---------------------------------------------------------------------------
  // Round 1 methods
  // ---------------------------------------------------------------------------

  // Sets the encrypted shares received from a client.
  Status HandleShareKeysResponse(uint32_t client_id,
                                 const ShareKeysResponse& share_keys_response);

  // Erases the encrypted shares for a client.
  void EraseShareKeysForClient(uint32_t client_id);

  // Prepares MaskedInputCollectionRequest message to send to the client.
  // This method will update fields in the request as needed, but will not clear
  // any fields that are not specific to the share keys request for the specific
  // client.  The caller can therefore set up a single ShareKeysRequest object,
  // populate fields that will be common to all clients, and repeatedly call
  // this method to set the client-specific fields before serializing the
  // message and sending it.
  void PrepareMaskedInputCollectionRequestForClient(
      uint32_t client_id, MaskedInputCollectionRequest* request) const;

  // Clears all encrypted shares.
  void ClearShareKeys();

  // ---------------------------------------------------------------------------
  // Round 2 methods
  // ---------------------------------------------------------------------------

  // Sets up the sum of encrypted vectors received by the clients in R1.  This
  // must be called before any other R2 methods are called.
  virtual AsyncToken SetupMaskedInputCollection() = 0;

  // Finalizes the async aggregation of R2 messages before moving to R3.
  virtual void FinalizeMaskedInputCollection() = 0;

  // Check that an encrypted vector received by the user is valid, and add it to
  // the sum of encrypted vectors.
  virtual Status HandleMaskedInputCollectionResponse(
      std::unique_ptr<MaskedInputCollectionResponse> masked_input_response) = 0;

  // ---------------------------------------------------------------------------
  // Round 3 methods
  // ---------------------------------------------------------------------------

  // This must be called in the beginning of round 3 to setup Shamir shares
  // tables based on client states at the beginning of the round.
  void SetUpShamirSharesTables();

  // Populates Shamir shares tables with the data from UnmaskingResponse.
  // Returning an error status means that the unmasking response was invalid.
  Status HandleUnmaskingResponse(uint32_t client_id,
                                 const UnmaskingResponse& unmasking_response);

  // ---------------------------------------------------------------------------
  // PRNG computation methods
  // ---------------------------------------------------------------------------

  // Result of performing Shamir secret sharing keys reconstruction.
  struct ShamirReconstructionResult {
    absl::flat_hash_map<uint32_t, EcdhKeyAgreement>
        aborted_client_key_agreements;
    absl::node_hash_map<uint32_t, AesKey> self_keys;
  };

  // Performs reconstruction secret sharing keys reconstruction step of
  // the PRNG stage of the protocol.
  StatusOr<ShamirReconstructionResult> HandleShamirReconstruction();

  struct PrngWorkItems {
    std::vector<AesKey> prng_keys_to_add;
    std::vector<AesKey> prng_keys_to_subtract;
  };

  // Initializes PRNG work items.
  StatusOr<PrngWorkItems> InitializePrng(
      const ShamirReconstructionResult& shamir_reconstruction_result) const;

  // Tells the PRNG stage of the protocol to start running asynchronously by
  // executing PRNG work items.
  // The returned cancellation token can be used to abort the asynchronous
  // execution.
  virtual AsyncToken StartPrng(const PrngWorkItems& work_items,
                               std::function<void(Status)> done_callback) = 0;

 private:
  std::unique_ptr<SecretSharingGraph> secret_sharing_graph_;
  int minimum_number_of_clients_to_proceed_;

  std::vector<InputVectorSpecification> input_vector_specs_;
  std::unique_ptr<SecAggServerMetricsListener> metrics_;
  std::unique_ptr<AesPrngFactory> prng_factory_;
  SendToClientsInterface* sender_;
  std::unique_ptr<SecAggScheduler> scheduler_;

  std::unique_ptr<SecAggVectorMap> result_;

  size_t total_number_of_clients_;
  std::vector<ClientStatus> client_statuses_;
  std::unique_ptr<ExperimentsInterface> experiments_;

  // This vector collects the public keys sent by the clients that will be used
  // for running the PRNG later on.
  std::vector<EcdhPublicKey> pairwise_public_keys_;

  // This vector collects all pairs of public keys sent by the clients, so they
  // can be forwarded at the end of Advertise Keys round.
  std::vector<PairOfPublicKeys> pairs_of_public_keys_;

  std::unique_ptr<SessionId> session_id_;

  // Track the encrypted shares received from clients in preparation for sending
  // them. encrypted_shares_table_[i][j] is an encryption of the pair of shares
  // to be sent to client i, received from client j.
  std::vector<std::vector<std::string>> encrypted_shares_;

  // Shamir shares tables.
  // These store shares that have been collected from clients, and will be built
  // up over the course of round 3. For both tables, the map key represents
  // the client whose key these are shares of; the index in the vector
  // represents the client who provided that key share.
  std::unique_ptr<absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>
      pairwise_shamir_share_table_;
  std::unique_ptr<absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>
      self_shamir_share_table_;
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SERVER_SECAGG_SERVER_PROTOCOL_IMPL_H_
