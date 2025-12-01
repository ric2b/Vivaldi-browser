/*
 * Copyright 2019 Google LLC
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

#include "fcp/secagg/server/secagg_server_r1_share_keys_state.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "fcp/base/monitoring.h"
#include "fcp/secagg/server/secagg_server_r2_masked_input_coll_state.h"

namespace fcp {
namespace secagg {

SecAggServerR1ShareKeysState::SecAggServerR1ShareKeysState(
    std::unique_ptr<SecAggServerProtocolImpl> impl,
    int number_of_clients_failed_after_sending_masked_input,
    int number_of_clients_failed_before_sending_masked_input,
    int number_of_clients_terminated_without_unmasking)
    : SecAggServerState(number_of_clients_failed_after_sending_masked_input,
                        number_of_clients_failed_before_sending_masked_input,
                        number_of_clients_terminated_without_unmasking,
                        SecAggServerStateKind::R1_SHARE_KEYS, std::move(impl)) {
}

SecAggServerR1ShareKeysState::~SecAggServerR1ShareKeysState() = default;

Status SecAggServerR1ShareKeysState::HandleMessage(
    uint32_t client_id, const ClientToServerWrapperMessage& message) {
  if (message.has_abort()) {
    MessageReceived(message, false);
    AbortClient(client_id, "", ClientDropReason::SENT_ABORT_MESSAGE,
                /*notify=*/false);
    return FCP_STATUS(OK);
  }
  // If the client has aborted or sent a message already, ignore its messages.
  if (client_status(client_id) != ClientStatus::ADVERTISE_KEYS_RECEIVED) {
    MessageReceived(message, false);
    AbortClient(client_id,
                "Not expecting an ShareKeysResponse from this "
                "client - either the client already aborted or one such "
                "message was already received.",
                ClientDropReason::SHARE_KEYS_UNEXPECTED);
    return FCP_STATUS(OK);
  }
  if (!message.has_share_keys_response()) {
    MessageReceived(message, false);
    AbortClient(client_id,
                "Message type received is different from what was expected.",
                ClientDropReason::UNEXPECTED_MESSAGE_TYPE);
    return FCP_STATUS(OK);
  }
  MessageReceived(message, true);

  Status status =
      impl()->HandleShareKeysResponse(client_id, message.share_keys_response());
  if (!status.ok()) {
    AbortClient(client_id, std::string(status.message()),
                ClientDropReason::INVALID_SHARE_KEYS_RESPONSE);
    return FCP_STATUS(OK);
  }

  set_client_status(client_id, ClientStatus::SHARE_KEYS_RECEIVED);
  number_of_messages_received_in_this_round_++;
  number_of_clients_ready_for_next_round_++;
  return FCP_STATUS(OK);
}

bool SecAggServerR1ShareKeysState::IsNumberOfIncludedInputsCommitted() const {
  return false;
}

int SecAggServerR1ShareKeysState::MinimumMessagesNeededForNextRound() const {
  return std::max(0, minimum_number_of_clients_to_proceed() -
                         number_of_clients_ready_for_next_round_);
}

int SecAggServerR1ShareKeysState::NumberOfPendingClients() const {
  return NumberOfAliveClients() - number_of_clients_ready_for_next_round_;
}

void SecAggServerR1ShareKeysState::HandleAbortClient(
    uint32_t client_id, ClientDropReason reason_code) {
  number_of_clients_failed_before_sending_masked_input_++;
  if (client_status(client_id) == ClientStatus::SHARE_KEYS_RECEIVED) {
    number_of_clients_ready_for_next_round_--;
    // Remove that client's shared keys as if they were never sent. This will
    // avoid wasted computation on both client and server ends.
    impl()->EraseShareKeysForClient(client_id);
  }
  set_client_status(client_id,
                    ClientStatus::DEAD_AFTER_ADVERTISE_KEYS_RECEIVED);
  if (NumberOfAliveClients() < minimum_number_of_clients_to_proceed()) {
    needs_to_abort_ = true;
  }
}

StatusOr<std::unique_ptr<SecAggServerState>>
SecAggServerR1ShareKeysState::ProceedToNextRound() {
  if (!ReadyForNextRound()) {
    return FCP_STATUS(UNAVAILABLE);
  }
  if (needs_to_abort_) {
    std::string error_string = "Too many clients aborted.";
    ServerToClientWrapperMessage message;
    message.mutable_abort()->set_diagnostic_info(error_string);
    message.mutable_abort()->set_early_success(false);
    BroadcastMessage(message);

    return AbortState(error_string,
                      SecAggServerOutcome::NOT_ENOUGH_CLIENTS_REMAINING);
  }

  // Abort all clients that haven't yet sent a message, and send a message to
  // all clients that are still alive.
  for (int i = 0; i < total_number_of_clients(); ++i) {
    if (!IsClientDead(i) &&
        client_status(i) != ClientStatus::SHARE_KEYS_RECEIVED) {
      AbortClient(
          i, "Client did not send ShareKeysResponse before round transition.",
          ClientDropReason::NO_SHARE_KEYS);
    } else if (client_status(i) == ClientStatus::SHARE_KEYS_RECEIVED) {
      ServerToClientWrapperMessage message;
      impl()->PrepareMaskedInputCollectionRequestForClient(
          i, message.mutable_masked_input_request());
      Send(i, message);
    }
  }

  // Encrypted shares are no longer needed beyond this point as the server has
  // already forwarded them to the clients.
  impl()->ClearShareKeys();

  return std::make_unique<SecAggServerR2MaskedInputCollState>(
      ExitState(StateTransition::kSuccess),
      number_of_clients_failed_after_sending_masked_input_,
      number_of_clients_failed_before_sending_masked_input_,
      number_of_clients_terminated_without_unmasking_);
}

bool SecAggServerR1ShareKeysState::ReadyForNextRound() const {
  return (number_of_clients_ready_for_next_round_ >=
          minimum_number_of_clients_to_proceed()) ||
         (needs_to_abort_);
}

}  // namespace secagg
}  // namespace fcp
