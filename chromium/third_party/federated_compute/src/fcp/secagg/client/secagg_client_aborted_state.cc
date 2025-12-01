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

#include "fcp/secagg/client/secagg_client_aborted_state.h"

#include <memory>
#include <string>
#include <utility>

#include "fcp/base/monitoring.h"
#include "fcp/secagg/client/secagg_client_state.h"
#include "fcp/secagg/client/state_transition_listener_interface.h"

namespace fcp {
namespace secagg {

SecAggClientAbortedState::SecAggClientAbortedState(
    const std::string& reason, std::unique_ptr<SendToServerInterface> sender,
    std::unique_ptr<StateTransitionListenerInterface> transition_listener)
    : SecAggClientState(std::move(sender), std::move(transition_listener),
                        ClientState::ABORTED),
      reason_(reason) {
  FCP_LOG(WARNING) << "Aborting for reason: " << reason_;
}

SecAggClientAbortedState::~SecAggClientAbortedState() = default;

bool SecAggClientAbortedState::IsAborted() const { return true; }

StatusOr<std::string> SecAggClientAbortedState::ErrorMessage() const {
  return reason_;
}

std::string SecAggClientAbortedState::StateName() const { return "ABORTED"; }

}  // namespace secagg
}  // namespace fcp
