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

#include "fcp/secagg/client/secagg_client_r3_unmasking_state.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "fcp/base/monitoring.h"
#include "fcp/secagg/client/secagg_client_aborted_state.h"
#include "fcp/secagg/client/secagg_client_completed_state.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/secagg/shared/secagg_vector.h"
#include "fcp/secagg/shared/shamir_secret_sharing.h"
#include "fcp/secagg/testing/mock_send_to_server_interface.h"
#include "fcp/secagg/testing/mock_state_transition_listener.h"
#include "fcp/testing/testing.h"

namespace fcp {
namespace secagg {
namespace {

using ::testing::Eq;
using ::testing::Pointee;
using ::testing::StrEq;

static const ShamirShare test_pairwise_share = {"test pairwise share"};
static const ShamirShare test_self_share = {"test self share"};

TEST(SecAggClientR3UnmaskingStateTest, IsAbortedReturnsFalse) {
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR3UnmaskingState r3_state(
      0, 4, 4, 4, std::make_unique<std::vector<OtherClientState> >(4),
      std::make_unique<std::vector<ShamirShare> >(4),
      std::make_unique<std::vector<ShamirShare> >(4),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener));
  EXPECT_THAT(r3_state.IsAborted(), Eq(false));
}

TEST(SecAggClientR3UnmaskingStateTest, IsCompletedSuccessfullyReturnsFalse) {
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR3UnmaskingState r3_state(
      0, 4, 4, 4, std::make_unique<std::vector<OtherClientState> >(4),
      std::make_unique<std::vector<ShamirShare> >(4),
      std::make_unique<std::vector<ShamirShare> >(4),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener));
  EXPECT_THAT(r3_state.IsCompletedSuccessfully(), Eq(false));
}

TEST(SecAggClientR3UnmaskingStateTest, StartRaisesErrorStatus) {
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR3UnmaskingState r3_state(
      0, 4, 4, 4, std::make_unique<std::vector<OtherClientState> >(4),
      std::make_unique<std::vector<ShamirShare> >(4),
      std::make_unique<std::vector<ShamirShare> >(4),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener));
  EXPECT_THAT(r3_state.Start().ok(), Eq(false));
}

TEST(SecAggClientR3UnmaskingStateTest, SetInputRaisesErrorStatus) {
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR3UnmaskingState r3_state(
      0, 4, 4, 4, std::make_unique<std::vector<OtherClientState> >(4),
      std::make_unique<std::vector<ShamirShare> >(4),
      std::make_unique<std::vector<ShamirShare> >(4),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener));
  EXPECT_THAT(r3_state.SetInput(std::make_unique<SecAggVectorMap>()).ok(),
              Eq(false));
}

TEST(SecAggClientR3UnmaskingStateTest, ErrorMessageRaisesErrorStatus) {
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR3UnmaskingState r3_state(
      0, 4, 4, 4, std::make_unique<std::vector<OtherClientState> >(4),
      std::make_unique<std::vector<ShamirShare> >(4),
      std::make_unique<std::vector<ShamirShare> >(4),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener));
  EXPECT_THAT(r3_state.ErrorMessage().ok(), Eq(false));
}

TEST(SecAggClientR3UnmaskingStateTest,
     AbortReturnsValidAbortStateAndNotifiesServer) {
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();

  std::string error_string =
      "Abort upon external request for reason <Abort reason>.";

  ClientToServerWrapperMessage expected_message;
  expected_message.mutable_abort()->set_diagnostic_info(error_string);
  EXPECT_CALL(*sender, Send(Pointee(EqualsProto(expected_message))));

  SecAggClientR3UnmaskingState r3_state(
      0, 4, 4, 4, std::make_unique<std::vector<OtherClientState> >(4),
      std::make_unique<std::vector<ShamirShare> >(4),
      std::make_unique<std::vector<ShamirShare> >(4),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener));
  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r3_state.Abort("Abort reason");
  ASSERT_THAT(new_state.ok(), Eq(true));
  EXPECT_THAT(new_state.value()->StateName(), StrEq("ABORTED"));
  EXPECT_THAT(new_state.value()->ErrorMessage().value(), StrEq(error_string));
}

