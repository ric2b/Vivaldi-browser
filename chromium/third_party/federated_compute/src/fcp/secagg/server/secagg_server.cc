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

#include "fcp/secagg/server/secagg_server.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_set.h"
#include "absl/strings/str_cat.h"
#include "fcp/base/monitoring.h"
#include "fcp/secagg/server/aes/aes_secagg_server_protocol_impl.h"
#include "fcp/secagg/server/experiments_names.h"
#include "fcp/secagg/server/graph_parameter_finder.h"
#include "fcp/secagg/server/secagg_scheduler.h"
#include "fcp/secagg/server/secagg_server_aborted_state.h"
#include "fcp/secagg/server/secagg_server_enums.pb.h"
#include "fcp/secagg/server/secagg_server_messages.pb.h"
#include "fcp/secagg/server/secagg_server_metrics_listener.h"
#include "fcp/secagg/server/secagg_server_r0_advertise_keys_state.h"
#include "fcp/secagg/server/secagg_server_state.h"
#include "fcp/secagg/server/secagg_trace_utility.h"
#include "fcp/secagg/server/secret_sharing_graph.h"
#include "fcp/secagg/server/secret_sharing_graph_factory.h"
#include "fcp/secagg/server/send_to_clients_interface.h"
#include "fcp/secagg/server/tracing_schema.h"
#include "fcp/secagg/shared/aes_ctr_prng_factory.h"
#include "fcp/secagg/shared/aes_prng_factory.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/tracing/tracing_span.h"

