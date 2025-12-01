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

#include "fcp/secagg/server/secagg_server_r2_masked_input_coll_state.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "fcp/base/monitoring.h"
#include "fcp/secagg/server/secagg_server_r3_unmasking_state.h"

namespace fcp {
namespace secagg {

SecAggServerR2MaskedInputCollState::SecAggServerR2MaskedInputCollState(
    std::unique_ptr<SecAggServerProtocolImpl> impl,
    int number_of_clients_failed_after_sending_masked_input,
    int number_of_clients_failed_before_sending_masked_input,
    int number_of_clients_terminated_without_unmasking)
    : SecAggServerState(number_of_clients_failed_after_sending_masked_input,
                        number_of_clients_failed_before_sending_masked_input,
                        number_of_clients_terminated_without_unmasking,
                        SecAggServerStateKind::R2_MASKED_INPUT_COLLECTION,
                        std::move(impl)) {
  async_token_ = this->impl()->SetupMaskedInputCollection();
}

SecAggServerR2MaskedInputCollState::~SecAggServerR2MaskedInputCollState() =
    default;

Status SecAggServerR2MaskedInputCollState::HandleMessage(
    uint32_t client_id, const ClientToServerWrapperMessage& message) {
  return FCP_STATUS(FAILED_PRECONDITION)
         << "Call to deprecated HandleMessage method.";
}

Status SecAggServerR2MaskedInputCollState::HandleMessage(
    uint32_t client_id, std::unique_ptr<ClientToServerWrapperMessage> message) {
  if (message->has_abort()) {
    MessageReceived(*message, false);
    AbortClient(client_id, "", ClientDropReason::SENT_ABORT_MESSAGE,
                /*notify=*/false);
    return FCP_STATUS(OK);
  }
  // If the client has aborted already, ignore its messages.
  if (client_status(client_id) != ClientStatus::SHARE_KEYS_RECEIVED) {
    MessageReceived(*message, false);
    AbortClient(client_id,
                "Not expecting an MaskedInputCollectionResponse from this "
                "client - either the client already aborted or one such "
                "message was already received.",
                ClientDropReason::MASKED_INPUT_UNEXPECTED);
    return FCP_STATUS(OK);
  }
  if (!message->has_masked_input_response()) {
    MessageReceived(*message, false);
    AbortClient(client_id,
                "Message type received is different from what was expected.",
                ClientDropReason::UNEXPECTED_MESSAGE_TYPE);
    return FCP_STATUS(OK);
  }
  MessageReceived(*message, true);

  Status check_and_accumulate_status =
      impl()->HandleMaskedInputCollectionResponse(
          std::make_unique<MaskedInputCollectionResponse>(
              std::move(*message->mutable_masked_input_response())));
  if (!check_and_accumulate_status.ok()) {
    AbortClient(client_id, std::string(check_and_accumulate_status.message()),
                ClientDropReason::INVALID_MASKED_INPUT);
    return FCP_STATUS(OK);
  }
  set_client_status(client_id, ClientStatus::MASKED_INPUT_RESPONSE_RECEIVED);
  number_of_messages_received_in_this_round_++;
  number_of_clients_ready_for_next_round_++;
  return FCP_STATUS(OK);
}

bool SecAggServerR2MaskedInputCollState::IsNumberOfIncludedInputsCommitted()
    const {
  return false;
}

int SecAggServerR2MaskedInputCollState::MinimumMessagesNeededForNextRound()
    const {
  return std::max(0, minimum_number_of_clients_to_proceed() -
                         number_of_clients_ready_for_next_round_);
}

int SecAggServerR2MaskedInputCollState::NumberOfIncludedInputs() const {
  return number_of_messages_received_in_this_round_;
}

int SecAggServerR2MaskedInputCollState::NumberOfPendingClients() const {
  return NumberOfAliveClients() - number_of_clients_ready_for_next_round_;
}

void SecAggServerR2MaskedInputCollState::HandleAbortClient(
    uint32_t client_id, ClientDropReason reason_code) {
  if (client_status(client_id) ==
      ClientStatus::MASKED_INPUT_RESPONSE_RECEIVED) {
    number_of_clients_ready_for_next_round_--;
    number_of_clients_failed_after_sending_masked_input_++;
    set_client_status(client_id,
                      ClientStatus::DEAD_AFTER_MASKED_INPUT_RESPONSE_RECEIVED);
  } else {
    number_of_clients_failed_before_sending_masked_input_++;
    clients_aborted_at_round_2_.push_back(client_id);
    set_client_status(client_id, ClientStatus::DEAD_AFTER_SHARE_KEYS_RECEIVED);
  }
  if (NumberOfAliveClients() < minimum_number_of_clients_to_proceed()) {
    needs_to_abort_ = true;
  }
}

void SecAggServerR2MaskedInputCollState::HandleAbort() {
  if (async_token_) {
    async_token_->Cancel();
  }
}

StatusOr<std::unique_ptr<SecAggServerState>>
SecAggServerR2MaskedInputCollState::ProceedToNextRound() {
  if (!ReadyForNextRound()) {
    return FCP_STATUS(UNAVAILABLE);
  }
  if (needs_to_abort_) {
    std::string error_string = "Too many clients aborted.";
    ServerToClientWrapperMessage message;
    message.mutable_abort()->set_diagnostic_info(error_string);
    message.mutable_abort()->set_early_success(false);
    BroadcastMessage(message);
    HandleAbort();

    return AbortState(error_string,
                      SecAggServerOutcome::NOT_ENOUGH_CLIENTS_REMAINING);
  }

  // Close all clients that haven't yet sent a message.
  for (int i = 0; i < total_number_of_clients(); ++i) {
    if (!IsClientDead(i) &&
        client_status(i) != ClientStatus::MASKED_INPUT_RESPONSE_RECEIVED) {
      AbortClient(i,
                  "Client did not send MaskedInputCollectionResponse before "
                  "round transition.",
                  ClientDropReason::NO_MASKED_INPUT);
    }
  }
  // Send to each alive client the list of their aborted neighbors
  for (int i = 0; i < total_number_of_clients(); ++i) {
    if (IsClientDead(i)) {
      continue;
    }
    ServerToClientWrapperMessage message_to_i;
    // Set message to proper type
    auto request = message_to_i.mutable_unmasking_request();
    for (uint32_t aborted_client : clients_aborted_at_round_2_) {
      //  neighbor_index has a value iff i and aborted_client are neighbors
      auto neighbor_index = GetNeighborIndex(i, aborted_client);
      if (neighbor_index.has_value()) {
        // TODO(team): Stop adding + 1 here once we don't need
        // compatibility.
        request->add_dead_3_client_ids(neighbor_index.value() + 1);
      }
    }
    Send(i, message_to_i);
  }

  impl()->FinalizeMaskedInputCollection();

  return {std::make_unique<SecAggServerR3UnmaskingState>(
      ExitState(StateTransition::kSuccess),
      number_of_clients_failed_after_sending_masked_input_,
      number_of_clients_failed_before_sending_masked_input_,
      number_of_clients_terminated_without_unmasking_)};
}

bool SecAggServerR2MaskedInputCollState::SetAsyncCallback(
    std::function<void()> async_callback) {
  if (async_token_) {
    return async_token_->SetAsyncObserver(async_callback);
  }
  return false;
}

bool SecAggServerR2MaskedInputCollState::ReadyForNextRound() const {
  // Async accumulator is not set (this is a synchronous session) or it does not
  // have unobserved work.
  bool is_idle = (!async_token_ || async_token_->IsIdle());
  return is_idle && ((number_of_clients_ready_for_next_round_ >=
                      minimum_number_of_clients_to_proceed()) ||
                     (needs_to_abort_));
}

}  // namespace secagg
}  // namespace fcp