TEST(SecAggClientR3UnmaskingStateTest,
     AbortFailureMessageCausesAbortWithoutNotifyingServer) {
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR3UnmaskingState r3_state(
      1,  // client_id
      6,  // number_of_alive_neighbors
      4,  // minimum_surviving_neighbors_for_reconstruction
      6,  // number_of_neighbors
      std::make_unique<std::vector<OtherClientState> >(
          6, OtherClientState::kAlive),
      std::make_unique<std::vector<ShamirShare> >(6, test_pairwise_share),
      std::make_unique<std::vector<ShamirShare> >(6, test_self_share),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener));

  EXPECT_CALL(*sender, Send(::testing::_)).Times(0);
  ServerToClientWrapperMessage abort_message;
  abort_message.mutable_abort()->set_early_success(false);

  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r3_state.HandleMessage(abort_message);
  ASSERT_TRUE(new_state.ok());
  EXPECT_THAT(new_state.value()->StateName(), StrEq("ABORTED"));
  EXPECT_THAT(new_state.value()->ErrorMessage().value(),
              StrEq("Aborting because of abort message from the server."));
}

TEST(SecAggClientR3UnmaskingStateTest,
     EarlySuccessMessageCausesTransitionToCompletedState) {
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR3UnmaskingState r3_state(
      1,  // client_id
      6,  // number_of_alive_neighbors
      4,  // minimum_surviving_neighbors_for_reconstruction
      6,  // number_of_neighbors
      std::make_unique<std::vector<OtherClientState> >(
          6, OtherClientState::kAlive),
      std::make_unique<std::vector<ShamirShare> >(6, test_pairwise_share),
      std::make_unique<std::vector<ShamirShare> >(6, test_self_share),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener));

  EXPECT_CALL(*sender, Send(::testing::_)).Times(0);
  ServerToClientWrapperMessage abort_message;
  abort_message.mutable_abort()->set_early_success(true);

  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r3_state.HandleMessage(abort_message);
  ASSERT_TRUE(new_state.ok());
  EXPECT_THAT(new_state.value()->StateName(), StrEq("COMPLETED"));
}

TEST(SecAggClientR3UnmaskingStateTest,
     UnmaskingRequestIsCorrectlyHandledWhenNoClientsDie) {
  // In this test, this is client id 1. There are 6 clients, and none of them
  // drop out.
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR3UnmaskingState r3_state(
      1,  // client_id
      6,  // number_of_alive_neighbors
      4,  // minimum_surviving_neighbors_for_reconstruction
      6,  // number_of_neighbors
      std::make_unique<std::vector<OtherClientState> >(
          6, OtherClientState::kAlive),
      std::make_unique<std::vector<ShamirShare> >(6, test_pairwise_share),
      std::make_unique<std::vector<ShamirShare> >(6, test_self_share),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener));

  ClientToServerWrapperMessage expected_message;
  for (int i = 0; i < 6; i++) {
    expected_message.mutable_unmasking_response()
        ->add_noise_or_prf_key_shares()
        ->set_prf_sk_share("test self share");
  }
  EXPECT_CALL(*sender, Send(Pointee(EqualsProto(expected_message))));

  ServerToClientWrapperMessage request;
  request.mutable_unmasking_request();
  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r3_state.HandleMessage(request);
  ASSERT_TRUE(new_state.ok());
  EXPECT_THAT(new_state.value()->StateName(), StrEq("COMPLETED"));
}

TEST(SecAggClientR3UnmaskingStateTest,
     UnmaskingRequestIsCorrectlyHandledWhenFewClientsDie) {
  // In this test, this is client id 1. Client 3 already died at round 2, and
  // client 5 dies in round 3.
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  std::vector<OtherClientState> other_clients_states{
      OtherClientState::kAlive, OtherClientState::kAlive,
      OtherClientState::kAlive, OtherClientState::kDeadAtRound2,
      OtherClientState::kAlive, OtherClientState::kAlive};

  SecAggClientR3UnmaskingState r3_state(
      1,  // client_id
      5,  // number_of_alive_neighbors
      4,  // minimum_surviving_neighbors_for_reconstruction
      6,  // number_of_neighbors
      std::make_unique<std::vector<OtherClientState> >(other_clients_states),
      std::make_unique<std::vector<ShamirShare> >(6, test_pairwise_share),
      std::make_unique<std::vector<ShamirShare> >(6, test_self_share),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener));

  ClientToServerWrapperMessage expected_message;
  for (int i = 0; i < 6; i++) {
    if (i == 3) {
      expected_message.mutable_unmasking_response()
          ->add_noise_or_prf_key_shares();
    } else if (i == 5) {
      expected_message.mutable_unmasking_response()
          ->add_noise_or_prf_key_shares()
          ->set_noise_sk_share("test pairwise share");
    } else {
      expected_message.mutable_unmasking_response()
          ->add_noise_or_prf_key_shares()
          ->set_prf_sk_share("test self share");
    }
  }
  EXPECT_CALL(*sender, Send(Pointee(EqualsProto(expected_message))));

  ServerToClientWrapperMessage request;
  // TODO(team): 6 -> 5 below, once backwards compatibility not needed.
  request.mutable_unmasking_request()->add_dead_3_client_ids(6);
  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r3_state.HandleMessage(request);
  ASSERT_TRUE(new_state.ok());
  EXPECT_THAT(new_state.value()->StateName(), StrEq("COMPLETED"));
}

