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

#include "fcp/secagg/server/secagg_server_completed_state.h"

#include <memory>
#include <utility>

#include "fcp/base/monitoring.h"
#include "fcp/tracing/tracing_span.h"

namespace fcp {
namespace secagg {

SecAggServerCompletedState::SecAggServerCompletedState(
    std::unique_ptr<SecAggServerProtocolImpl> impl,
    int number_of_clients_failed_after_sending_masked_input,
    int number_of_clients_failed_before_sending_masked_input,
    int number_of_clients_terminated_without_unmasking)
    : SecAggServerState(number_of_clients_failed_after_sending_masked_input,
                        number_of_clients_failed_before_sending_masked_input,
                        number_of_clients_terminated_without_unmasking,
                        SecAggServerStateKind::COMPLETED, std::move(impl)) {
  // Moving to this state means the protocol succeeded!
  if (metrics()) {
    metrics()->ProtocolOutcomes(SecAggServerOutcome::SUCCESS);
  }
  Trace<SecAggProtocolOutcome>(TracingSecAggServerOutcome_Success);
}

SecAggServerCompletedState::~SecAggServerCompletedState() = default;

bool SecAggServerCompletedState::IsCompletedSuccessfully() const {
  return true;
}

int SecAggServerCompletedState::NumberOfIncludedInputs() const {
  return total_number_of_clients() -
         number_of_clients_failed_before_sending_masked_input_;
}

bool SecAggServerCompletedState::IsNumberOfIncludedInputsCommitted() const {
  return true;
}

StatusOr<std::unique_ptr<SecAggVectorMap> >
SecAggServerCompletedState::Result() {
  auto result = impl()->TakeResult();
  if (!result) {
    return FCP_STATUS(UNAVAILABLE)
           << "Result is uninitialized or requested more than once";
  }
  return std::move(result);
}

}  // namespace secagg
}  // namespace fcp
