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

#include "fcp/secagg/server/secagg_server_r3_unmasking_state.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "fcp/base/monitoring.h"
#include "fcp/secagg/server/secagg_server_prng_running_state.h"

namespace fcp {
namespace secagg {

SecAggServerR3UnmaskingState::SecAggServerR3UnmaskingState(
    std::unique_ptr<SecAggServerProtocolImpl> impl,
    int number_of_clients_failed_after_sending_masked_input,
    int number_of_clients_failed_before_sending_masked_input,
    int number_of_clients_terminated_without_unmasking)
    : SecAggServerState(number_of_clients_failed_after_sending_masked_input,
                        number_of_clients_failed_before_sending_masked_input,
                        number_of_clients_terminated_without_unmasking,
                        SecAggServerStateKind::R3_UNMASKING, std::move(impl)) {
  this->impl()->SetUpShamirSharesTables();
}

SecAggServerR3UnmaskingState::~SecAggServerR3UnmaskingState() = default;

Status SecAggServerR3UnmaskingState::HandleMessage(
    uint32_t client_id, const ClientToServerWrapperMessage& message) {
  if (message.has_abort()) {
    MessageReceived(message, false);
    AbortClient(client_id, "Client sent abort message.",
                ClientDropReason::SENT_ABORT_MESSAGE,
                /*notify=*/false);
    return FCP_STATUS(OK);
  }
  // If the client has aborted already, ignore its messages.
  if (client_status(client_id) !=
      ClientStatus::MASKED_INPUT_RESPONSE_RECEIVED) {
    MessageReceived(message, false);
    AbortClient(
        client_id,
        "Not expecting an UnmaskingResponse from this client - either the "
        "client already aborted or one such message was already received.",
        ClientDropReason::UNMASKING_RESPONSE_UNEXPECTED);
    return FCP_STATUS(OK);
  }
  if (!message.has_unmasking_response()) {
    MessageReceived(message, false);
    AbortClient(client_id,
                "Message type received is different from what was expected.",
                ClientDropReason::UNEXPECTED_MESSAGE_TYPE);
    return FCP_STATUS(OK);
  }
  MessageReceived(message, true);

  Status status =
      impl()->HandleUnmaskingResponse(client_id, message.unmasking_response());
  if (!status.ok()) {
    AbortClient(client_id, std::string(status.message()),
                ClientDropReason::INVALID_UNMASKING_RESPONSE);
    return FCP_STATUS(OK);
  }

  set_client_status(client_id, ClientStatus::UNMASKING_RESPONSE_RECEIVED);
  number_of_messages_received_in_this_round_++;
  number_of_clients_ready_for_next_round_++;
  return FCP_STATUS(OK);
}

bool SecAggServerR3UnmaskingState::IsNumberOfIncludedInputsCommitted() const {
  return true;
}

int SecAggServerR3UnmaskingState::MinimumMessagesNeededForNextRound() const {
  return std::max(0, minimum_number_of_clients_to_proceed() -
                         number_of_messages_received_in_this_round_);
}

int SecAggServerR3UnmaskingState::NumberOfIncludedInputs() const {
  return total_number_of_clients() -
         number_of_clients_failed_before_sending_masked_input_;
}

int SecAggServerR3UnmaskingState::NumberOfPendingClients() const {
  return NumberOfAliveClients() - number_of_clients_ready_for_next_round_;
}

void SecAggServerR3UnmaskingState::HandleAbortClient(
    uint32_t client_id, ClientDropReason reason_code) {
  if (client_status(client_id) == ClientStatus::UNMASKING_RESPONSE_RECEIVED) {
    set_client_status(client_id,
                      ClientStatus::DEAD_AFTER_UNMASKING_RESPONSE_RECEIVED);
    return;
  }
  if (reason_code == ClientDropReason::EARLY_SUCCESS) {
    number_of_clients_terminated_without_unmasking_++;
  } else {
    number_of_clients_failed_after_sending_masked_input_++;
  }
  set_client_status(client_id,
                    ClientStatus::DEAD_AFTER_MASKED_INPUT_RESPONSE_RECEIVED);
  if (NumberOfPendingClients() + number_of_messages_received_in_this_round_ <
      minimum_number_of_clients_to_proceed()) {
    needs_to_abort_ = true;
  }
}

bool SecAggServerR3UnmaskingState::ReadyForNextRound() const {
  return (number_of_messages_received_in_this_round_ >=
          minimum_number_of_clients_to_proceed()) ||
         (needs_to_abort_);
}

StatusOr<std::unique_ptr<SecAggServerState> >
SecAggServerR3UnmaskingState::ProceedToNextRound() {
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

  // Abort all clients that haven't yet sent a message, but let them count it as
  // a success.
  for (int i = 0; i < total_number_of_clients(); ++i) {
    if (client_status(i) != ClientStatus::UNMASKING_RESPONSE_RECEIVED) {
      AbortClient(
          i,
          "Client did not send unmasking response but protocol completed "
          "successfully.",
          ClientDropReason::EARLY_SUCCESS);
    }
  }

  return {std::make_unique<SecAggServerPrngRunningState>(
      ExitState(StateTransition::kSuccess),
      number_of_clients_failed_after_sending_masked_input_,
      number_of_clients_failed_before_sending_masked_input_,
      number_of_clients_terminated_without_unmasking_)};
}

}  // namespace secagg
}  // namespace fcp
