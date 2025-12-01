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

#include "fcp/secagg/client/secagg_client_state.h"

#include <string>
#include <utility>

#include "fcp/base/monitoring.h"
#include "fcp/secagg/client/state_transition_listener_interface.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/secagg/shared/secagg_vector.h"

// The methods implemented here should be overridden by state classes from
// which these transitions are valid, and inherited by state classes from which
// they are invalid. For example, only round 0 classes should override the Start
// method.
//
// Classes that return booleans should only be overridden by state classes for
// which they will return true.

namespace fcp {
namespace secagg {

SecAggClientState::SecAggClientState(
    std::unique_ptr<SendToServerInterface> sender,
    std::unique_ptr<StateTransitionListenerInterface> transition_listener,
    ClientState state)
    : sender_(std::move(sender)),
      transition_listener_(std::move(transition_listener)),
      state_(state) {
  transition_listener_->Transition(state_);
}

StatusOr<std::unique_ptr<SecAggClientState> > SecAggClientState::Start() {
  return FCP_STATUS(FAILED_PRECONDITION)
         << "An illegal start transition was attempted from state "
         << StateName();
}

StatusOr<std::unique_ptr<SecAggClientState> > SecAggClientState::HandleMessage(
    const ServerToClientWrapperMessage& message) {
  if (message.message_content_case() ==
      ServerToClientWrapperMessage::MESSAGE_CONTENT_NOT_SET) {
    return FCP_STATUS(FAILED_PRECONDITION)
           << "Client received a message of unknown type but was in state "
           << StateName();
  } else {
    return FCP_STATUS(FAILED_PRECONDITION)
           << "Client received a message of type "
           << message.message_content_case() << " but was in state "
           << StateName();
  }
}

StatusOr<std::unique_ptr<SecAggClientState> > SecAggClientState::SetInput(
    std::unique_ptr<SecAggVectorMap> input_map) {
  return FCP_STATUS(FAILED_PRECONDITION)
         << "An illegal input transition was attempted from state "
         << StateName();
}

StatusOr<std::unique_ptr<SecAggClientState> > SecAggClientState::Abort(
    const std::string& reason) {
  return FCP_STATUS(FAILED_PRECONDITION)
         << "The client was already in terminal state " << StateName()
         << " but received an abort with message: " << reason;
}

bool SecAggClientState::IsAborted() const { return false; }

bool SecAggClientState::IsCompletedSuccessfully() const { return false; }

StatusOr<std::string> SecAggClientState::ErrorMessage() const {
  return FCP_STATUS(FAILED_PRECONDITION)
         << "Error message requested, but client is in state " << StateName();
}

bool SecAggClientState::ValidateInput(
    const SecAggVectorMap& input_map,
    const std::vector<InputVectorSpecification>& input_vector_specs) {
  if (input_map.size() != input_vector_specs.size()) {
    return false;
  }
  for (const auto& vector_spec : input_vector_specs) {
    auto input_vec = input_map.find(vector_spec.name());
    if (input_vec == input_map.end() ||
        input_vec->second.modulus() != vector_spec.modulus() ||
        input_vec->second.num_elements() != vector_spec.length()) {
      return false;
    }
  }

  return true;
}

}  // namespace secagg
}  // namespace fcp
