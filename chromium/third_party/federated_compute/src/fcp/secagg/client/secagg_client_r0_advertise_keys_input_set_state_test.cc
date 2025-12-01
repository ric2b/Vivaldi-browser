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

#include "fcp/secagg/client/secagg_client_r0_advertise_keys_input_set_state.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/node_hash_map.h"
#include "fcp/secagg/client/secagg_client_state.h"
#include "fcp/secagg/shared/aes_ctr_prng_factory.h"
#include "fcp/secagg/shared/ecdh_keys.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/secagg/shared/secagg_vector.h"
#include "fcp/secagg/testing/fake_prng.h"
#include "fcp/secagg/testing/mock_send_to_server_interface.h"
#include "fcp/secagg/testing/mock_state_transition_listener.h"
#include "fcp/testing/testing.h"

namespace fcp {
namespace secagg {
namespace {

using ::testing::Eq;
using ::testing::Pointee;

TEST(SecaggClientR0AdvertiseKeysInputSetStateTest, IsAbortedReturnsFalse) {
  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({2, 4, 6, 8}, 32));
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR0AdvertiseKeysInputSetState r0_state(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      std::move(input_map), std::move(input_vector_specs),
      std::make_unique<FakePrng>(),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  EXPECT_THAT(r0_state.IsAborted(), Eq(false));
}

TEST(SecaggClientR0AdvertiseKeysInputSetStateTest,
     IsCompletedSuccessfullyReturnsFalse) {
  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({2, 4, 6, 8}, 32));
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR0AdvertiseKeysInputSetState r0_state(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      std::move(input_map), std::move(input_vector_specs),
      std::make_unique<FakePrng>(),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  EXPECT_THAT(r0_state.IsCompletedSuccessfully(), Eq(false));
}

// A gMock matcher that just checks that the message is a valid AdvertiseKeys
// message, with the right fields set to the right lengths.
MATCHER(IsValidAdvertiseKeysMessage, "") {
  return (arg->advertise_keys().pair_of_public_keys().enc_pk().size() ==
          EcdhPublicKey::kSize) &&
         (arg->advertise_keys().pair_of_public_keys().noise_pk().size() ==
          EcdhPublicKey::kSize);
}

TEST(SecaggClientR0AdvertiseKeysInputSetStateTest,
     StartSendsCorrectMessageAndTransitionsState) {
  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({2, 4, 6, 8}, 32));
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR0AdvertiseKeysInputSetState r0_state(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      std::move(input_map), std::move(input_vector_specs),
      std::make_unique<FakePrng>(),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  EXPECT_CALL(*sender, Send(IsValidAdvertiseKeysMessage())).Times(1);

  StatusOr<std::unique_ptr<SecAggClientState> > new_state = r0_state.Start();
  ASSERT_TRUE(new_state.ok());
  EXPECT_THAT(new_state.value()->StateName(), Eq("R1_SHARE_KEYS_INPUT_SET"));
}

TEST(SecaggClientR0AdvertiseKeysInputSetStateTest, SetInputRaisesErrorStatus) {
  std::vector<uint64_t> vec = {2, 4, 6, 8};
  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector(vec, 32));
  auto copy_of_input_map = std::make_unique<SecAggVectorMap>();
  copy_of_input_map->emplace("test", SecAggVector(vec, 32));
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR0AdvertiseKeysInputSetState r0_state(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      std::move(input_map), std::move(input_vector_specs),
      std::make_unique<FakePrng>(),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  EXPECT_THAT(r0_state.SetInput(std::move(copy_of_input_map)).ok(), Eq(false));
}

TEST(SecaggClientR0AdvertiseKeysInputSetStateTest,
     ErrorMessageRaisesErrorStatus) {
  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({2, 4, 6, 8}, 32));
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR0AdvertiseKeysInputSetState r0_state(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      std::move(input_map), std::move(input_vector_specs),
      std::make_unique<FakePrng>(),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  EXPECT_THAT(r0_state.ErrorMessage().ok(), Eq(false));
}

TEST(SecaggClientR0AdvertiseKeysInputSetStateTest,
     AbortReturnsValidAbortStateAndNotifiesServer) {
  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({2, 4, 6, 8}, 32));
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR0AdvertiseKeysInputSetState r0_state(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      std::move(input_map), std::move(input_vector_specs),
      std::make_unique<FakePrng>(),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  std::string error_string =
      "Abort upon external request for reason <Abort reason>.";
  ClientToServerWrapperMessage expected_message;
  expected_message.mutable_abort()->set_diagnostic_info(error_string);
  EXPECT_CALL(*sender, Send(Pointee(EqualsProto(expected_message))));

  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r0_state.Abort("Abort reason");
  ASSERT_THAT(new_state.ok(), Eq(true));
  EXPECT_THAT(new_state.value()->StateName(), Eq("ABORTED"));
  EXPECT_THAT(new_state.value()->ErrorMessage().value(), Eq(error_string));
}

TEST(SecaggClientR0AdvertiseKeysInputSetStateTest,
     AbortFailureMessageCausesAbortWithoutNotifyingServer) {
  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({2, 4, 6, 8}, 32));
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR0AdvertiseKeysInputSetState r0_state(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      std::move(input_map), std::move(input_vector_specs),
      std::make_unique<FakePrng>(),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  EXPECT_CALL(*sender, Send(::testing::_)).Times(0);
  ServerToClientWrapperMessage abort_message;
  abort_message.mutable_abort()->set_early_success(false);

  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r0_state.HandleMessage(abort_message);
  ASSERT_TRUE(new_state.ok());
  EXPECT_THAT(new_state.value()->StateName(), Eq("ABORTED"));
  EXPECT_THAT(new_state.value()->ErrorMessage().value(),
              Eq("Aborting because of abort message from the server."));
}

TEST(SecaggClientR0AdvertiseKeysInputSetStateTest,
     EarlySuccessMessageCausesTransitionToCompletedState) {
  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({2, 4, 6, 8}, 32));
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR0AdvertiseKeysInputSetState r0_state(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      std::move(input_map), std::move(input_vector_specs),
      std::make_unique<FakePrng>(),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  EXPECT_CALL(*sender, Send(::testing::_)).Times(0);
  ServerToClientWrapperMessage abort_message;
  abort_message.mutable_abort()->set_early_success(true);

  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r0_state.HandleMessage(abort_message);
  ASSERT_TRUE(new_state.ok());
  EXPECT_THAT(new_state.value()->StateName(), Eq("COMPLETED"));
}

TEST(SecaggClientR0AdvertiseKeysInputSetStateTest,
     HandleNonAbortMessageRaisesError) {
  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({2, 4, 6, 8}, 32));
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR0AdvertiseKeysInputSetState r0_state(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      std::move(input_map), std::move(input_vector_specs),
      std::make_unique<FakePrng>(),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  ServerToClientWrapperMessage message;
  message.mutable_share_keys_request()->add_pairs_of_public_keys();

  EXPECT_THAT(r0_state.HandleMessage(message).ok(), Eq(false));
}

}  // namespace
}  // namespace secagg
}  // namespace fcp
