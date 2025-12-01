/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FCP_SECAGG_TESTING_MOCK_STATE_TRANSITION_LISTENER_H_
#define FCP_SECAGG_TESTING_MOCK_STATE_TRANSITION_LISTENER_H_

#include "gmock/gmock.h"
#include "fcp/secagg/client/state_transition_listener_interface.h"

namespace fcp {
namespace secagg {

// GMock Implementation of SendToServerInterface.

class MockStateTransitionListener : public StateTransitionListenerInterface {
 public:
  MOCK_METHOD(void, Transition, (ClientState state));
  MOCK_METHOD(void, Started, (ClientState state));
  MOCK_METHOD(void, Stopped, (ClientState state));
  MOCK_METHOD(void, set_execution_session_id, (int64_t execution_session_id));
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_TESTING_MOCK_STATE_TRANSITION_LISTENER_H_
