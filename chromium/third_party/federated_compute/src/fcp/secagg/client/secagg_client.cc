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

#include "fcp/secagg/client/secagg_client.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "fcp/base/monitoring.h"
#include "fcp/secagg/client/secagg_client_r0_advertise_keys_input_not_set_state.h"
#include "fcp/secagg/client/secagg_client_state.h"
#include "fcp/secagg/client/send_to_server_interface.h"
#include "fcp/secagg/client/state_transition_listener_interface.h"
#include "fcp/secagg/shared/aes_prng_factory.h"
#include "fcp/secagg/shared/async_abort.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/prng.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/secagg/shared/secagg_vector.h"

namespace fcp {
namespace secagg {

SecAggClient::SecAggClient(
    int max_neighbors_expected,
    int minimum_surviving_neighbors_for_reconstruction,
    std::vector<InputVectorSpecification> input_vector_specs,
    std::unique_ptr<SecurePrng> prng,
    std::unique_ptr<SendToServerInterface> sender,
    std::unique_ptr<StateTransitionListenerInterface> transition_listener,
    std::unique_ptr<AesPrngFactory> prng_factory,
    std::atomic<std::string*>* abort_signal_for_test)
    : mu_(),
      abort_signal_(nullptr),
      async_abort_(abort_signal_for_test ? abort_signal_for_test
                                         : &abort_signal_),
      state_(std::make_unique<SecAggClientR0AdvertiseKeysInputNotSetState>(
          max_neighbors_expected,
          minimum_surviving_neighbors_for_reconstruction,
          std::make_unique<std::vector<InputVectorSpecification> >(
              std::move(input_vector_specs)),
          std::move(prng), std::move(sender), std::move(transition_listener),
          std::move(prng_factory), &async_abort_)) {}

Status SecAggClient::Start() {
  absl::WriterMutexLock _(&mu_);
  auto state_or_error = state_->Start();
  if (state_or_error.ok()) {
    state_ = std::move(state_or_error.value());
  }
  return state_or_error.status();
}

Status SecAggClient::Abort() { return Abort("unknown reason"); }

Status SecAggClient::Abort(const std::string& reason) {
  async_abort_.Abort(reason);
  absl::WriterMutexLock _(&mu_);
  if (state_->IsAborted() || state_->IsCompletedSuccessfully())
    return FCP_STATUS(OK);

  auto state_or_error = state_->Abort(reason);
  if (state_or_error.ok()) {
    state_ = std::move(state_or_error.value());
  }
  return state_or_error.status();
}

Status SecAggClient::SetInput(std::unique_ptr<SecAggVectorMap> input_map) {
  absl::WriterMutexLock _(&mu_);
  auto state_or_error = state_->SetInput(std::move(input_map));
  if (state_or_error.ok()) {
    state_ = std::move(state_or_error.value());
  }
  return state_or_error.status();
}

StatusOr<bool> SecAggClient::ReceiveMessage(
    const ServerToClientWrapperMessage& incoming) {
  absl::WriterMutexLock _(&mu_);
  auto state_or_error = state_->HandleMessage(incoming);
  if (state_or_error.ok()) {
    state_ = std::move(state_or_error.value());
    // Return true iff neither aborted nor completed.
    return !(state_->IsAborted() || state_->IsCompletedSuccessfully());
  } else {
    return state_or_error.status();
  }
}

StatusOr<std::string> SecAggClient::ErrorMessage() const {
  absl::ReaderMutexLock _(&mu_);
  return state_->ErrorMessage();
}

bool SecAggClient::IsAborted() const {
  absl::ReaderMutexLock _(&mu_);
  return state_->IsAborted();
}

bool SecAggClient::IsCompletedSuccessfully() const {
  absl::ReaderMutexLock _(&mu_);
  return state_->IsCompletedSuccessfully();
}

std::string SecAggClient::State() const {
  absl::ReaderMutexLock _(&mu_);
  return state_->StateName();
}

}  // namespace secagg
}  // namespace fcp
