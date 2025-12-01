/*
 * Copyright 2020 Google LLC
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

#ifndef FCP_SECAGG_CLIENT_STATE_TRANSITION_LISTENER_INTERFACE_H_
#define FCP_SECAGG_CLIENT_STATE_TRANSITION_LISTENER_INTERFACE_H_

#include <cstdint>

namespace fcp {
namespace secagg {

enum class ClientState : int {
  INITIAL = 0,
  R0_ADVERTISE_KEYS = 1,
  R1_SHARE_KEYS = 2,
  R2_MASKED_INPUT = 3,
  R3_UNMASKING = 4,
  COMPLETED = 5,
  ABORTED = 6,
};

// Listens for state transition messages.
//
// The expected call pattern in the successful case is the following:
// - Transition(R0_ADVERTISE_KEYS)
//   - Started(R0_ADVERTISE_KEYS)
//   - Stopped(R0_ADVERTISE_KEYS)
// - Transition(R1_SHARE_KEYS)
//   - Started(R1_SHARE_KEYS)
//   - Stopped(R1_SHARE_KEYS)
// - Transition(R2_MASKED_INPUT)
// ...
// - Transition(COMPLETED)
//
// It is also possible to have more than one pair of Started and Stopped calls
// for any given state.
//
// If the protocol gets aborted at any point, Transition(ABORTED) would be
// called and any remaining Started and Stopped calls would be skipped.
class StateTransitionListenerInterface {
 public:
  // Called on transition to a new state.
  virtual void Transition(ClientState new_state) = 0;
  // Called just before a state starts computation, excluding any idle or
  // waiting time.
  virtual void Started(ClientState state) = 0;
  // Called just after a state stops computation, excluding any idle or
  // waiting time, or sending messages to to the server.
  virtual void Stopped(ClientState state) = 0;
  virtual void set_execution_session_id(int64_t execution_session_id) = 0;

  virtual ~StateTransitionListenerInterface() = default;
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_CLIENT_STATE_TRANSITION_LISTENER_INTERFACE_H_
