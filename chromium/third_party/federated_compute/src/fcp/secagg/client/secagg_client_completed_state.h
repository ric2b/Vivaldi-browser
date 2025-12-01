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

#ifndef FCP_SECAGG_CLIENT_SECAGG_CLIENT_COMPLETED_STATE_H_
#define FCP_SECAGG_CLIENT_SECAGG_CLIENT_COMPLETED_STATE_H_

#include <memory>
#include <string>

#include "fcp/secagg/client/secagg_client_state.h"
#include "fcp/secagg/client/state_transition_listener_interface.h"

namespace fcp {
namespace secagg {

// This class represents the Completed state for a client, meaning the
// client has either sent its final message or received a message indicating
// success from the server.
//
// There are no transitions out of this state.

class SecAggClientCompletedState : public SecAggClientState {
 public:
  // As a terminal state, this State does not need to store any specific
  // information except the sender (to ensure it does not go out of scope
  // unexpectedly).
  explicit SecAggClientCompletedState(
      std::unique_ptr<SendToServerInterface> sender,
      std::unique_ptr<StateTransitionListenerInterface> transition_listener);

  ~SecAggClientCompletedState() override;

  // Returns true from this state.
  bool IsCompletedSuccessfully() const override;

  // Returns the name of this state, "COMPLETED".
  std::string StateName() const override;
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_CLIENT_SECAGG_CLIENT_COMPLETED_STATE_H_
