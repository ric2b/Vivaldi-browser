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

#include "fcp/secagg/client/secagg_client_completed_state.h"

#include <string>
#include <unordered_map>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/node_hash_map.h"
#include "fcp/base/monitoring.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/secagg/shared/secagg_vector.h"
#include "fcp/secagg/testing/mock_send_to_server_interface.h"
#include "fcp/secagg/testing/mock_state_transition_listener.h"

namespace fcp {
namespace secagg {
namespace {

using ::testing::Eq;

TEST(SecAggClientCompletedStateTest, IsAbortedReturnsFalse) {
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientCompletedState completed_state(
      std::unique_ptr<SendToServerInterface>{sender},
      std::unique_ptr<MockStateTransitionListener>{transition_listener});
  EXPECT_THAT(completed_state.IsAborted(), Eq(false));
}

TEST(SecAggClientCompletedStateTest, IsCompletedSuccessfullyReturnsTrue) {
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientCompletedState completed_state(
      std::unique_ptr<SendToServerInterface>{sender},
      std::unique_ptr<MockStateTransitionListener>{transition_listener});
  EXPECT_THAT(completed_state.IsCompletedSuccessfully(), Eq(true));
}

TEST(SecAggClientCompletedStateTest, ErrorMessageRaisesErrorStatus) {
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientCompletedState completed_state(
      std::unique_ptr<SendToServerInterface>{sender},
      std::unique_ptr<MockStateTransitionListener>{transition_listener});
  EXPECT_THAT(completed_state.ErrorMessage().ok(), Eq(false));
}

TEST(SecAggClientCompletedStateTest, StartRaisesErrorStatus) {
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientCompletedState completed_state(
      std::unique_ptr<SendToServerInterface>{sender},
      std::unique_ptr<MockStateTransitionListener>{transition_listener});
  EXPECT_THAT(completed_state.Start().ok(), Eq(false));
}

TEST(SecAggClientCompletedStateTest, HandleMessageRaisesErrorStatus) {
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientCompletedState completed_state(
      std::unique_ptr<SendToServerInterface>{sender},
      std::unique_ptr<MockStateTransitionListener>{transition_listener});
  EXPECT_THAT(
      completed_state
          .HandleMessage(ServerToClientWrapperMessage::default_instance())
          .ok(),
      Eq(false));
}

TEST(SecAggClientCompletedStateTest, SetInputRaisesErrorStatus) {
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientCompletedState completed_state(
      std::unique_ptr<SendToServerInterface>{sender},
      std::unique_ptr<MockStateTransitionListener>{transition_listener});
  EXPECT_THAT(
      completed_state.SetInput(std::make_unique<SecAggVectorMap>()).ok(),
      Eq(false));
}

TEST(SecAggClientCompletedStateTest, AbortRaisesErrorStatus) {
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientCompletedState completed_state(
      std::unique_ptr<SendToServerInterface>{sender},
      std::unique_ptr<MockStateTransitionListener>{transition_listener});
  EXPECT_THAT(completed_state.Abort("incorrect abort").ok(), Eq(false));
}

}  // namespace
}  // namespace secagg
}  // namespace fcp
