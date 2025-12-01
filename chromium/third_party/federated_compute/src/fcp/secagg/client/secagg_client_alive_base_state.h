/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FCP_SECAGG_CLIENT_SECAGG_CLIENT_ALIVE_BASE_STATE_H_
#define FCP_SECAGG_CLIENT_SECAGG_CLIENT_ALIVE_BASE_STATE_H_

#include <memory>
#include <string>

#include "fcp/secagg/client/secagg_client_state.h"
#include "fcp/secagg/client/send_to_server_interface.h"
#include "fcp/secagg/client/state_transition_listener_interface.h"
#include "fcp/secagg/shared/async_abort.h"

namespace fcp {
namespace secagg {

// Abstract base class containing code used by all SecAggClientStates where the
// client is still alive and online, i.e. non-terminal states.

class SecAggClientAliveBaseState : public SecAggClientState {
 public:
  ~SecAggClientAliveBaseState() override = default;

  StatusOr<std::unique_ptr<SecAggClientState> > Abort(
      const std::string& reason) override;

 protected:
  // SecAggClientAliveBaseState should never be instantiated directly.
  explicit SecAggClientAliveBaseState(
      std::unique_ptr<SendToServerInterface> sender,
      std::unique_ptr<StateTransitionListenerInterface> transition_listener,
      ClientState state, AsyncAbort* async_abort = nullptr);

  // Method to be used internally by child SecAggClient*State classes, called
  // when an abort is required by the protocol. Sends an abort message to the
  // server, then constructs and returns an abort state.
  std::unique_ptr<SecAggClientState> AbortAndNotifyServer(
      const std::string& reason);

  AsyncAbort* async_abort_;  // Owned by state owner.
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_CLIENT_SECAGG_CLIENT_ALIVE_BASE_STATE_H_