namespace fcp {
namespace secagg {

SecAggServer::SecAggServer(std::unique_ptr<SecAggServerProtocolImpl> impl) {
  state_ = std::make_unique<SecAggServerR0AdvertiseKeysState>(std::move(impl));

  // Start the span for the current state. The rest of the state span
  // transitioning is done in TransitionState.
  state_span_ = std::make_unique<UnscopedTracingSpan<SecureAggServerState>>(
      span_.Ref(), TracingState(state_->State()));
}

StatusOr<std::unique_ptr<SecAggServer>> SecAggServer::Create(
    int minimum_number_of_clients_to_proceed, int total_number_of_clients,
    const std::vector<InputVectorSpecification>& input_vector_specs,
    SendToClientsInterface* sender,
    std::unique_ptr<SecAggServerMetricsListener> metrics,
    std::unique_ptr<SecAggScheduler> prng_runner,
    std::unique_ptr<ExperimentsInterface> experiments,
    const SecureAggregationRequirements& threat_model) {
  TracingSpan<CreateSecAggServer> span;
  SecretSharingGraphFactory factory;
  std::unique_ptr<SecretSharingGraph> secret_sharing_graph;
  ServerVariant server_variant = ServerVariant::UNKNOWN_VERSION;

  bool is_fullgraph_protocol_variant =
      experiments->IsEnabled(kFullgraphSecAggExperiment);
  int degree, threshold;
  // We first compute parameters degree and threshold for the subgraph variant,
  // unless the kFullgraphSecAggExperiment is enabled, and then set
  // is_subgraph_protocol_variant to false if the parameter finding procedure
  // fails. In that case we resort to classical full-graph secagg.
  // This will happen for very small values of total_number_of_clients (e.g. <
  // 65), i.e. cohort sizes where subgraph-secagg does not give much advantage.
  if (!is_fullgraph_protocol_variant) {
    if (experiments->IsEnabled(kForceSubgraphSecAggExperiment)) {
      // In kForceSubgraphSecAggExperiment (which is only for testing
      // purposes) we fix the degree in the Harary graph to be half the number
      // of clients (rounding to the next odd number to account for self-edges
      // as above) and degree to be half of the degree (or 2 whatever is
      // larger). This means that, for example in a simple test with 5
      // clients, each client shares keys with 2 other clients and the
      // threshold is one.
      degree = total_number_of_clients / 2;
      if (degree % 2 == 0) {
        degree += 1;
      }
      threshold = std::max(2, degree / 2);

    } else {
      // kSubgraphSecAggCuriousServerExperiment sets the threat model to
      // CURIOUS_SERVER in subgraph-secagg executions.
      // This experiment was introduced as part of go/subgraph-secagg-rollout
      // and is temporary (see b/191179307).
      StatusOr<fcp::secagg::HararyGraphParameters>
          computed_params_status_or_value;
      if (experiments->IsEnabled(kSubgraphSecAggCuriousServerExperiment)) {
        SecureAggregationRequirements alternate_threat_model = threat_model;
        alternate_threat_model.set_adversary_class(
            AdversaryClass::CURIOUS_SERVER);
        computed_params_status_or_value = ComputeHararyGraphParameters(
            total_number_of_clients, alternate_threat_model);
      } else {
        computed_params_status_or_value =
            ComputeHararyGraphParameters(total_number_of_clients, threat_model);
      }
      if (computed_params_status_or_value.ok()) {
        // We add 1 to the computed degree to account for a self-edge in the
        // SecretSharingHararyGraph graph
        degree = computed_params_status_or_value->degree + 1;
        threshold = computed_params_status_or_value->threshold;
      } else {
        is_fullgraph_protocol_variant = true;
      }
    }
  }

  // In both the FullGraph and SubGraph variants, the protocol only successfully
  // completes and returns a sum if no more than
  // floor(total_number_of_clients * threat_model.estimated_dropout_rate())
  // clients dropout before the end of the protocol execution. This ensure that
  // at least ceil(total_number_of_clients *(1. -
  // threat_model.estimated_dropout_rate() -
  // threat_model.adversarial_client_rate)) values from honest clients are
  // included in the final sum.
  // The protocol allows to make that threshold larger by providing a larger
  // value of minimum_number_of_clients_to_proceed to the create function, but
  // never lower.
  minimum_number_of_clients_to_proceed =
      std::max(minimum_number_of_clients_to_proceed,
               static_cast<int>(
                   std::ceil(total_number_of_clients *
                             (1. - threat_model.estimated_dropout_rate()))));
  if (is_fullgraph_protocol_variant) {
    // We're instantiating full-graph secagg, either because that was
    // the intent of the caller (by setting kFullgraphSecAggExperiment), or
    // because ComputeHararyGraphParameters returned and error.
    FCP_RETURN_IF_ERROR(CheckFullGraphParameters(
        total_number_of_clients, minimum_number_of_clients_to_proceed,
        threat_model));
    secret_sharing_graph = factory.CreateCompleteGraph(
        total_number_of_clients, minimum_number_of_clients_to_proceed);
    server_variant = ServerVariant::NATIVE_V1;
    Trace<FullGraphServerParameters>(
        total_number_of_clients, minimum_number_of_clients_to_proceed,
        experiments->IsEnabled(kSecAggAsyncRound2Experiment));
  } else {
    secret_sharing_graph =
        factory.CreateHararyGraph(total_number_of_clients, degree, threshold);
    server_variant = ServerVariant::NATIVE_SUBGRAPH;
    Trace<SubGraphServerParameters>(
        total_number_of_clients, degree, threshold,
        minimum_number_of_clients_to_proceed,
        experiments->IsEnabled(kSecAggAsyncRound2Experiment));
  }

  return absl::WrapUnique(
      new SecAggServer(std::make_unique<AesSecAggServerProtocolImpl>(
          std::move(secret_sharing_graph), minimum_number_of_clients_to_proceed,
          input_vector_specs, std::move(metrics),
          std::make_unique<AesCtrPrngFactory>(), sender, std::move(prng_runner),
          std::vector<ClientStatus>(total_number_of_clients,
                                    ClientStatus::READY_TO_START),
          server_variant, std::move(experiments))));
}

Status SecAggServer::Abort() {
  const std::string reason = "Abort upon external request.";
  TracingSpan<AbortSecAggServer> span(state_span_->Ref(), reason);
  FCP_RETURN_IF_ERROR(ErrorIfAbortedOrCompleted());
  TransitionState(state_->Abort(reason, SecAggServerOutcome::EXTERNAL_REQUEST));
  return FCP_STATUS(OK);
}

Status SecAggServer::Abort(const std::string& reason,
                           SecAggServerOutcome outcome) {
  const std::string formatted_reason =
      absl::StrCat("Abort upon external request for reason <", reason, ">.");
  TracingSpan<AbortSecAggServer> span(state_span_->Ref(), formatted_reason);
  FCP_RETURN_IF_ERROR(ErrorIfAbortedOrCompleted());
  TransitionState(state_->Abort(formatted_reason, outcome));
  return FCP_STATUS(OK);
}

std::string MakeClientAbortMessage(ClientAbortReason reason) {
  return absl::StrCat("The protocol is closing client with ClientAbortReason <",
                      ClientAbortReason_Name(reason), ">.");
}

Status SecAggServer::AbortClient(uint32_t client_id, ClientAbortReason reason) {
  TracingSpan<AbortSecAggClient> span(
      state_span_->Ref(), client_id,
      ClientAbortReason_descriptor()->FindValueByNumber(reason)->name());
  FCP_RETURN_IF_ERROR(ErrorIfAbortedOrCompleted());
  FCP_RETURN_IF_ERROR(ValidateClientId(client_id));
  // By default, put all AbortClient calls in the same bucket (with some
  // exceptions below).
  ClientDropReason client_drop_reason =
      ClientDropReason::SERVER_PROTOCOL_ABORT_CLIENT;
  bool notify_client = false;
  bool log_metrics = true;
  std::string message;
  // Handle all specific abortClient cases
  switch (reason) {
    case ClientAbortReason::INVALID_MESSAGE:
      notify_client = true;
      message = MakeClientAbortMessage(reason);
      break;
    case ClientAbortReason::CONNECTION_DROPPED:
      client_drop_reason = ClientDropReason::CONNECTION_CLOSED;
      break;
    default:
      log_metrics = false;
      message = MakeClientAbortMessage(reason);
      break;
  }

  state_->AbortClient(client_id, message, client_drop_reason, notify_client,
                      log_metrics);
  return FCP_STATUS(OK);
}

Status SecAggServer::ProceedToNextRound() {
  TracingSpan<ProceedToNextSecAggRound> span(state_span_->Ref());
  FCP_RETURN_IF_ERROR(ErrorIfAbortedOrCompleted());
  StatusOr<std::unique_ptr<SecAggServerState>> status_or_next_state =
      state_->ProceedToNextRound();
  if (status_or_next_state.ok()) {
    TransitionState(std::move(status_or_next_state.value()));
  }
  return status_or_next_state.status();
}

StatusOr<bool> SecAggServer::ReceiveMessage(
    uint32_t client_id, std::unique_ptr<ClientToServerWrapperMessage> message) {
  TracingSpan<ReceiveSecAggMessage> span(state_span_->Ref(), client_id);
  FCP_RETURN_IF_ERROR(ErrorIfAbortedOrCompleted());
  FCP_RETURN_IF_ERROR(ValidateClientId(client_id));
  FCP_RETURN_IF_ERROR(state_->HandleMessage(client_id, std::move(message)));
  return ReadyForNextRound();
}

bool SecAggServer::SetAsyncCallback(std::function<void()> async_callback) {
  return state_->SetAsyncCallback(async_callback);
}

void SecAggServer::TransitionState(
    std::unique_ptr<SecAggServerState> new_state) {
  // Reset state_span_ before creating a new unscoped span for the next state
  // to ensure old span is destructed before the new one is created.
  state_span_.reset();
  state_ = std::move(new_state);
  state_span_ = std::make_unique<UnscopedTracingSpan<SecureAggServerState>>(
      span_.Ref(), TracingState(state_->State()));
  state_->EnterState();
}

absl::flat_hash_set<uint32_t> SecAggServer::AbortedClientIds() const {
  return state_->AbortedClientIds();
}

StatusOr<std::string> SecAggServer::ErrorMessage() const {
  return state_->ErrorMessage();
}

bool SecAggServer::IsAborted() const { return state_->IsAborted(); }

bool SecAggServer::IsCompletedSuccessfully() const {
  return state_->IsCompletedSuccessfully();
}

bool SecAggServer::IsNumberOfIncludedInputsCommitted() const {
  return state_->IsNumberOfIncludedInputsCommitted();
}

StatusOr<int> SecAggServer::MinimumMessagesNeededForNextRound() const {
  FCP_RETURN_IF_ERROR(ErrorIfAbortedOrCompleted());
  return state_->MinimumMessagesNeededForNextRound();
}

int SecAggServer::NumberOfAliveClients() const {
  return state_->NumberOfAliveClients();
}

int SecAggServer::NumberOfClientsFailedAfterSendingMaskedInput() const {
  return state_->NumberOfClientsFailedAfterSendingMaskedInput();
}

int SecAggServer::NumberOfClientsFailedBeforeSendingMaskedInput() const {
  return state_->NumberOfClientsFailedBeforeSendingMaskedInput();
}

int SecAggServer::NumberOfClientsTerminatedWithoutUnmasking() const {
  return state_->NumberOfClientsTerminatedWithoutUnmasking();
}

int SecAggServer::NumberOfIncludedInputs() const {
  return state_->NumberOfIncludedInputs();
}

int SecAggServer::NumberOfPendingClients() const {
  return state_->NumberOfPendingClients();
}

StatusOr<int> SecAggServer::NumberOfClientsReadyForNextRound() const {
  FCP_RETURN_IF_ERROR(ErrorIfAbortedOrCompleted());
  return state_->NumberOfClientsReadyForNextRound();
}

StatusOr<int> SecAggServer::NumberOfMessagesReceivedInThisRound() const {
  FCP_RETURN_IF_ERROR(ErrorIfAbortedOrCompleted());
  return state_->NumberOfMessagesReceivedInThisRound();
}

StatusOr<bool> SecAggServer::ReadyForNextRound() const {
  FCP_RETURN_IF_ERROR(ErrorIfAbortedOrCompleted());
  return state_->ReadyForNextRound();
}

StatusOr<std::unique_ptr<SecAggVectorMap>> SecAggServer::Result() {
  return state_->Result();
}

int SecAggServer::NumberOfNeighbors() const {
  return state_->number_of_neighbors();
}

int SecAggServer::MinimumSurvivingNeighborsForReconstruction() const {
  return state_->minimum_surviving_neighbors_for_reconstruction();
}

SecAggServerStateKind SecAggServer::State() const { return state_->State(); }

Status SecAggServer::ValidateClientId(uint32_t client_id) const {
  if (client_id >= state_->total_number_of_clients()) {
    return FCP_STATUS(FAILED_PRECONDITION)
           << "Client Id " << client_id
           << " is outside of the expected bounds - 0 to "
           << state_->total_number_of_clients();
  }
  return FCP_STATUS(OK);
}

Status SecAggServer::ErrorIfAbortedOrCompleted() const {
  if (state_->IsAborted()) {
    return FCP_STATUS(FAILED_PRECONDITION)
           << "The server has already aborted. The request cannot be "
              "satisfied.";
  }
  if (state_->IsCompletedSuccessfully()) {
    return FCP_STATUS(FAILED_PRECONDITION)
           << "The server has already completed the protocol. "
           << "Call getOutput() to retrieve the output.";
  }
  return FCP_STATUS(OK);
}

}  // namespace secagg
}  // namespace fcp
