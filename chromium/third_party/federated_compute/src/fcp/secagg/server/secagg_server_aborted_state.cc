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

#include "fcp/secagg/server/secagg_server_aborted_state.h"

#include <memory>
#include <string>
#include <utility>

namespace fcp {
namespace secagg {

SecAggServerAbortedState::SecAggServerAbortedState(
    const std::string& error_message,
    std::unique_ptr<SecAggServerProtocolImpl> impl,
    int number_of_clients_failed_after_sending_masked_input,
    int number_of_clients_failed_before_sending_masked_input,
    int number_of_clients_terminated_without_unmasking)
    : SecAggServerState(number_of_clients_failed_after_sending_masked_input,
                        number_of_clients_failed_before_sending_masked_input,
                        number_of_clients_terminated_without_unmasking,
                        SecAggServerStateKind::ABORTED, std::move(impl)),
      error_message_(error_message) {}

SecAggServerAbortedState::~SecAggServerAbortedState() = default;

bool SecAggServerAbortedState::IsAborted() const { return true; }

StatusOr<std::string> SecAggServerAbortedState::ErrorMessage() const {
  return error_message_;
}

bool SecAggServerAbortedState::IsNumberOfIncludedInputsCommitted() const {
  return true;
}

}  // namespace secagg
}  // namespace fcp