TEST(SecAggClientR3UnmaskingStateTest,
     UnmaskingRequestCausesAbortWhenTooManyClientsDie) {
  // In this test, this is client id 1. Client 3 already died at round 2, and
  // clients 4 and 5 die in round 3. This should cause a transition to an abort
  // state and an abort message to be sent to the server.
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  std::vector<OtherClientState> other_clients_states{
      OtherClientState::kAlive, OtherClientState::kAlive,
      OtherClientState::kAlive, OtherClientState::kDeadAtRound2,
      OtherClientState::kAlive, OtherClientState::kAlive};

  SecAggClientR3UnmaskingState r3_state(
      1,  // client_id
      5,  // number_of_alive_neighbors
      4,  // minimum_surviving_neighbors_for_reconstruction
      6,  // number_of_neighbors
      std::make_unique<std::vector<OtherClientState> >(other_clients_states),
      std::make_unique<std::vector<ShamirShare> >(6, test_pairwise_share),
      std::make_unique<std::vector<ShamirShare> >(6, test_self_share),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener));

  std::string error_string =
      "Not enough clients survived. The server should not have sent this "
      "UnmaskingRequest.";
  ClientToServerWrapperMessage expected_message;
  expected_message.mutable_abort()->set_diagnostic_info(error_string);
  EXPECT_CALL(*sender, Send(Pointee(EqualsProto(expected_message))));

  ServerToClientWrapperMessage request;
  // TODO(team): 5 -> 4 below, once backwards compatibility not needed.
  request.mutable_unmasking_request()->add_dead_3_client_ids(5);
  // TODO(team): 6 -> 5 below, once backwards compatibility not needed.
  request.mutable_unmasking_request()->add_dead_3_client_ids(6);
  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r3_state.HandleMessage(request);
  ASSERT_TRUE(new_state.ok());
  EXPECT_THAT(new_state.value()->StateName(), StrEq("ABORTED"));
  EXPECT_THAT(new_state.value()->ErrorMessage().value(), StrEq(error_string));
}

TEST(SecAggClientR3UnmaskingStateTest,
     UnmaskingRequestCausesAbortIfServerListsThisClientAsDead) {
  // In this test, this is client id 1, but the server lists client 1 as dead.
  // This should cause a transition to an abort state and an abort message to be
  // sent to the server.
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();

  SecAggClientR3UnmaskingState r3_state(
      1,  // client_id
      6,  // number_of_alive_neighbors
      4,  // minimum_surviving_neighbors_for_reconstruction
      6,  // number_of_neighbors
      std::make_unique<std::vector<OtherClientState> >(
          6, OtherClientState::kAlive),
      std::make_unique<std::vector<ShamirShare> >(6, test_pairwise_share),
      std::make_unique<std::vector<ShamirShare> >(6, test_self_share),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener));

  std::string error_string =
      "The received UnmaskingRequest states this client has aborted, but this "
      "client had not yet aborted.";
  ClientToServerWrapperMessage expected_message;
  expected_message.mutable_abort()->set_diagnostic_info(error_string);
  EXPECT_CALL(*sender, Send(Pointee(EqualsProto(expected_message))));

  ServerToClientWrapperMessage request;
  // TODO(team): 2 -> 1 below, once backwards compatibility not needed.
  request.mutable_unmasking_request()->add_dead_3_client_ids(2);
  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r3_state.HandleMessage(request);
  ASSERT_TRUE(new_state.ok());
  EXPECT_THAT(new_state.value()->StateName(), StrEq("ABORTED"));
  EXPECT_THAT(new_state.value()->ErrorMessage().value(), StrEq(error_string));
}

