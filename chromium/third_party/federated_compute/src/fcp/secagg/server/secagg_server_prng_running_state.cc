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

#include "fcp/secagg/server/secagg_server_prng_running_state.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/synchronization/mutex.h"
#include "fcp/base/monitoring.h"
#include "fcp/secagg/server/secagg_server_completed_state.h"
#include "fcp/tracing/tracing_span.h"

namespace fcp {
namespace secagg {

SecAggServerPrngRunningState::SecAggServerPrngRunningState(
    std::unique_ptr<SecAggServerProtocolImpl> impl,
    int number_of_clients_failed_after_sending_masked_input,
    int number_of_clients_failed_before_sending_masked_input,
    int number_of_clients_terminated_without_unmasking)
    : SecAggServerState(number_of_clients_failed_after_sending_masked_input,
                        number_of_clients_failed_before_sending_masked_input,
                        number_of_clients_terminated_without_unmasking,
                        SecAggServerStateKind::PRNG_RUNNING, std::move(impl)),
      completion_status_(std::nullopt) {}

SecAggServerPrngRunningState::~SecAggServerPrngRunningState() = default;

Status SecAggServerPrngRunningState::HandleMessage(
    uint32_t client_id, const ClientToServerWrapperMessage& message) {
  MessageReceived(message, false);  // Messages are always unexpected here.
  if (message.has_abort()) {
    AbortClient(client_id, "", ClientDropReason::SENT_ABORT_MESSAGE,
                /*notify=*/false);
  } else {
    AbortClient(client_id, "Non-abort message sent during PrngUnmasking step.",
                ClientDropReason::UNEXPECTED_MESSAGE_TYPE);
  }
  return FCP_STATUS(OK);
}

void SecAggServerPrngRunningState::HandleAbort() {
  if (async_token_) {
    async_token_->Cancel();
  }
}

StatusOr<SecAggServerProtocolImpl::PrngWorkItems>
SecAggServerPrngRunningState::Initialize() {
  // Shamir reconstruction part of PRNG
  absl::Time reconstruction_start = absl::Now();
  FCP_ASSIGN_OR_RETURN(auto shamir_reconstruction_result,
                       impl()->HandleShamirReconstruction());
  auto elapsed_millis =
      absl::ToInt64Milliseconds(absl::Now() - reconstruction_start);
  if (metrics()) {
    metrics()->ShamirReconstructionTimes(elapsed_millis);
  }
  Trace<ShamirReconstruction>(elapsed_millis);

  // Generating workitems for PRNG computation.
  return impl()->InitializePrng(std::move(shamir_reconstruction_result));
}

void SecAggServerPrngRunningState::EnterState() {
  auto initialize_result = Initialize();

  if (!initialize_result.ok()) {
    absl::MutexLock lock(&mutex_);
    completion_status_ = initialize_result.status();
    return;
  }

  auto work_items = std::move(initialize_result).value();

  // Scheduling workitems to run.
  prng_started_time_ = absl::Now();

  async_token_ = impl()->StartPrng(
      work_items, [this](Status status) { this->PrngRunnerFinished(status); });
}

bool SecAggServerPrngRunningState::SetAsyncCallback(
    std::function<void()> async_callback) {
  absl::MutexLock lock(&mutex_);
  FCP_CHECK(async_callback != nullptr) << "async_callback is expected";

  if (completion_status_.has_value()) {
    // PRNG computation has already finished.
    impl()->scheduler()->ScheduleCallback(async_callback);
  } else {
    prng_done_callback_ = async_callback;
  }
  return true;
}

void SecAggServerPrngRunningState::PrngRunnerFinished(Status final_status) {
  auto elapsed_millis =
      absl::ToInt64Milliseconds(absl::Now() - prng_started_time_);
  if (metrics()) {
    metrics()->PrngExpansionTimes(elapsed_millis);
  }
  Trace<PrngExpansion>(elapsed_millis);

  std::function<void()> prng_done_callback;
  {
    absl::MutexLock lock(&mutex_);
    completion_status_ = final_status;
    prng_done_callback = prng_done_callback_;
  }

  if (prng_done_callback) {
    prng_done_callback();
  }
}

void SecAggServerPrngRunningState::HandleAbortClient(
    uint32_t client_id, ClientDropReason reason_code) {
  set_client_status(client_id,
                    ClientStatus::DEAD_AFTER_UNMASKING_RESPONSE_RECEIVED);
}

StatusOr<std::unique_ptr<SecAggServerState>>
SecAggServerPrngRunningState::ProceedToNextRound() {
  // Block if StartPrng is still being called. That done to ensure that
  // StartPrng doesn't use *this* object after it has been destroyed by
  // the code that called ProceedToNextRound.
  absl::MutexLock lock(&mutex_);

  if (!completion_status_.has_value()) {
    return FCP_STATUS(UNAVAILABLE);
  }

  // Don't send any messages; every client either got an "early success"
  // notification at the end of Round 3, marked itself completed after sending
  // its Round 3 message, or was already aborted.
  if (completion_status_.value().ok()) {
    return std::make_unique<SecAggServerCompletedState>(
        ExitState(StateTransition::kSuccess),
        number_of_clients_failed_after_sending_masked_input_,
        number_of_clients_failed_before_sending_masked_input_,
        number_of_clients_terminated_without_unmasking_);
  } else {
    return AbortState(std::string(completion_status_.value().message()),
                      SecAggServerOutcome::UNHANDLED_ERROR);
  }
}

bool SecAggServerPrngRunningState::ReadyForNextRound() const {
  absl::MutexLock lock(&mutex_);
  return completion_status_.has_value();
}

int SecAggServerPrngRunningState::NumberOfIncludedInputs() const {
  return total_number_of_clients() -
         number_of_clients_failed_before_sending_masked_input_;
}

bool SecAggServerPrngRunningState::IsNumberOfIncludedInputsCommitted() const {
  return true;
}

}  // namespace secagg
}  // namespace fcp
