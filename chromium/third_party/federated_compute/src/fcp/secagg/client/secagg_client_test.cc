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

#include "fcp/secagg/client/secagg_client.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "fcp/base/monitoring.h"
#include "fcp/secagg/client/send_to_server_interface.h"
#include "fcp/secagg/client/state_transition_listener_interface.h"
#include "fcp/secagg/shared/aes_ctr_prng_factory.h"
#include "fcp/secagg/shared/compute_session_id.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/secagg/shared/secagg_vector.h"
#include "fcp/secagg/testing/ecdh_pregenerated_test_keys.h"
#include "fcp/secagg/testing/fake_prng.h"
#include "fcp/secagg/testing/mock_send_to_server_interface.h"
#include "fcp/secagg/testing/mock_state_transition_listener.h"
#include "fcp/testing/testing.h"

// All of the actual client functionality is contained within the
// SecAggClient*State classes. This class only tests very basic functionality
// of the containing SecAggClient class.

namespace fcp {
namespace secagg {
namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::Pointee;

TEST(SecAggClientTest, ConstructedWithCorrectState) {
  std::vector<InputVectorSpecification> input_vector_specs;
  input_vector_specs.push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClient client(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      input_vector_specs, std::make_unique<FakePrng>(),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  EXPECT_THAT(client.IsAborted(), Eq(false));
  EXPECT_THAT(client.IsCompletedSuccessfully(), Eq(false));
  EXPECT_THAT(client.State(), Eq("R0_ADVERTISE_KEYS_INPUT_NOT_SET"));
}

TEST(SecAggClientTest, StartCausesStateTransition) {
  std::vector<InputVectorSpecification> input_vector_specs;
  input_vector_specs.push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClient client(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      input_vector_specs, std::make_unique<FakePrng>(),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),

      std::make_unique<AesCtrPrngFactory>());

  // Message correctness is checked in the tests for the Round 0 classes.
  EXPECT_CALL(*sender, Send(::testing::_));
  Status result = client.Start();

  EXPECT_THAT(result.code(), Eq(OK));
  EXPECT_THAT(client.IsAborted(), Eq(false));
  EXPECT_THAT(client.IsCompletedSuccessfully(), Eq(false));
  EXPECT_THAT(client.State(), Eq("R1_SHARE_KEYS_INPUT_NOT_SET"));
}

TEST(SecAggClientTest, ReceiveMessageReturnValuesAreCorrect) {
  // The actual behavior of the client upon receipt of messages is tested in the
  // state class test files; here we just check that ReceiveMessage returns
  // values correctly.
  std::vector<InputVectorSpecification> input_vector_specs;
  input_vector_specs.push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();

  SecAggClient client(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      input_vector_specs, std::make_unique<FakePrng>(),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),

      std::make_unique<AesCtrPrngFactory>());

  // Get the client into a state where it can receive a message.
  ClientToServerWrapperMessage round_0_client_message;
  EXPECT_CALL(*sender, Send(_))
      .WillOnce(::testing::SaveArgPointee<0>(&round_0_client_message));
  EXPECT_THAT(client.Start(), IsOk());

  ServerToClientWrapperMessage round_1_message;
  EcdhPregeneratedTestKeys ecdh_keys;
  for (int i = 0; i < 4; ++i) {
    PairOfPublicKeys* keypair = round_1_message.mutable_share_keys_request()
                                    ->add_pairs_of_public_keys();
    if (i == 1) {
      *keypair = round_0_client_message.advertise_keys().pair_of_public_keys();
    } else {
      keypair->set_enc_pk(ecdh_keys.GetPublicKeyString(2 * i));
      keypair->set_noise_pk(ecdh_keys.GetPublicKeyString(2 * i + 1));
    }
  }
  round_1_message.mutable_share_keys_request()->set_session_id(
      ComputeSessionId(round_1_message.share_keys_request()).data);

  EXPECT_CALL(*sender, Send(_));

