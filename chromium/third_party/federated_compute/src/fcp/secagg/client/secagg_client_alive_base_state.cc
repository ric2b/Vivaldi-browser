/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, softwar
 * e
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fcp/secagg/client/secagg_client_alive_base_state.h"

#include <string>
#include <utility>

#include "fcp/secagg/client/secagg_client_aborted_state.h"
#include "fcp/secagg/client/secagg_client_state.h"
#include "fcp/secagg/client/send_to_server_interface.h"
#include "fcp/secagg/client/state_transition_listener_interface.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"

namespace fcp {
namespace secagg {

SecAggClientAliveBaseState::SecAggClientAliveBaseState(
    std::unique_ptr<SendToServerInterface> sender,
    std::unique_ptr<StateTransitionListenerInterface> transition_listener,
    ClientState state, AsyncAbort* async_abort)
    : SecAggClientState(std::move(sender), std::move(transition_listener),
                        state),
      async_abort_(async_abort) {}

StatusOr<std::unique_ptr<SecAggClientState> > SecAggClientAliveBaseState::Abort(
    const std::string& reason) {
  return AbortAndNotifyServer("Abort upon external request for reason <" +
                              reason + ">.");
}

std::unique_ptr<SecAggClientState>
SecAggClientAliveBaseState::AbortAndNotifyServer(const std::string& reason) {
  ClientToServerWrapperMessage message_to_server;
  message_to_server.mutable_abort()->set_diagnostic_info(reason);
  sender_->Send(&message_to_server);
  return std::make_unique<SecAggClientAbortedState>(
      reason, std::move(sender_), std::move(transition_listener_));
}
}  // namespace secagg
}  // namespace fcp