TEST(SecAggClientR3UnmaskingStateTest,
     UnmaskingRequestCausesAbortIfServerListsNonexistentClientAsDead) {
  // In this test, there are 6 clients (labeled 0-5), but the server lists
  // client 6 as dead. This should cause a transition to an abort state and an
  // abort message to be sent to the server.
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();

  SecAggClientR3UnmaskingState r3_state(
      1,  // client_id
      6,  // number_of_alive_neighbors
      4,  // minimum_surviving_neighbors_for_reconstruction
      6,  // number_of_neighbors
      std::make_unique<std::vector<OtherClientState> >(
          6, OtherClientState::kAlive),
      std::make_unique<std::vector<ShamirShare> >(6, test_pairwise_share),
      std::make_unique<std::vector<ShamirShare> >(6, test_self_share),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener));

  std::string error_string =
      "The received UnmaskingRequest contains a client id that does not "
      "correspond to any client.";
  ClientToServerWrapperMessage expected_message;
  expected_message.mutable_abort()->set_diagnostic_info(error_string);
  EXPECT_CALL(*sender, Send(Pointee(EqualsProto(expected_message))));

  ServerToClientWrapperMessage request;
  // TODO(team): 7 -> 6 below, once backwards compatibility not needed.
  request.mutable_unmasking_request()->add_dead_3_client_ids(7);
  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r3_state.HandleMessage(request);
  ASSERT_TRUE(new_state.ok());
  EXPECT_THAT(new_state.value()->StateName(), StrEq("ABORTED"));
  EXPECT_THAT(new_state.value()->ErrorMessage().value(), StrEq(error_string));
}

TEST(SecAggClientR3UnmaskingStateTest,
     UnmaskingRequestCausesAbortIfServerListsClientThatAlreadyDied) {
  // In this test, client 3 died at round 1, but the server lists client 3 as
  // dead at round 3. This should cause a transition to an abort state and an
  // abort message to be sent to the server.
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  std::vector<OtherClientState> other_clients_states{
      OtherClientState::kAlive, OtherClientState::kAlive,
      OtherClientState::kAlive, OtherClientState::kDeadAtRound1,
      OtherClientState::kAlive, OtherClientState::kAlive};

  SecAggClientR3UnmaskingState r3_state(
      1,  // client_id
      5,  // number_of_alive_neighbors
      4,  // minimum_surviving_neighbors_for_reconstruction
      6,  // number_of_neighbors
      std::make_unique<std::vector<OtherClientState> >(other_clients_states),
      std::make_unique<std::vector<ShamirShare> >(6, test_pairwise_share),
      std::make_unique<std::vector<ShamirShare> >(6, test_self_share),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener));

  std::string error_string =
      "The received UnmaskingRequest considers a client dead in round 3 "
      "that was already considered dead.";
  ClientToServerWrapperMessage expected_message;
  expected_message.mutable_abort()->set_diagnostic_info(error_string);
  EXPECT_CALL(*sender, Send(Pointee(EqualsProto(expected_message))));

  ServerToClientWrapperMessage request;
  // TODO(team): 4 -> 3 below, once backwards compatibility not needed.
  request.mutable_unmasking_request()->add_dead_3_client_ids(4);
  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r3_state.HandleMessage(request);
  ASSERT_TRUE(new_state.ok());
  EXPECT_THAT(new_state.value()->StateName(), StrEq("ABORTED"));
  EXPECT_THAT(new_state.value()->ErrorMessage().value(), StrEq(error_string));
}

TEST(SecAggClientR3UnmaskingStateTest,
     UnmaskingRequestCausesAbortIfServerListsSameClientTwice) {
  // In this test, the server lists client 5 as dead at round 3 twice. This
  // should cause a transition to an abort state and an abort message to be sent
  // to the server.
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();

  SecAggClientR3UnmaskingState r3_state(
      1,  // client_id
      6,  // number_of_alive_neighbors
      4,  // minimum_surviving_neighbors_for_reconstruction
      6,  // number_of_neighbors
      std::make_unique<std::vector<OtherClientState> >(
          6, OtherClientState::kAlive),
      std::make_unique<std::vector<ShamirShare> >(6, test_pairwise_share),
      std::make_unique<std::vector<ShamirShare> >(6, test_self_share),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener));

  std::string error_string =
      "The received UnmaskingRequest repeated a client more than once as a "
      "dead client.";
  ClientToServerWrapperMessage expected_message;
  expected_message.mutable_abort()->set_diagnostic_info(error_string);
  EXPECT_CALL(*sender, Send(Pointee(EqualsProto(expected_message))));

  ServerToClientWrapperMessage request;
  // TODO(team): 6 -> 5 below, once backwards compatibility not needed.
  request.mutable_unmasking_request()->add_dead_3_client_ids(6);
  request.mutable_unmasking_request()->add_dead_3_client_ids(6);
  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r3_state.HandleMessage(request);
  ASSERT_TRUE(new_state.ok());
  EXPECT_THAT(new_state.value()->StateName(), StrEq("ABORTED"));
  EXPECT_THAT(new_state.value()->ErrorMessage().value(), StrEq(error_string));
}

}  // namespace
}  // namespace secagg
}  // namespace fcp
