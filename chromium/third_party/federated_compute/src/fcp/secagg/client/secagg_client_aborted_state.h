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

#ifndef FCP_SECAGG_CLIENT_SECAGG_CLIENT_ABORTED_STATE_H_
#define FCP_SECAGG_CLIENT_SECAGG_CLIENT_ABORTED_STATE_H_

#include <memory>
#include <string>

#include "fcp/base/monitoring.h"
#include "fcp/secagg/client/secagg_client_state.h"
#include "fcp/secagg/client/state_transition_listener_interface.h"

namespace fcp {
namespace secagg {

// This class represents the abort state for a client. There are no transitions
// out of this state. A new SecAggClient object will be needed to start a new
// run of the protocol.

class SecAggClientAbortedState : public SecAggClientState {
 public:
  SecAggClientAbortedState(
      const std::string& reason, std::unique_ptr<SendToServerInterface> sender,
      std::unique_ptr<StateTransitionListenerInterface> transition_listener);

  ~SecAggClientAbortedState() override;

  // Returns true from this state.
  bool IsAborted() const override;

  // Returns the error message with which the client aborted.
  StatusOr<std::string> ErrorMessage() const override;

  // Returns the name of this state, "ABORTED".
  std::string StateName() const override;

 private:
  const std::string reason_;
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_CLIENT_SECAGG_CLIENT_ABORTED_STATE_H_