  // A valid message from the server should return true if it can continue.
  StatusOr<bool> result = client.ReceiveMessage(round_1_message);
  ASSERT_THAT(result.ok(), Eq(true));
  EXPECT_THAT(result.value(), Eq(true));

  // An abort message from the server should return false.
  ServerToClientWrapperMessage abort_message;
  abort_message.mutable_abort()->set_early_success(false);
  result = client.ReceiveMessage(abort_message);
  ASSERT_THAT(result.ok(), Eq(true));
  EXPECT_THAT(result.value(), Eq(false));

  // Any other message after abort should raise an error.
  result = client.ReceiveMessage(abort_message);
  EXPECT_THAT(result.ok(), Eq(false));
}

TEST(SecAggClientTest, AbortMovesToCorrectStateAndSendsMessageToServer) {
  std::vector<InputVectorSpecification> input_vector_specs;
  input_vector_specs.push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();

  SecAggClient client(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      input_vector_specs, std::make_unique<FakePrng>(),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),

      std::make_unique<AesCtrPrngFactory>());

  std::string error_string =
      "Abort upon external request for reason <Abort reason>.";
  ClientToServerWrapperMessage expected_message;
  expected_message.mutable_abort()->set_diagnostic_info(error_string);
  EXPECT_CALL(*sender, Send(Pointee(EqualsProto(expected_message))));

  Status result = client.Abort("Abort reason");
  EXPECT_THAT(result.code(), Eq(OK));
  EXPECT_THAT(client.State(), Eq("ABORTED"));
  EXPECT_THAT(client.ErrorMessage().value(), Eq(error_string));
}

TEST(SecAggClientTest,
     AbortWithNoMessageMovesToCorrectStateAndSendsMessageToServer) {
  std::vector<InputVectorSpecification> input_vector_specs;
  input_vector_specs.push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();

  SecAggClient client(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      input_vector_specs, std::make_unique<FakePrng>(),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),

      std::make_unique<AesCtrPrngFactory>());

  std::string error_string =
      "Abort upon external request for reason <unknown reason>.";
  ClientToServerWrapperMessage expected_message;
  expected_message.mutable_abort()->set_diagnostic_info(error_string);
  EXPECT_CALL(*sender, Send(Pointee(EqualsProto(expected_message))));

  Status result = client.Abort();
  EXPECT_THAT(result.code(), Eq(OK));
  EXPECT_THAT(client.State(), Eq("ABORTED"));
  EXPECT_THAT(client.ErrorMessage().value(), Eq(error_string));
}

TEST(SecAggClientTest, ErrorMessageRaisesErrorStatusIfNotAborted) {
  std::vector<InputVectorSpecification> input_vector_specs;
  input_vector_specs.push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();

  SecAggClient client(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      input_vector_specs, std::make_unique<FakePrng>(),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),

      std::make_unique<AesCtrPrngFactory>());

  EXPECT_THAT(client.ErrorMessage().ok(), Eq(false));
}

TEST(SecAggClientTest, SetInputChangesStateOnlyOnce) {
  std::vector<InputVectorSpecification> input_vector_specs;
  input_vector_specs.push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();

  SecAggClient client(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      input_vector_specs, std::make_unique<FakePrng>(),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),

      std::make_unique<AesCtrPrngFactory>());

  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({5, 8, 22, 30}, 32));

  Status result = client.SetInput(std::move(input_map));
  EXPECT_THAT(result.code(), Eq(OK));

  auto input_map2 = std::make_unique<SecAggVectorMap>();
  input_map2->emplace("test", SecAggVector({5, 8, 22, 30}, 32));
  result = client.SetInput(std::move(input_map));
  EXPECT_THAT(result.code(), Eq(FAILED_PRECONDITION));
  EXPECT_THAT(client.State(), Eq("R0_ADVERTISE_KEYS_INPUT_SET"));
}

}  // namespace
}  // namespace secagg
}  // namespace fcp
