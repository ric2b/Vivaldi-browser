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

#include "fcp/secagg/client/secagg_client_r2_masked_input_coll_waiting_for_input_state.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/node_hash_map.h"
#include "fcp/secagg/client/other_client_state.h"
#include "fcp/secagg/client/secagg_client_aborted_state.h"
#include "fcp/secagg/client/secagg_client_r2_masked_input_coll_base_state.h"
#include "fcp/secagg/client/secagg_client_r3_unmasking_state.h"
#include "fcp/secagg/client/send_to_server_interface.h"
#include "fcp/secagg/client/state_transition_listener_interface.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/map_of_masks.h"
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

static const ShamirShare test_pairwise_share = {"test pairwise share"};
static const ShamirShare test_self_share = {"test self share"};

TEST(SecAggClientR2MaskedInputCollWaitingForInputStateTest,
     IsAbortedReturnsFalse) {
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  auto map_of_masks = std::make_unique<SecAggVectorMap>();
  map_of_masks->emplace("test", SecAggVector({1, 2, 3, 4}, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR2MaskedInputCollWaitingForInputState r2_state(
      1,  // client_id
      4,  // minimum_surviving_neighbors_for_reconstruction
      6,  // number_of_alive_neighbors
      6,  // number_of_neighbors
      std::move(input_vector_specs), std::move(map_of_masks),
      std::make_unique<std::vector<OtherClientState> >(
          6, OtherClientState::kAlive),
      std::make_unique<std::vector<ShamirShare> >(6, test_pairwise_share),
      std::make_unique<std::vector<ShamirShare> >(6, test_self_share),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener));

  EXPECT_THAT(r2_state.IsAborted(), Eq(false));
}

TEST(SecAggClientR2MaskedInputCollWaitingForInputStateTest,
     IsCompletedSuccessfullyReturnsFalse) {
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  auto map_of_masks = std::make_unique<SecAggVectorMap>();
  map_of_masks->emplace("test", SecAggVector({1, 2, 3, 4}, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR2MaskedInputCollWaitingForInputState r2_state(
      1,  // client_id
      4,  // minimum_surviving_neighbors_for_reconstruction
      6,  // number_of_alive_neighbors
      6,  // number_of_neighbors
      std::move(input_vector_specs), std::move(map_of_masks),
      std::make_unique<std::vector<OtherClientState> >(
          6, OtherClientState::kAlive),
      std::make_unique<std::vector<ShamirShare> >(6, test_pairwise_share),
      std::make_unique<std::vector<ShamirShare> >(6, test_self_share),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener));

  EXPECT_THAT(r2_state.IsCompletedSuccessfully(), Eq(false));
}

TEST(SecAggClientR2MaskedInputCollWaitingForInputStateTest,
     StartRaisesErrorStatus) {
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  auto map_of_masks = std::make_unique<SecAggVectorMap>();
  map_of_masks->emplace("test", SecAggVector({1, 2, 3, 4}, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR2MaskedInputCollWaitingForInputState r2_state(
      1,  // client_id
      4,  // minimum_surviving_neighbors_for_reconstruction
      6,  // number_of_alive_neighbors
      6,  // number_of_neighbors
      std::move(input_vector_specs), std::move(map_of_masks),
      std::make_unique<std::vector<OtherClientState> >(
          6, OtherClientState::kAlive),
      std::make_unique<std::vector<ShamirShare> >(6, test_pairwise_share),
      std::make_unique<std::vector<ShamirShare> >(6, test_self_share),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener));

  EXPECT_THAT(r2_state.Start().ok(), Eq(false));
}

TEST(SecAggClientR2MaskedInputCollWaitingForInputStateTest,
     ErrorMessageRaisesErrorStatus) {
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  auto map_of_masks = std::make_unique<SecAggVectorMap>();
  map_of_masks->emplace("test", SecAggVector({1, 2, 3, 4}, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR2MaskedInputCollWaitingForInputState r2_state(
      1,  // client_id
      4,  // minimum_surviving_neighbors_for_reconstruction
      6,  // number_of_alive_neighbors
      6,  // number_of_neighbors
      std::move(input_vector_specs), std::move(map_of_masks),
      std::make_unique<std::vector<OtherClientState> >(
          6, OtherClientState::kAlive),
      std::make_unique<std::vector<ShamirShare> >(6, test_pairwise_share),
      std::make_unique<std::vector<ShamirShare> >(6, test_self_share),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener));

  EXPECT_THAT(r2_state.ErrorMessage().ok(), Eq(false));
}

TEST(SecAggClientR2MaskedInputCollWaitingForInputStateTest,
     AbortReturnsValidAbortStateAndNotifiesServer) {
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  auto map_of_masks = std::make_unique<SecAggVectorMap>();
  map_of_masks->emplace("test", SecAggVector({1, 2, 3, 4}, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR2MaskedInputCollWaitingForInputState r2_state(
      1,  // client_id
      4,  // minimum_surviving_neighbors_for_reconstruction
      6,  // number_of_alive_neighbors
      6,  // number_of_neighbors
      std::move(input_vector_specs), std::move(map_of_masks),
      std::make_unique<std::vector<OtherClientState> >(
          6, OtherClientState::kAlive),
      std::make_unique<std::vector<ShamirShare> >(6, test_pairwise_share),
      std::make_unique<std::vector<ShamirShare> >(6, test_self_share),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener));

  std::string error_string =
      "Abort upon external request for reason <Abort reason>.";
  ClientToServerWrapperMessage expected_message;
  expected_message.mutable_abort()->set_diagnostic_info(error_string);
  EXPECT_CALL(*sender, Send(Pointee(EqualsProto(expected_message))));

  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r2_state.Abort("Abort reason");
  ASSERT_THAT(new_state.ok(), Eq(true));
  EXPECT_THAT(new_state.value()->StateName(), Eq("ABORTED"));
  EXPECT_THAT(new_state.value()->ErrorMessage().value(), Eq(error_string));
}

TEST(SecAggClientR2MaskedInputCollWaitingForInputStateTest,
     AbortFailureMessageCausesAbortWithoutNotifyingServer) {
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  auto map_of_masks = std::make_unique<SecAggVectorMap>();
  map_of_masks->emplace("test", SecAggVector({1, 2, 3, 4}, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR2MaskedInputCollWaitingForInputState r2_state(
      1,  // client_id
      4,  // minimum_surviving_neighbors_for_reconstruction
      6,  // number_of_alive_neighbors
      6,  // number_of_neighbors
      std::move(input_vector_specs), std::move(map_of_masks),
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
      r2_state.HandleMessage(abort_message);
  ASSERT_TRUE(new_state.ok());
  EXPECT_THAT(new_state.value()->StateName(), Eq("ABORTED"));
  EXPECT_THAT(new_state.value()->ErrorMessage().value(),
              Eq("Aborting because of abort message from the server."));
}

TEST(SecAggClientR2MaskedInputCollWaitingForInputStateTest,
     EarlySuccessMessageCausesTransitionToCompletedState) {
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  auto map_of_masks = std::make_unique<SecAggVectorMap>();
  map_of_masks->emplace("test", SecAggVector({1, 2, 3, 4}, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR2MaskedInputCollWaitingForInputState r2_state(
      1,  // client_id
      4,  // minimum_surviving_neighbors_for_reconstruction
      6,  // number_of_alive_neighbors
      6,  // number_of_neighbors
      std::move(input_vector_specs), std::move(map_of_masks),
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
      r2_state.HandleMessage(abort_message);
  ASSERT_TRUE(new_state.ok());
  EXPECT_THAT(new_state.value()->StateName(), Eq("COMPLETED"));
}

TEST(SecAggClientR2MaskedInputCollWaitingForInputStateTest,
     SetInputCausesClientResponseAndRound3Transition) {
  // In this test, the client under test is id 1, and there are 4 clients, all
  // alive.
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  auto map_of_masks = std::make_unique<SecAggVectorMap>();
  map_of_masks->emplace("test", SecAggVector({28, 8, 10, 4}, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR2MaskedInputCollWaitingForInputState r2_state(
      1,  // client_id
      3,  // minimum_surviving_neighbors_for_reconstruction
      4,  // number_of_alive_neighbors
      4,  // number_of_neighbors
      std::move(input_vector_specs), std::move(map_of_masks),
      std::make_unique<std::vector<OtherClientState> >(
          6, OtherClientState::kAlive),
      std::make_unique<std::vector<ShamirShare> >(6, test_pairwise_share),
      std::make_unique<std::vector<ShamirShare> >(6, test_self_share),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener));

  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({5, 8, 22, 30}, 32));

  std::vector<uint64_t> sum_vec = {1, 16, 0, 2};

  MaskedInputVector sum_vec_proto;
  sum_vec_proto.set_encoded_vector(
      SecAggVector(sum_vec, 32).GetAsPackedBytes());
  ClientToServerWrapperMessage expected_message;
  (*expected_message.mutable_masked_input_response()
        ->mutable_vectors())["test"] = sum_vec_proto;

  EXPECT_CALL(*sender, Send(Pointee(EqualsProto(expected_message))));

  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r2_state.SetInput(std::move(input_map));
  ASSERT_TRUE(new_state.ok());
  EXPECT_THAT(new_state.value()->StateName(), Eq("R3_UNMASKING"));
}

TEST(SecAggClientR2MaskedInputCollWaitingForInputStateTest,
     SetInputRaisesErrorStatusIfInputVectorIsWrongSize) {
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  auto map_of_masks = std::make_unique<SecAggVectorMap>();
  map_of_masks->emplace("test", SecAggVector({28, 8, 10, 4}, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR2MaskedInputCollWaitingForInputState r2_state(
      1,  // client_id
      3,  // minimum_surviving_neighbors_for_reconstruction
      4,  // number_of_alive_neighbors
      4,  // number_of_neighbors
      std::move(input_vector_specs), std::move(map_of_masks),
      std::make_unique<std::vector<OtherClientState> >(
          6, OtherClientState::kAlive),
      std::make_unique<std::vector<ShamirShare> >(6, test_pairwise_share),
      std::make_unique<std::vector<ShamirShare> >(6, test_self_share),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener));

  auto input_map = std::make_unique<SecAggVectorMap>();
  // This vector has too many elements.
  input_map->emplace("test", SecAggVector({5, 8, 22, 30, 7}, 32));

  EXPECT_CALL(*sender, Send(::testing::_)).Times(0);

  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r2_state.SetInput(std::move(input_map));
  EXPECT_THAT(new_state.ok(), Eq(false));
}

TEST(SecAggClientR2MaskedInputCollWaitingForInputStateTest,
     SetInputRaisesErrorStatusIfInputVectorIsTooLargeForBitWidth) {
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  auto map_of_masks = std::make_unique<SecAggVectorMap>();
  map_of_masks->emplace("test", SecAggVector({28, 8, 10, 4}, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR2MaskedInputCollWaitingForInputState r2_state(
      1,  // client_id
      3,  // minimum_surviving_neighbors_for_reconstruction
      4,  // number_of_alive_neighbors
      4,  // number_of_neighbors
      std::move(input_vector_specs), std::move(map_of_masks),
      std::make_unique<std::vector<OtherClientState> >(
          6, OtherClientState::kAlive),
      std::make_unique<std::vector<ShamirShare> >(6, test_pairwise_share),
      std::make_unique<std::vector<ShamirShare> >(6, test_self_share),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener));

  auto input_map = std::make_unique<SecAggVectorMap>();
  // This vector's bit_width does not match the specified modulus of 32.
  input_map->emplace("test", SecAggVector({5, 8, 22, 40}, 64));

  EXPECT_CALL(*sender, Send(::testing::_)).Times(0);

  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r2_state.SetInput(std::move(input_map));
  EXPECT_THAT(new_state.ok(), Eq(false));
}

TEST(SecAggClientR2MaskedInputCollWaitingForInputStateTest,
     SetInputRaisesErrorStatusIfInputVectorHasWrongName) {
  // In this test, the client under test is id 1, and there are 4 clients, all
  // alive.
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  auto map_of_masks = std::make_unique<SecAggVectorMap>();
  map_of_masks->emplace("test", SecAggVector({28, 8, 10, 4}, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR2MaskedInputCollWaitingForInputState r2_state(
      1,  // client_id
      3,  // minimum_surviving_neighbors_for_reconstruction
      4,  // number_of_alive_neighbors
      4,  // number_of_neighbors
      std::move(input_vector_specs), std::move(map_of_masks),
      std::make_unique<std::vector<OtherClientState> >(
          6, OtherClientState::kAlive),
      std::make_unique<std::vector<ShamirShare> >(6, test_pairwise_share),
      std::make_unique<std::vector<ShamirShare> >(6, test_self_share),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener));

  auto input_map = std::make_unique<SecAggVectorMap>();
  // This vector has the wrong name.
  input_map->emplace("incorrect", SecAggVector({5, 8, 22, 30}, 32));

  EXPECT_CALL(*sender, Send(::testing::_)).Times(0);

  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r2_state.SetInput(std::move(input_map));
  EXPECT_THAT(new_state.ok(), Eq(false));
}

TEST(SecAggClientR2MaskedInputCollWaitingForInputStateTest,
     SetInputRaisesErrorStatusIfInputHasTooManyVectors) {
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  auto map_of_masks = std::make_unique<SecAggVectorMap>();
  map_of_masks->emplace("test", SecAggVector({28, 8, 10, 4}, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR2MaskedInputCollWaitingForInputState r2_state(
      1,  // client_id
      3,  // minimum_surviving_neighbors_for_reconstruction
      4,  // number_of_alive_neighbors
      4,  // number_of_neighbors
      std::move(input_vector_specs), std::move(map_of_masks),
      std::make_unique<std::vector<OtherClientState> >(
          6, OtherClientState::kAlive),
      std::make_unique<std::vector<ShamirShare> >(6, test_pairwise_share),
      std::make_unique<std::vector<ShamirShare> >(6, test_self_share),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener));

  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({5, 8, 22, 30}, 32));
  // This vector is extra.
  input_map->emplace("test2", SecAggVector({4, 7, 21, 29}, 32));

  EXPECT_CALL(*sender, Send(::testing::_)).Times(0);

  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r2_state.SetInput(std::move(input_map));
  EXPECT_THAT(new_state.ok(), Eq(false));
}

TEST(SecAggClientR2MaskedInputCollWaitingForInputStateTest,
     SetInputRaisesErrorStatusIfInputHasTooFewVectors) {
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  // Expects two vectors.
  input_vector_specs->push_back(InputVectorSpecification("test2", 4, 32));
  auto map_of_masks = std::make_unique<SecAggVectorMap>();
  map_of_masks->emplace("test", SecAggVector({28, 8, 10, 4}, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR2MaskedInputCollWaitingForInputState r2_state(
      1,  // client_id
      3,  // minimum_surviving_neighbors_for_reconstruction
      4,  // number_of_alive_neighbors
      4,  // number_of_neighbors
      std::move(input_vector_specs), std::move(map_of_masks),
      std::make_unique<std::vector<OtherClientState> >(
          6, OtherClientState::kAlive),
      std::make_unique<std::vector<ShamirShare> >(6, test_pairwise_share),
      std::make_unique<std::vector<ShamirShare> >(6, test_self_share),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener));

  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({5, 8, 22, 30}, 32));
  // Missing second vector.

  EXPECT_CALL(*sender, Send(::testing::_)).Times(0);

  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r2_state.SetInput(std::move(input_map));
  EXPECT_THAT(new_state.ok(), Eq(false));
}

}  // namespace
}  // namespace secagg
}  // namespace fcp
