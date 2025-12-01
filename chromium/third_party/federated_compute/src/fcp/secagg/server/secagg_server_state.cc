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

#include "fcp/secagg/server/secagg_server_state.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_set.h"
#include "absl/time/time.h"
#include "fcp/base/monitoring.h"
#include "fcp/secagg/server/secagg_server_aborted_state.h"
#include "fcp/secagg/server/secagg_server_enums.pb.h"
#include "fcp/secagg/server/secagg_trace_utility.h"
#include "fcp/secagg/server/tracing_schema.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/tracing/tracing_span.h"

namespace fcp {
namespace secagg {

SecAggServerState::SecAggServerState(
    int number_of_clients_failed_after_sending_masked_input,
    int number_of_clients_failed_before_sending_masked_input,
    int number_of_clients_terminated_without_unmasking,
    SecAggServerStateKind state_kind,
    std::unique_ptr<SecAggServerProtocolImpl> impl)
    : needs_to_abort_(false),
      number_of_clients_failed_after_sending_masked_input_(
          number_of_clients_failed_after_sending_masked_input),
      number_of_clients_failed_before_sending_masked_input_(
          number_of_clients_failed_before_sending_masked_input),
      number_of_clients_ready_for_next_round_(0),
      number_of_clients_terminated_without_unmasking_(
          number_of_clients_terminated_without_unmasking),
      number_of_messages_received_in_this_round_(0),
      round_start_(absl::Now()),
      state_kind_(state_kind),
      impl_(std::move(impl)) {}

SecAggServerState::~SecAggServerState() = default;

std::unique_ptr<SecAggServerProtocolImpl>&& SecAggServerState::ExitState(
    StateTransition state_transition_status) {
  bool record_success = state_transition_status == StateTransition::kSuccess;
  auto elapsed_time = absl::ToInt64Milliseconds(absl::Now() - round_start_);
  if (metrics()) {
    metrics()->RoundTimes(state_kind_, record_success, elapsed_time);
    metrics()->RoundSurvivingClients(state_kind_, NumberOfAliveClients());

    // Fractions of clients by state
    absl::flat_hash_map<ClientStatus, int> counts_by_state;
    for (uint32_t i = 0; i < total_number_of_clients(); i++) {
      counts_by_state[client_status(i)]++;
    }
    for (const auto& count_by_state : counts_by_state) {
      double fraction = static_cast<double>(count_by_state.second) /
                        total_number_of_clients();
      Trace<ClientCountsPerState>(TracingState(state_kind_),
                                  ClientStatusType(count_by_state.first),
                                  count_by_state.second, fraction);
      metrics()->RoundCompletionFractions(state_kind_, count_by_state.first,
                                          fraction);
    }
  }
  Trace<StateCompletion>(TracingState(state_kind_), record_success,
                         elapsed_time, NumberOfAliveClients());
  return std::move(impl_);
}

// These methods return default values unless overridden.
bool SecAggServerState::IsAborted() const { return false; }
bool SecAggServerState::IsCompletedSuccessfully() const { return false; }
int SecAggServerState::NumberOfPendingClients() const { return 0; }
int SecAggServerState::NumberOfIncludedInputs() const { return 0; }
int SecAggServerState::MinimumMessagesNeededForNextRound() const { return 0; }
bool SecAggServerState::ReadyForNextRound() const { return false; }

Status SecAggServerState::HandleMessage(
    uint32_t client_id, const ClientToServerWrapperMessage& message) {
  MessageReceived(message, false);
  if (message.message_content_case() ==
      ClientToServerWrapperMessage::MESSAGE_CONTENT_NOT_SET) {
    return FCP_STATUS(FAILED_PRECONDITION)
           << "Server received a message of unknown type from client "
           << client_id << " but was in state " << StateName();
  } else {
    return FCP_STATUS(FAILED_PRECONDITION)
           << "Server received a message of type "
           << message.message_content_case() << " from client " << client_id
           << " but was in state " << StateName();
  }
}

Status SecAggServerState::HandleMessage(
    uint32_t client_id, std::unique_ptr<ClientToServerWrapperMessage> message) {
  return HandleMessage(client_id, *message);
}

StatusOr<std::unique_ptr<SecAggServerState>>
SecAggServerState::ProceedToNextRound() {
  return FCP_STATUS(FAILED_PRECONDITION)
         << "The server cannot proceed to next round from state "
         << StateName();
}

bool SecAggServerState::IsClientDead(uint32_t client_id) const {
  switch (client_status(client_id)) {
    case ClientStatus::DEAD_BEFORE_SENDING_ANYTHING:
    case ClientStatus::DEAD_AFTER_ADVERTISE_KEYS_RECEIVED:
    case ClientStatus::DEAD_AFTER_SHARE_KEYS_RECEIVED:
    case ClientStatus::DEAD_AFTER_MASKED_INPUT_RESPONSE_RECEIVED:
    case ClientStatus::DEAD_AFTER_UNMASKING_RESPONSE_RECEIVED:
      return true;
      break;
    default:
      return false;
  }
}

void SecAggServerState::AbortClient(uint32_t client_id,
                                    const std::string& reason,
                                    ClientDropReason reason_code, bool notify,
                                    bool log_metrics) {
  FCP_CHECK(!(IsAborted() || IsCompletedSuccessfully()));

  if (IsClientDead(client_id)) {
    return;  // without sending a message
  }

  HandleAbortClient(client_id, reason_code);
  if (notify) {
    ServerToClientWrapperMessage message;
    message.mutable_abort()->set_diagnostic_info(reason);
    message.mutable_abort()->set_early_success(reason_code ==
                                               ClientDropReason::EARLY_SUCCESS);
    Send(client_id, message);
  }
  // Clients that have successfully completed the protocol should not be logging
  // metrics.
  if (metrics() && log_metrics &&
      client_status(client_id) !=
          ClientStatus::DEAD_AFTER_UNMASKING_RESPONSE_RECEIVED) {
    metrics()->ClientsDropped(client_status(client_id), reason_code);
  }
  auto elapsed_millis = absl::ToInt64Milliseconds(absl::Now() - round_start_);
  Trace<ClientsDropped>(ClientStatusType(client_status(client_id)),
                        ClientDropReasonType(reason_code), elapsed_millis,
                        reason);
}

std::unique_ptr<SecAggServerState> SecAggServerState::AbortState(
    const std::string& reason, SecAggServerOutcome outcome) {
  if (metrics()) {
    metrics()->ProtocolOutcomes(outcome);
  }
  Trace<SecAggProtocolOutcome>(ConvertSecAccServerOutcomeToTrace(outcome));
  return std::make_unique<SecAggServerAbortedState>(
      reason, ExitState(StateTransition::kAbort),
      number_of_clients_failed_after_sending_masked_input_,
      number_of_clients_failed_before_sending_masked_input_,
      number_of_clients_terminated_without_unmasking_);
}

std::unique_ptr<SecAggServerState> SecAggServerState::Abort(
    const std::string& reason, SecAggServerOutcome outcome) {
  FCP_CHECK(!(IsAborted() || IsCompletedSuccessfully()));

  HandleAbort();

  ServerToClientWrapperMessage message;
  message.mutable_abort()->set_early_success(false);
  message.mutable_abort()->set_diagnostic_info(reason);
  BroadcastMessage(message);

  return AbortState(reason, outcome);
}

StatusOr<std::string> SecAggServerState::ErrorMessage() const {
  return FCP_STATUS(FAILED_PRECONDITION)
         << "Error message requested, but server is in state " << StateName();
}

int SecAggServerState::NumberOfAliveClients() const {
  return total_number_of_clients() -
         number_of_clients_failed_before_sending_masked_input_ -
         number_of_clients_failed_after_sending_masked_input_ -
         number_of_clients_terminated_without_unmasking_;
}

int SecAggServerState::NumberOfMessagesReceivedInThisRound() const {
  return number_of_messages_received_in_this_round_;
}

int SecAggServerState::NumberOfClientsReadyForNextRound() const {
  return number_of_clients_ready_for_next_round_;
}

int SecAggServerState::NumberOfClientsFailedAfterSendingMaskedInput() const {
  return number_of_clients_failed_after_sending_masked_input_;
}

int SecAggServerState::NumberOfClientsFailedBeforeSendingMaskedInput() const {
  return number_of_clients_failed_before_sending_masked_input_;
}

int SecAggServerState::NumberOfClientsTerminatedWithoutUnmasking() const {
  return number_of_clients_terminated_without_unmasking_;
}

bool SecAggServerState::NeedsToAbort() const { return needs_to_abort_; }

absl::flat_hash_set<uint32_t> SecAggServerState::AbortedClientIds() const {
  auto aborted_client_ids_ = absl::flat_hash_set<uint32_t>();
  for (int i = 0; i < total_number_of_clients(); ++i) {
    // Clients that have successfully completed the protocol are not reported
    // as aborted.
    if (IsClientDead(i)) {
      aborted_client_ids_.insert(i);
    }
  }
  return aborted_client_ids_;
}

bool SecAggServerState::SetAsyncCallback(std::function<void()> async_callback) {
  return false;
}

StatusOr<std::unique_ptr<SecAggVectorMap>> SecAggServerState::Result() {
  return FCP_STATUS(UNAVAILABLE)
         << "Result requested, but server is in state " << StateName();
}

SecAggServerStateKind SecAggServerState::State() const { return state_kind_; }

std::string SecAggServerState::StateName() const {
  switch (state_kind_) {
    case SecAggServerStateKind::ABORTED:
      return "Aborted";
    case SecAggServerStateKind::COMPLETED:
      return "Completed";
    case SecAggServerStateKind::PRNG_RUNNING:
      return "PrngRunning";
    case SecAggServerStateKind::R0_ADVERTISE_KEYS:
      return "R0AdvertiseKeys";
    case SecAggServerStateKind::R1_SHARE_KEYS:
      return "R1ShareKeys";
    case SecAggServerStateKind::R2_MASKED_INPUT_COLLECTION:
      return "R2MaskedInputCollection";
    case SecAggServerStateKind::R3_UNMASKING:
      return "R3Unmasking";
    default:
      return "Unknown";
  }
}

void SecAggServerState::MessageReceived(
    const ClientToServerWrapperMessage& message, bool expected) {
  auto elapsed_millis = absl::ToInt64Milliseconds(absl::Now() - round_start_);
  if (metrics()) {
    if (expected) {
      metrics()->ClientResponseTimes(message.message_content_case(),
                                     elapsed_millis);
    }
    metrics()->MessageReceivedSizes(message.message_content_case(), expected,
                                    message.ByteSizeLong());
  }
  Trace<ClientMessageReceived>(GetClientToServerMessageType(message),
                               message.ByteSizeLong(), expected,
                               elapsed_millis);
}

void SecAggServerState::BroadcastMessage(
    const ServerToClientWrapperMessage& message) {
  FCP_CHECK(message.message_content_case() !=
            ServerToClientWrapperMessage::MESSAGE_CONTENT_NOT_SET);
  for (int i = 0; i < total_number_of_clients(); ++i) {
    if (!IsClientDead(i)) {
      if (metrics()) {
        metrics()->IndividualMessageSizes(message.message_content_case(),
                                          message.ByteSizeLong());
      }
      sender()->Send(i, message);
    }
  }
  Trace<BroadcastMessageSent>(GetServerToClientMessageType(message),
                              message.ByteSizeLong());
}

void SecAggServerState::Send(uint32_t recipient_id,
                             const ServerToClientWrapperMessage& message) {
  FCP_CHECK(message.message_content_case() !=
            ServerToClientWrapperMessage::MESSAGE_CONTENT_NOT_SET);
  if (metrics()) {
    metrics()->IndividualMessageSizes(message.message_content_case(),
                                      message.ByteSizeLong());
  }
  sender()->Send(recipient_id, message);

  Trace<IndividualMessageSent>(recipient_id,
                               GetServerToClientMessageType(message),
                               message.ByteSizeLong());
}

}  // namespace secagg
}  // namespace fcp
