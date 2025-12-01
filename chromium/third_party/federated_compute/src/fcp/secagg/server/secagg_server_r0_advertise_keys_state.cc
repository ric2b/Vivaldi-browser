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

#include "fcp/secagg/server/secagg_server_r0_advertise_keys_state.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "fcp/base/monitoring.h"
#include "fcp/secagg/server/secagg_server_r1_share_keys_state.h"

namespace fcp {
namespace secagg {

SecAggServerR0AdvertiseKeysState::SecAggServerR0AdvertiseKeysState(
    std::unique_ptr<SecAggServerProtocolImpl> impl)
    : SecAggServerState(0, 0, 0, SecAggServerStateKind::R0_ADVERTISE_KEYS,
                        std::move(impl)) {
  if (metrics()) {
    // This is the initial state, so we count the start of the protocol from the
    // moment it is constructed.
    metrics()->ProtocolStarts(this->impl()->server_variant());
  }
}

SecAggServerR0AdvertiseKeysState::~SecAggServerR0AdvertiseKeysState() = default;

Status SecAggServerR0AdvertiseKeysState::HandleMessage(
    uint32_t client_id, const ClientToServerWrapperMessage& message) {
  if (message.has_abort()) {
    MessageReceived(message, false);
    AbortClient(client_id, "", ClientDropReason::SENT_ABORT_MESSAGE,
                /*notify=*/false);
    return FCP_STATUS(OK);
  }
  // If the client has aborted or sent a message already, ignore its messages.
  if (client_status(client_id) != ClientStatus::READY_TO_START) {
    MessageReceived(message, false);
    AbortClient(
        client_id,
        "Not expecting an AdvertiseKeys message from this client - either the "
        "client already aborted or one such message was already received.",
        ClientDropReason::ADVERTISE_KEYS_UNEXPECTED);
    return FCP_STATUS(OK);
  }
  if (!message.has_advertise_keys()) {
    MessageReceived(message, false);
    AbortClient(client_id,
                "Message type received is different from what was expected.",
                ClientDropReason::UNEXPECTED_MESSAGE_TYPE);
    return FCP_STATUS(OK);
  }
  MessageReceived(message, true);

  Status status =
      impl()->HandleAdvertiseKeys(client_id, message.advertise_keys());
  if (!status.ok()) {
    AbortClient(client_id, std::string(status.message()),
                ClientDropReason::INVALID_PUBLIC_KEY);
    return FCP_STATUS(OK);
  }

  set_client_status(client_id, ClientStatus::ADVERTISE_KEYS_RECEIVED);
  number_of_clients_ready_for_next_round_++;
  number_of_messages_received_in_this_round_++;

  return FCP_STATUS(OK);
}

bool SecAggServerR0AdvertiseKeysState::IsNumberOfIncludedInputsCommitted()
    const {
  return false;
}

int SecAggServerR0AdvertiseKeysState::MinimumMessagesNeededForNextRound()
    const {
  return std::max(0, minimum_number_of_clients_to_proceed() -
                         number_of_clients_ready_for_next_round_);
}

int SecAggServerR0AdvertiseKeysState::NumberOfPendingClients() const {
  return NumberOfAliveClients() - number_of_clients_ready_for_next_round_;
}

void SecAggServerR0AdvertiseKeysState::HandleAbortClient(
    uint32_t client_id, ClientDropReason reason_code) {
  number_of_clients_failed_before_sending_masked_input_++;
  if (client_status(client_id) == ClientStatus::ADVERTISE_KEYS_RECEIVED) {
    number_of_clients_ready_for_next_round_--;
    // Remove that client's public keys as if they were never sent. This will
    // avoid wasted computation and bandwidth.
    impl()->ErasePublicKeysForClient(client_id);
  }
  set_client_status(client_id, ClientStatus::DEAD_BEFORE_SENDING_ANYTHING);
  if (NumberOfAliveClients() < minimum_number_of_clients_to_proceed()) {
    needs_to_abort_ = true;
  }
}

StatusOr<std::unique_ptr<SecAggServerState>>
SecAggServerR0AdvertiseKeysState::ProceedToNextRound() {
  if (!ReadyForNextRound()) {
    return FCP_STATUS(UNAVAILABLE);
  }
  if (needs_to_abort_) {
    std::string error_string = "Too many clients aborted.";
    ServerToClientWrapperMessage abort_message;
    abort_message.mutable_abort()->set_diagnostic_info(error_string);
    abort_message.mutable_abort()->set_early_success(false);
    BroadcastMessage(abort_message);

    return AbortState(error_string,
                      SecAggServerOutcome::NOT_ENOUGH_CLIENTS_REMAINING);
  }

  // Abort all clients that haven't yet sent a message.
  for (int i = 0; i < total_number_of_clients(); ++i) {
    if (!IsClientDead(i) &&
        client_status(i) != ClientStatus::ADVERTISE_KEYS_RECEIVED) {
      AbortClient(
          i,
          "Client did not send AdvertiseKeys message before round transition.",
          ClientDropReason::NO_ADVERTISE_KEYS);
    }
  }

  impl()->ComputeSessionId();

  ServerToClientWrapperMessage message_to_client;
  message_to_client.mutable_share_keys_request()->set_session_id(
      impl()->session_id().data);
  FCP_RETURN_IF_ERROR(impl()->InitializeShareKeysRequest(
      message_to_client.mutable_share_keys_request()));

  for (int i = 0; i < total_number_of_clients(); ++i) {
    //  Reuse the common parts of the ShareKeysRequest message and update the
    //  client-specific parts.
    if (!IsClientDead(i)) {
      impl()->PrepareShareKeysRequestForClient(
          i, message_to_client.mutable_share_keys_request());
      Send(i, message_to_client);
    }
  }

  // Pairs of public keys are no longer needed beyond this point as the server
  // has already forwarded them to the clients.
  impl()->ClearPairsOfPublicKeys();

  return {std::make_unique<SecAggServerR1ShareKeysState>(
      ExitState(StateTransition::kSuccess),
      number_of_clients_failed_after_sending_masked_input_,
      number_of_clients_failed_before_sending_masked_input_,
      number_of_clients_terminated_without_unmasking_)};
}

bool SecAggServerR0AdvertiseKeysState::ReadyForNextRound() const {
  return (number_of_clients_ready_for_next_round_ >=
          minimum_number_of_clients_to_proceed()) ||
         (needs_to_abort_);
}

}  // namespace secagg
}  // namespace fcp
