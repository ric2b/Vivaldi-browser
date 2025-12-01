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

#ifndef FCP_SECAGG_CLIENT_SECAGG_CLIENT_STATE_H_
#define FCP_SECAGG_CLIENT_SECAGG_CLIENT_STATE_H_

#include <memory>
#include <string>
#include <vector>

#include "fcp/base/monitoring.h"
#include "fcp/secagg/client/send_to_server_interface.h"
#include "fcp/secagg/client/state_transition_listener_interface.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/secagg/shared/secagg_vector.h"

namespace fcp {
namespace secagg {

// This is an abstract class which is the parent of the other SecAggClient*State
// classes. It should not be instantiated directly. Default versions of all the
// methods declared here are provided for use by states which do not expect, and
// therefore do not implement, those methods.

class SecAggClientState {
 public:
  // Initiates the protocol by computing its first message and sending it to the
  // server. If called in a valid state, returns the new State. Otherwise,
  // returns an error Status with code PRECONDITION_FAILED.
  virtual StatusOr<std::unique_ptr<SecAggClientState> > Start();

  // Handles the received message in a way consistent with the current state.
  // If called from a state expecting a message, returns the new State. If the
  // message was of the right type but had invalid contents, the new State will
  // be a SecAggClientAbortState.
  // If the state was not expecting a message of this type at all, returns an
  // error Status with code PRECONDITION_FAILED.
  virtual StatusOr<std::unique_ptr<SecAggClientState> > HandleMessage(
      const ServerToClientWrapperMessage& message);

  // Sets the input of this client for this protocol session. If successful,
  // returns the new state. If the input does not match the specification,
  // returns an error Status with code INVALID_ARGUMENT.
  // If the client's state was not ready for an input to be set, returns an
  // error Status with code PRECONDITION_FAILED.
  virtual StatusOr<std::unique_ptr<SecAggClientState> > SetInput(
      std::unique_ptr<SecAggVectorMap> input_map);

  // Aborts the protocol for the specified reason. Returns the new state. If the
  // protocol was already aborted or completed, instead returns an error Status
  // with code PRECONDITION_FAILED.
  virtual StatusOr<std::unique_ptr<SecAggClientState> > Abort(
      const std::string& reason);

  // Returns true if the current state is Abort, false else.
  ABSL_MUST_USE_RESULT virtual bool IsAborted() const;

  // Returns true if the current state is ProtocolCompleted, false else.
  ABSL_MUST_USE_RESULT virtual bool IsCompletedSuccessfully() const;

  // Returns the error message, if the current state is an abort state. If not,
  // returns an error Status with code PRECONDITION_FAILED.
  ABSL_MUST_USE_RESULT virtual StatusOr<std::string> ErrorMessage() const;

  // Returns the name of the current state, as a string.
  ABSL_MUST_USE_RESULT virtual std::string StateName() const = 0;

  virtual ~SecAggClientState() = default;

 protected:
  // The object that sends messages to the server.
  std::unique_ptr<SendToServerInterface> sender_;
  // A listener for state transitions.
  std::unique_ptr<StateTransitionListenerInterface> transition_listener_;
  // State type.
  ClientState state_;

  // SecAggClientState should never be instantiated directly.
  SecAggClientState(
      std::unique_ptr<SendToServerInterface> sender,
      std::unique_ptr<StateTransitionListenerInterface> transition_listener,
      ClientState state);

  // Validates an input map by returning true if all SecAggVectors match their
  // corresponding InputVectorSpecifications, and false otherwise.
  bool ValidateInput(
      const SecAggVectorMap& input_map,
      const std::vector<InputVectorSpecification>& input_vector_specs);
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_CLIENT_SECAGG_CLIENT_STATE_H_
