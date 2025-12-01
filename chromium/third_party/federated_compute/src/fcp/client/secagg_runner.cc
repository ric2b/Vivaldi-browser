/*
 * Copyright 2022 Google LLC
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
#include "fcp/client/secagg_runner.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/federated_protocol.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/client/log_manager.h"
#include "fcp/client/secagg_event_publisher.h"
#include "fcp/secagg/client/secagg_client.h"
#include "fcp/secagg/client/state_transition_listener_interface.h"
#include "fcp/secagg/shared/aes_ctr_prng_factory.h"
#include "fcp/secagg/shared/crypto_rand_prng.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/secagg_vector.h"

namespace fcp {
namespace client {

using ::fcp::secagg::ClientState;

// Implementation of StateTransitionListenerInterface.
class SecAggStateTransitionListenerImpl
    : public secagg::StateTransitionListenerInterface {
 public:
  SecAggStateTransitionListenerImpl(
      SecAggEventPublisher& secagg_event_publisher, LogManager& log_manager,
      SecAggSendToServerBase& secagg_send_to_server_impl,
      SecAggProtocolDelegate& secagg_protocol_delegate);
  void Transition(secagg::ClientState new_state) override;

  void Started(secagg::ClientState state) override;

  void Stopped(secagg::ClientState state) override;

  void set_execution_session_id(int64_t execution_session_id) override;

 private:
  SecAggEventPublisher& secagg_event_publisher_;
  LogManager& log_manager_;
  SecAggSendToServerBase& secagg_send_to_server_;
  SecAggProtocolDelegate& secagg_protocol_delegate_;
  secagg::ClientState state_ = secagg::ClientState::INITIAL;
};

SecAggStateTransitionListenerImpl::SecAggStateTransitionListenerImpl(
    SecAggEventPublisher& secagg_event_publisher, LogManager& log_manager,
    SecAggSendToServerBase& secagg_send_to_server_impl,
    SecAggProtocolDelegate& secagg_protocol_delegate)
    : secagg_event_publisher_(secagg_event_publisher),
      log_manager_(log_manager),
      secagg_send_to_server_(secagg_send_to_server_impl),
      secagg_protocol_delegate_(secagg_protocol_delegate) {}

void SecAggStateTransitionListenerImpl::Transition(ClientState new_state) {
  FCP_LOG(INFO) << "Transitioning from state: " << static_cast<int>(state_)
                << " to state: " << static_cast<int>(new_state);
  state_ = new_state;
  if (state_ == ClientState::ABORTED) {
    log_manager_.LogDiag(ProdDiagCode::SECAGG_CLIENT_NATIVE_ERROR_GENERIC);
  }
  secagg_event_publisher_.PublishStateTransition(
      new_state, secagg_send_to_server_.last_sent_message_size(),
      secagg_protocol_delegate_.last_received_message_size());
}

void SecAggStateTransitionListenerImpl::Started(ClientState state) {
  // TODO(team): Implement this.
}

void SecAggStateTransitionListenerImpl::Stopped(ClientState state) {
  // TODO(team): Implement this.
}

void SecAggStateTransitionListenerImpl::set_execution_session_id(
    int64_t execution_session_id) {
  secagg_event_publisher_.set_execution_session_id(execution_session_id);
}

SecAggRunnerImpl::SecAggRunnerImpl(
    std::unique_ptr<SecAggSendToServerBase> send_to_server_impl,
    std::unique_ptr<SecAggProtocolDelegate> protocol_delegate,
    SecAggEventPublisher* secagg_event_publisher, LogManager* log_manager,
    InterruptibleRunner* interruptible_runner,
    int64_t expected_number_of_clients,
    int64_t minimum_surviving_clients_for_reconstruction)
    : send_to_server_impl_(std::move(send_to_server_impl)),
      protocol_delegate_(std::move(protocol_delegate)),
      secagg_event_publisher_(*secagg_event_publisher),
      log_manager_(*log_manager),
      interruptible_runner_(*interruptible_runner),
      expected_number_of_clients_(expected_number_of_clients),
      minimum_surviving_clients_for_reconstruction_(
          minimum_surviving_clients_for_reconstruction) {}

absl::Status SecAggRunnerImpl::Run(ComputationResults results) {
  auto secagg_state_transition_listener =
      std::make_unique<SecAggStateTransitionListenerImpl>(
          secagg_event_publisher_, log_manager_, *send_to_server_impl_,
          *protocol_delegate_);
  auto input_map = std::make_unique<secagg::SecAggVectorMap>();
  std::vector<secagg::InputVectorSpecification> input_vector_specification;
  for (auto& [k, v] : results) {
    if (std::holds_alternative<QuantizedTensor>(v)) {
      FCP_ASSIGN_OR_RETURN(uint64_t modulus, protocol_delegate_->GetModulus(k));
      // Note: std::move is used below to ensure that each QuantizedTensor
      // is consumed when converted to SecAggVector and that we don't
      // continue having both in memory for longer than needed.
      auto vector = std::get<QuantizedTensor>(std::move(v));
      if (modulus <= 1 || modulus > secagg::SecAggVector::kMaxModulus) {
        return absl::InternalError(
            absl::StrCat("Invalid SecAgg modulus configuration: ", modulus));
      }
      if (vector.values.empty())
        return absl::InternalError(
            absl::StrCat("Zero sized vector found: ", k));
      int64_t flattened_length = 1;
      for (const auto& size : vector.dimensions) flattened_length *= size;
      auto data_length = vector.values.size();
      if (flattened_length != data_length)
        return absl::InternalError(
            absl::StrCat("Flattened length: ", flattened_length,
                         " does not match vector size: ", data_length));
      for (const auto& value : vector.values) {
        if (value >= modulus) {
          return absl::InternalError(absl::StrCat(
              "The input SecAgg vector doesn't have the appropriate "
              "modulus: element with value ",
              value, " found, max value allowed ", (modulus - 1ULL)));
        }
      }
      input_vector_specification.emplace_back(k, flattened_length, modulus);
      input_map->try_emplace(
          k, absl::MakeConstSpan(vector.values.data(), data_length), modulus);
    }
  }
  secagg_client_ = std::make_unique<secagg::SecAggClient>(
      expected_number_of_clients_,
      minimum_surviving_clients_for_reconstruction_,
      std::move(input_vector_specification),
      std::make_unique<secagg::CryptoRandPrng>(),
      std::move(send_to_server_impl_),
      std::move(secagg_state_transition_listener),
      std::make_unique<secagg::AesCtrPrngFactory>());

  FCP_RETURN_IF_ERROR(interruptible_runner_.Run(
      [this, &input_map]() -> absl::Status {
        FCP_RETURN_IF_ERROR(secagg_client_->Start());
        FCP_RETURN_IF_ERROR(secagg_client_->SetInput(std::move(input_map)));
        while (!secagg_client_->IsCompletedSuccessfully()) {
          absl::StatusOr<secagg::ServerToClientWrapperMessage>
              server_to_client_wrapper_message =
                  this->protocol_delegate_->ReceiveServerMessage();
          if (!server_to_client_wrapper_message.ok()) {
            return absl::Status(
                server_to_client_wrapper_message.status().code(),
                absl::StrCat(
                    "Error during SecAgg receive: ",
                    server_to_client_wrapper_message.status().message()));
          }
          auto result =
              secagg_client_->ReceiveMessage(*server_to_client_wrapper_message);
          if (!result.ok()) {
            this->secagg_event_publisher_.PublishError();
            return absl::Status(result.status().code(),
                                absl::StrCat("Error receiving SecAgg message: ",
                                             result.status().message()));
          }
          if (secagg_client_->IsAborted()) {
            std::string error_message = "error message not found.";
            if (secagg_client_->ErrorMessage().ok())
              error_message = secagg_client_->ErrorMessage().value();
            this->secagg_event_publisher_.PublishAbort(false, error_message);
            return absl::CancelledError("SecAgg aborted: " + error_message);
          }
        }
        return absl::OkStatus();
      },
      [this]() {
        AbortInternal();
        this->protocol_delegate_->Abort();
      }));
  return absl::OkStatus();
}

void SecAggRunnerImpl::AbortInternal() {
  log_manager_.LogDiag(ProdDiagCode::SECAGG_CLIENT_NATIVE_ERROR_GENERIC);
  auto abort_message = "Client-initiated abort.";
  auto result = secagg_client_->Abort(abort_message);
  if (!result.ok()) {
    FCP_LOG(ERROR) << "Could not initiate client abort, code: " << result.code()
                   << " message: " << result.message();
  }
  // Note: the implementation assumes that secagg_event_publisher
  // cannot hang indefinitely, i.e. does not need its own interruption
  // trigger.
  secagg_event_publisher_.PublishAbort(true, abort_message);
}

std::unique_ptr<SecAggRunner> SecAggRunnerFactoryImpl::CreateSecAggRunner(
    std::unique_ptr<SecAggSendToServerBase> send_to_server_impl,
    std::unique_ptr<SecAggProtocolDelegate> protocol_delegate,
    SecAggEventPublisher* secagg_event_publisher, LogManager* log_manager,
    InterruptibleRunner* interruptible_runner,
    int64_t expected_number_of_clients,
    int64_t minimum_surviving_clients_for_reconstruction) {
  return std::make_unique<SecAggRunnerImpl>(
      std::move(send_to_server_impl), std::move(protocol_delegate),
      secagg_event_publisher, log_manager, interruptible_runner,
      expected_number_of_clients, minimum_surviving_clients_for_reconstruction);
}

}  // namespace client
}  // namespace fcp
