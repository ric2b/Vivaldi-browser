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

#include "fcp/secagg/client/secagg_client_r3_unmasking_state.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "fcp/base/monitoring.h"
#include "fcp/secagg/client/other_client_state.h"
#include "fcp/secagg/client/secagg_client_aborted_state.h"
#include "fcp/secagg/client/secagg_client_alive_base_state.h"
#include "fcp/secagg/client/secagg_client_completed_state.h"
#include "fcp/secagg/client/secagg_client_state.h"
#include "fcp/secagg/client/send_to_server_interface.h"
#include "fcp/secagg/client/state_transition_listener_interface.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/secagg/shared/secagg_vector.h"

namespace fcp {
namespace secagg {

SecAggClientR3UnmaskingState::SecAggClientR3UnmaskingState(
    uint32_t client_id, uint32_t number_of_alive_neighbors,
    uint32_t minimum_surviving_neighbors_for_reconstruction,
    uint32_t number_of_neighbors,
    std::unique_ptr<std::vector<OtherClientState> > other_client_states,
    std::unique_ptr<std::vector<ShamirShare> > pairwise_key_shares,
    std::unique_ptr<std::vector<ShamirShare> > self_key_shares,
    std::unique_ptr<SendToServerInterface> sender,
    std::unique_ptr<StateTransitionListenerInterface> transition_listener,
    AsyncAbort* async_abort)
    : SecAggClientAliveBaseState(std::move(sender),
                                 std::move(transition_listener),
                                 ClientState::R3_UNMASKING, async_abort),
      client_id_(client_id),
      number_of_alive_neighbors_(number_of_alive_neighbors),
      minimum_surviving_neighbors_for_reconstruction_(
          minimum_surviving_neighbors_for_reconstruction),
      number_of_neighbors_(number_of_neighbors),
      other_client_states_(std::move(other_client_states)),
      pairwise_key_shares_(std::move(pairwise_key_shares)),
      self_key_shares_(std::move(self_key_shares)) {
  FCP_CHECK(client_id_ >= 0)
      << "Client id must not be negative but was " << client_id_;
}

SecAggClientR3UnmaskingState::~SecAggClientR3UnmaskingState() = default;

StatusOr<std::unique_ptr<SecAggClientState> >
SecAggClientR3UnmaskingState::HandleMessage(
    const ServerToClientWrapperMessage& message) {
  // Handle abort messages or unmasking requests only.
  if (message.has_abort()) {
    if (message.abort().early_success()) {
      return {std::make_unique<SecAggClientCompletedState>(
          std::move(sender_), std::move(transition_listener_))};
    } else {
      return {std::make_unique<SecAggClientAbortedState>(
          "Aborting because of abort message from the server.",
          std::move(sender_), std::move(transition_listener_))};
    }
  } else if (!message.has_unmasking_request()) {
    // Returns an error indicating that the message is of invalid type.
    return SecAggClientState::HandleMessage(message);
  }
  if (async_abort_ && async_abort_->Signalled())
    return AbortAndNotifyServer(async_abort_->Message());

  const UnmaskingRequest& request = message.unmasking_request();
  std::set<uint32_t> dead_at_round_3_client_ids;

  // Parse incoming request and mark dead clients as dead.
  for (uint32_t i : request.dead_3_client_ids()) {
    // TODO(team): Remove this once backwards compatibility not needed.
    uint32_t id = i - 1;
    if (id == client_id_) {
      return AbortAndNotifyServer(
          "The received UnmaskingRequest states this client has aborted, but "
          "this client had not yet aborted.");
    } else if (id >= number_of_neighbors_) {
      return AbortAndNotifyServer(
          "The received UnmaskingRequest contains a client id that does not "
          "correspond to any client.");
    }
    switch ((*other_client_states_)[id]) {
      case OtherClientState::kAlive:
        (*other_client_states_)[id] = OtherClientState::kDeadAtRound3;
        --number_of_alive_neighbors_;
        break;
      case OtherClientState::kDeadAtRound3:
        return AbortAndNotifyServer(
            "The received UnmaskingRequest repeated a client more than once "
            "as a dead client.");
        break;
      case OtherClientState::kDeadAtRound1:
      case OtherClientState::kDeadAtRound2:
      default:
        return AbortAndNotifyServer(
            "The received UnmaskingRequest considers a client dead in round 3 "
            "that was already considered dead.");
        break;
    }
  }

  if (number_of_alive_neighbors_ <
      minimum_surviving_neighbors_for_reconstruction_) {
    return AbortAndNotifyServer(
        "Not enough clients survived. The server should not have sent this "
        "UnmaskingRequest.");
  }

  /*
   * Construct a response for the server by choosing the appropriate shares for
   * each client (i.e. the pairwise share if the client died at round 3, the
   * self share if the client is alive, or no shares at all if the client died
   * at or before round 2.
   */
  ClientToServerWrapperMessage message_to_server;
  UnmaskingResponse* unmasking_response =
      message_to_server.mutable_unmasking_response();
  for (uint32_t i = 0; i < number_of_neighbors_; ++i) {
    if (async_abort_ && async_abort_->Signalled())
      return AbortAndNotifyServer(async_abort_->Message());
    switch ((*other_client_states_)[i]) {
      case OtherClientState::kAlive:
        unmasking_response->add_noise_or_prf_key_shares()->set_prf_sk_share(
            (*self_key_shares_)[i].data);
        break;
      case OtherClientState::kDeadAtRound3:
        unmasking_response->add_noise_or_prf_key_shares()->set_noise_sk_share(
            (*pairwise_key_shares_)[i].data);
        break;
      case OtherClientState::kDeadAtRound1:
      case OtherClientState::kDeadAtRound2:
      default:
        unmasking_response->add_noise_or_prf_key_shares();
        break;
    }
  }

  // Send this final message to the server, then enter Completed state.
  sender_->Send(&message_to_server);
  return {std::make_unique<SecAggClientCompletedState>(
      std::move(sender_), std::move(transition_listener_))};
}

std::string SecAggClientR3UnmaskingState::StateName() const {
  return "R3_UNMASKING";
}

}  // namespace secagg
}  // namespace fcp
