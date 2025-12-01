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

#include "fcp/secagg/server/secagg_server_prng_running_state.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_map.h"
#include "absl/container/node_hash_set.h"
#include "absl/strings/str_cat.h"
#include "fcp/base/monitoring.h"
#include "fcp/base/scheduler.h"
#include "fcp/secagg/server/aes/aes_secagg_server_protocol_impl.h"
#include "fcp/secagg/server/secagg_scheduler.h"
#include "fcp/secagg/server/secagg_server_enums.pb.h"
#include "fcp/secagg/server/secret_sharing_graph_factory.h"
#include "fcp/secagg/shared/aes_ctr_prng_factory.h"
#include "fcp/secagg/shared/aes_key.h"
#include "fcp/secagg/shared/ecdh_key_agreement.h"
#include "fcp/secagg/shared/ecdh_keys.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/map_of_masks.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/secagg/shared/secagg_vector.h"
#include "fcp/secagg/shared/shamir_secret_sharing.h"
#include "fcp/secagg/testing/ecdh_pregenerated_test_keys.h"
#include "fcp/secagg/testing/fake_prng.h"
#include "fcp/secagg/testing/server/mock_secagg_server_metrics_listener.h"
#include "fcp/secagg/testing/server/mock_send_to_clients_interface.h"
#include "fcp/secagg/testing/server/test_async_runner.h"
#include "fcp/secagg/testing/test_matchers.h"
#include "fcp/testing/testing.h"
#include "fcp/tracing/test_tracing_recorder.h"

namespace fcp {
namespace secagg {
namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::NiceMock;

// For testing purposes, make an AesKey out of a string.
AesKey MakeAesKey(const std::string& key) {
  EXPECT_THAT(key.size(), Eq(AesKey::kSize));
  return AesKey(reinterpret_cast<const uint8_t*>(key.c_str()));
}

class MockScheduler : public Scheduler {
 public:
  MOCK_METHOD(void, Schedule, (std::function<void()>), (override));
  MOCK_METHOD(void, WaitUntilIdle, ());
};

constexpr auto call_fn = [](const std::function<void()>& f) { f(); };

// Default test session_id.
std::unique_ptr<SessionId> MakeTestSessionId() {
  SessionId session_id = {"session id number, 32 bytes long"};
  return std::make_unique<SessionId>(session_id);
}

std::unique_ptr<AesSecAggServerProtocolImpl> CreateSecAggServerProtocolImpl(
    std::vector<InputVectorSpecification> input_vector_specs,
    MockSendToClientsInterface* sender,
    MockSecAggServerMetricsListener* metrics_listener = nullptr) {
  SecretSharingGraphFactory factory;
  auto parallel_scheduler = std::make_unique<NiceMock<MockScheduler>>();
  auto sequential_scheduler = std::make_unique<NiceMock<MockScheduler>>();
  EXPECT_CALL(*parallel_scheduler, Schedule(_)).WillRepeatedly(call_fn);
  EXPECT_CALL(*sequential_scheduler, Schedule(_)).WillRepeatedly(call_fn);
  auto impl = std::make_unique<AesSecAggServerProtocolImpl>(
      factory.CreateCompleteGraph(4, 3),  // total number of clients is 4
      3,  // minimum_number_of_clients_to_proceed
      input_vector_specs,
      std::unique_ptr<MockSecAggServerMetricsListener>(metrics_listener),
      std::make_unique<AesCtrPrngFactory>(), sender,
      std::make_unique<TestAsyncRunner>(std::move(parallel_scheduler),
                                        std::move(sequential_scheduler)),
      std::vector<ClientStatus>(4, ClientStatus::UNMASKING_RESPONSE_RECEIVED),
      ServerVariant::NATIVE_V1);
  impl->set_session_id(MakeTestSessionId());
  EcdhPregeneratedTestKeys ecdh_keys;
  for (int i = 0; i < 4; ++i) {
    impl->SetPairwisePublicKeys(i, ecdh_keys.GetPublicKey(i));
  }
  impl->set_masked_input(std::make_unique<SecAggUnpackedVectorMap>());
  return impl;
}

// Mock class containing a callback that would be called when the PRNG is done.
class MockPrngDone {
 public:
  MOCK_METHOD(void, Callback, ());
};

TEST(SecaggServerPrngRunningStateTest, IsAbortedReturnsFalse) {
  auto input_vector_specs = std::vector<InputVectorSpecification>();
  input_vector_specs.push_back(InputVectorSpecification("foobar", 4, 32));
  auto sender = std::make_unique<MockSendToClientsInterface>();
  FakePrng prng;
  ShamirSecretSharing sharer;
  auto self_shamir_share_table = std::make_unique<
      absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>();
  for (int i = 0; i < 4; ++i) {
    self_shamir_share_table->try_emplace(
        i, sharer.Share(
               3, 4,
               MakeAesKey(absl::StrCat("test 32 byte AES key for user #", i))));
  }

  auto impl = CreateSecAggServerProtocolImpl(input_vector_specs, sender.get());
  impl->set_pairwise_shamir_share_table(
      std::make_unique<
          absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>());
  impl->set_self_shamir_share_table(std::move(self_shamir_share_table));

  SecAggServerPrngRunningState state(
      std::move(impl),
      0,   // number_of_clients_failed_after_sending_masked_input
      0,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  EXPECT_THAT(state.IsAborted(), Eq(false));
}

TEST(SecaggServerPrngRunningStateTest, IsCompletedSuccessfullyReturnsFalse) {
  auto input_vector_specs = std::vector<InputVectorSpecification>();
  input_vector_specs.push_back(InputVectorSpecification("foobar", 4, 32));
  auto sender = std::make_unique<MockSendToClientsInterface>();
  FakePrng prng;
  ShamirSecretSharing sharer;
  auto self_shamir_share_table = std::make_unique<
      absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>();
  for (int i = 0; i < 4; ++i) {
    self_shamir_share_table->try_emplace(
        i, sharer.Share(
               3, 4,
               MakeAesKey(absl::StrCat("test 32 byte AES key for user #", i))));
  }

  auto impl = CreateSecAggServerProtocolImpl(input_vector_specs, sender.get());
  impl->set_pairwise_shamir_share_table(
      std::make_unique<
          absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>());
  impl->set_self_shamir_share_table(std::move(self_shamir_share_table));

  SecAggServerPrngRunningState state(
      std::move(impl),
      0,   // number_of_clients_failed_after_sending_masked_input
      0,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  EXPECT_THAT(state.IsCompletedSuccessfully(), Eq(false));
}

TEST(SecaggServerPrngRunningStateTest, ErrorMessageRaisesError) {
  auto input_vector_specs = std::vector<InputVectorSpecification>();
  input_vector_specs.push_back(InputVectorSpecification("foobar", 4, 32));
  auto sender = std::make_unique<MockSendToClientsInterface>();
  FakePrng prng;
  ShamirSecretSharing sharer;
  auto self_shamir_share_table = std::make_unique<
      absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>();
  for (int i = 0; i < 4; ++i) {
    self_shamir_share_table->try_emplace(
        i, sharer.Share(
               3, 4,
               MakeAesKey(absl::StrCat("test 32 byte AES key for user #", i))));
  }

  auto impl = CreateSecAggServerProtocolImpl(input_vector_specs, sender.get());
  impl->set_pairwise_shamir_share_table(
      std::make_unique<
          absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>());
  impl->set_self_shamir_share_table(std::move(self_shamir_share_table));

  SecAggServerPrngRunningState state(
      std::move(impl),
      0,   // number_of_clients_failed_after_sending_masked_input
      0,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  EXPECT_THAT(state.ErrorMessage().ok(), Eq(false));
}

TEST(SecaggServerPrngRunningStateTest,
     NumberOfMessagesReceivedInThisRoundReturnsZero) {
  auto input_vector_specs = std::vector<InputVectorSpecification>();
  input_vector_specs.push_back(InputVectorSpecification("foobar", 4, 32));
  auto sender = std::make_unique<MockSendToClientsInterface>();
  FakePrng prng;
  ShamirSecretSharing sharer;
  auto self_shamir_share_table = std::make_unique<
      absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>();
  for (int i = 0; i < 4; ++i) {
    self_shamir_share_table->try_emplace(
        i, sharer.Share(
               3, 4,
               MakeAesKey(absl::StrCat("test 32 byte AES key for user #", i))));
  }

  auto impl = CreateSecAggServerProtocolImpl(input_vector_specs, sender.get());
  impl->set_pairwise_shamir_share_table(
      std::make_unique<
          absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>());
  impl->set_self_shamir_share_table(std::move(self_shamir_share_table));

  SecAggServerPrngRunningState state(
      std::move(impl),
      0,   // number_of_clients_failed_after_sending_masked_input
      0,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  EXPECT_THAT(state.NumberOfMessagesReceivedInThisRound(), Eq(0));
}

TEST(SecaggServerPrngRunningStateTest,
     NumberOfClientsReadyForNextRoundReturnsZero) {
  auto input_vector_specs = std::vector<InputVectorSpecification>();
  input_vector_specs.push_back(InputVectorSpecification("foobar", 4, 32));
  auto sender = std::make_unique<MockSendToClientsInterface>();
  FakePrng prng;
  ShamirSecretSharing sharer;
  auto self_shamir_share_table = std::make_unique<
      absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>();
  for (int i = 0; i < 4; ++i) {
    self_shamir_share_table->try_emplace(
        i, sharer.Share(
               3, 4,
               MakeAesKey(absl::StrCat("test 32 byte AES key for user #", i))));
  }

  auto impl = CreateSecAggServerProtocolImpl(input_vector_specs, sender.get());
  impl->set_pairwise_shamir_share_table(
      std::make_unique<
          absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>());
  impl->set_self_shamir_share_table(std::move(self_shamir_share_table));

  SecAggServerPrngRunningState state(
      std::move(impl),
      0,   // number_of_clients_failed_after_sending_masked_input
      0,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  EXPECT_THAT(state.NumberOfClientsReadyForNextRound(), Eq(0));
}

TEST(SecaggServerPrngRunningStateTest,
     HandleNonAbortMessageAbortsClientDoesNotRecordMetrics) {
  TestTracingRecorder tracing_recorder;
  auto input_vector_specs = std::vector<InputVectorSpecification>();
  input_vector_specs.push_back(InputVectorSpecification("foobar", 4, 32));
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();
  auto sender = std::make_unique<MockSendToClientsInterface>();
  FakePrng prng;
  ShamirSecretSharing sharer;
  auto self_shamir_share_table = std::make_unique<
      absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>();
  for (int i = 0; i < 4; ++i) {
    self_shamir_share_table->try_emplace(
        i, sharer.Share(
               3, 4,
               MakeAesKey(absl::StrCat("test 32 byte AES key for user #", i))));
  }

  auto impl =
      CreateSecAggServerProtocolImpl(input_vector_specs, sender.get(), metrics);
  impl->set_pairwise_shamir_share_table(
      std::make_unique<
          absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>());
  impl->set_self_shamir_share_table(std::move(self_shamir_share_table));

  SecAggServerPrngRunningState state(
      std::move(impl),
      0,   // number_of_clients_failed_after_sending_masked_input
      0,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  ServerToClientWrapperMessage abort_message;
  abort_message.mutable_abort()->set_early_success(false);
  abort_message.mutable_abort()->set_diagnostic_info(
      "Non-abort message sent during PrngUnmasking step.");

  ClientToServerWrapperMessage client_message;
  EXPECT_CALL(*sender, Send(Eq(0), EqualsProto(abort_message)));
  EXPECT_CALL(*metrics, MessageReceivedSizes(
                            Eq(ClientToServerWrapperMessage::
                                   MessageContentCase::MESSAGE_CONTENT_NOT_SET),
                            Eq(false), Eq(client_message.ByteSizeLong())));
  EXPECT_CALL(*metrics,
              IndividualMessageSizes(
                  Eq(ServerToClientWrapperMessage::MessageContentCase::kAbort),
                  Eq(abort_message.ByteSizeLong())));
  EXPECT_CALL(*metrics, ClientsDropped(_, _)).Times(0);

  EXPECT_THAT(state.HandleMessage(0, client_message), IsOk());
  EXPECT_THAT(state.NumberOfClientsFailedAfterSendingMaskedInput(), Eq(0));
  ASSERT_THAT(state.AbortedClientIds().contains(0), Eq(true));
  EXPECT_THAT(tracing_recorder.FindAllEvents<IndividualMessageSent>(),
              ElementsAre(IsEvent<IndividualMessageSent>(
                  Eq(0), Eq(ServerToClientMessageType_Abort),
                  Eq(abort_message.ByteSizeLong()))));
  EXPECT_THAT(tracing_recorder.FindAllEvents<ClientMessageReceived>(),
              ElementsAre(IsEvent<ClientMessageReceived>(
                  Eq(ClientToServerMessageType_MessageContentNotSet),
                  Eq(client_message.ByteSizeLong()), Eq(false), Ge(0))));
}

TEST(SecaggServerPrngRunningStateTest,
     HandleAbortMessageAbortsClientDoesNotRecordMetrics) {
  TestTracingRecorder tracing_recorder;
  auto input_vector_specs = std::vector<InputVectorSpecification>();
  input_vector_specs.push_back(InputVectorSpecification("foobar", 4, 32));
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();
  auto sender = std::make_unique<MockSendToClientsInterface>();
  FakePrng prng;
  ShamirSecretSharing sharer;
  auto self_shamir_share_table = std::make_unique<
      absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>();
  for (int i = 0; i < 4; ++i) {
    self_shamir_share_table->try_emplace(
        i, sharer.Share(
               3, 4,
               MakeAesKey(absl::StrCat("test 32 byte AES key for user #", i))));
  }

  auto impl =
      CreateSecAggServerProtocolImpl(input_vector_specs, sender.get(), metrics);
  impl->set_pairwise_shamir_share_table(
      std::make_unique<
          absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>());
  impl->set_self_shamir_share_table(std::move(self_shamir_share_table));

  SecAggServerPrngRunningState state(
      std::move(impl),
      0,   // number_of_clients_failed_after_sending_masked_input
      0,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  ClientToServerWrapperMessage client_message;
  client_message.mutable_abort();
  EXPECT_CALL(*metrics,
              MessageReceivedSizes(
                  Eq(ClientToServerWrapperMessage::MessageContentCase::kAbort),
                  Eq(false), Eq(client_message.ByteSizeLong())));
  EXPECT_CALL(*metrics, ClientsDropped(_, _)).Times(0);
  EXPECT_CALL(*sender, Send(Eq(0), _)).Times(0);

  EXPECT_THAT(state.HandleMessage(0, client_message), IsOk());
  EXPECT_THAT(state.NumberOfClientsFailedAfterSendingMaskedInput(), Eq(0));
  ASSERT_THAT(state.AbortedClientIds().contains(0), Eq(true));
  EXPECT_THAT(tracing_recorder.FindAllEvents<ClientMessageReceived>(),
              ElementsAre(IsEvent<ClientMessageReceived>(
                  Eq(ClientToServerMessageType_Abort),
                  Eq(client_message.ByteSizeLong()), Eq(false), Ge(0))));
}

TEST(SecaggServerPrngRunningStateTest,
     AbortReturnsValidStateAndNotifiesClients) {
  TestTracingRecorder tracing_recorder;
  auto input_vector_specs = std::vector<InputVectorSpecification>();
  input_vector_specs.push_back(InputVectorSpecification("foobar", 4, 32));
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();
  auto sender = std::make_unique<MockSendToClientsInterface>();
  FakePrng prng;
  ShamirSecretSharing sharer;
  auto self_shamir_share_table = std::make_unique<
      absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>();
  for (int i = 0; i < 4; ++i) {
    self_shamir_share_table->try_emplace(
        i, sharer.Share(
               3, 4,
               MakeAesKey(absl::StrCat("test 32 byte AES key for user #", i))));
  }

  auto impl =
      CreateSecAggServerProtocolImpl(input_vector_specs, sender.get(), metrics);
  impl->set_pairwise_shamir_share_table(
      std::make_unique<
          absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>());
  impl->set_self_shamir_share_table(std::move(self_shamir_share_table));

  SecAggServerPrngRunningState state(
      std::move(impl),
      0,   // number_of_clients_failed_after_sending_masked_input
      0,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  ServerToClientWrapperMessage abort_message;
  abort_message.mutable_abort()->set_early_success(false);
  abort_message.mutable_abort()->set_diagnostic_info("test abort reason");

  EXPECT_CALL(*metrics,
              ProtocolOutcomes(Eq(SecAggServerOutcome::UNHANDLED_ERROR)));
  EXPECT_CALL(*sender, Send(_, _))
      .Times(0);  // Except those specified in the following loop
  for (int i = 0; i < 4; ++i) {
    EXPECT_CALL(*sender, Send(i, EqualsProto(abort_message))).Times(1);
  }
  auto next_state =
      state.Abort("test abort reason", SecAggServerOutcome::UNHANDLED_ERROR);

  ASSERT_THAT(next_state->State(), Eq(SecAggServerStateKind::ABORTED));
  ASSERT_THAT(next_state->ErrorMessage().ok(), Eq(true));
  EXPECT_THAT(next_state->ErrorMessage().value(), Eq("test abort reason"));
  EXPECT_THAT(tracing_recorder.FindAllEvents<BroadcastMessageSent>(),
              ElementsAre(IsEvent<BroadcastMessageSent>(
                  Eq(ServerToClientMessageType_Abort),
                  Eq(abort_message.ByteSizeLong()))));
}

TEST(SecaggServerPrngRunningStateTest,
     PrngGetsRightMasksWhenAllClientsSurvive) {
  // First, set up necessary data for the SecAggServerPrngRunningState
  auto input_vector_specs = std::vector<InputVectorSpecification>();
  input_vector_specs.push_back(InputVectorSpecification("foobar", 4, 32));
  auto sender = std::make_unique<MockSendToClientsInterface>();
  FakePrng prng;
  ShamirSecretSharing sharer;
  auto self_shamir_share_table = std::make_unique<
      absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>();
  for (int i = 0; i < 4; ++i) {
    self_shamir_share_table->insert(std::make_pair(
        i, sharer.Share(3, 4,
                        MakeAesKey(absl::StrCat(
                            "test 32 byte AES key for user #", i)))));
  }

  // Generate the expected (negative) sum of masking vectors using MapofMasks.
  std::vector<AesKey> prng_keys_to_add;
  std::vector<AesKey> prng_keys_to_subtract;
  for (int i = 0; i < 4; ++i) {
    prng_keys_to_subtract.push_back(
        MakeAesKey(absl::StrCat("test 32 byte AES key for user #", i)));
  }
  auto session_id = MakeTestSessionId();
  auto expected_map_of_masks =
      MapOfMasks(prng_keys_to_add, prng_keys_to_subtract, input_vector_specs,
                 *session_id, AesCtrPrngFactory());

  auto impl = CreateSecAggServerProtocolImpl(input_vector_specs, sender.get());
  auto zero_map = std::make_unique<SecAggUnpackedVectorMap>();
  zero_map->emplace("foobar", SecAggUnpackedVector({0, 0, 0, 0}, 32));
  impl->set_masked_input(std::move(zero_map));
  impl->set_pairwise_shamir_share_table(
      std::make_unique<
          absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>());
  impl->set_self_shamir_share_table(std::move(self_shamir_share_table));

  SecAggServerPrngRunningState state(
      std::move(impl),
      0,   // number_of_clients_failed_after_sending_masked_input
      0,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  MockPrngDone prng_done;
  EXPECT_CALL(prng_done, Callback());

  state.EnterState();
  state.SetAsyncCallback([&]() { prng_done.Callback(); });

  EXPECT_THAT(state.ReadyForNextRound(), Eq(true));

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state.ok(), Eq(true));
  ASSERT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::COMPLETED));
  auto result = next_state.value()->Result();
  ASSERT_THAT(result.ok(), Eq(true));
  EXPECT_THAT(*result.value(),
              testing::MatchesSecAggVectorMap(*expected_map_of_masks));
}

TEST(SecaggServerPrngRunningStateTest,
     PrngGetsRightMasksWithOneDeadClientAfterSendingInput) {
  // In this test, client 1 died after sending its masked input. Its input will
  // still be included.
  //
  // First, set up necessary data for the SecAggServerPrngRunningState.
  auto input_vector_specs = std::vector<InputVectorSpecification>();
  input_vector_specs.push_back(InputVectorSpecification("foobar", 4, 32));
  auto sender = std::make_unique<MockSendToClientsInterface>();
  FakePrng prng;
  ShamirSecretSharing sharer;
  auto pairwise_shamir_share_table = std::make_unique<
      absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>();
  auto self_shamir_share_table = std::make_unique<
      absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>();

  auto aborted_client_ids = std::make_unique<absl::flat_hash_set<uint32_t>>();
  auto impl = CreateSecAggServerProtocolImpl(input_vector_specs, sender.get());
  impl->set_client_status(
      1, ClientStatus::DEAD_AFTER_MASKED_INPUT_RESPONSE_RECEIVED);

  aborted_client_ids->insert(1);

  for (int i = 0; i < 4; ++i) {
    self_shamir_share_table->insert(std::make_pair(
        i, sharer.Share(3, 4,
                        MakeAesKey(absl::StrCat(
                            "test 32 byte AES key for user #", i)))));
    // Blank out the share in position 1 because it would not have been sent.
    (*self_shamir_share_table)[i][1] = {""};
  }

  auto zero_map = std::make_unique<SecAggUnpackedVectorMap>();
  zero_map->insert(std::make_pair(
      "foobar", SecAggUnpackedVector(std::vector<uint64_t>{0, 0, 0, 0}, 32)));
  impl->set_masked_input(std::move(zero_map));
  impl->set_pairwise_shamir_share_table(std::move(pairwise_shamir_share_table));
  impl->set_self_shamir_share_table(std::move(self_shamir_share_table));

  // Generate the expected (negative) sum of masking vectors using MapofMasks.
  std::vector<AesKey> prng_keys_to_add;
  std::vector<AesKey> prng_keys_to_subtract;
  for (int i = 0; i < 4; ++i) {
    prng_keys_to_subtract.push_back(
        MakeAesKey(absl::StrCat("test 32 byte AES key for user #", i)));
  }
  auto session_id = MakeTestSessionId();
  auto expected_map_of_masks =
      MapOfMasks(prng_keys_to_add, prng_keys_to_subtract, input_vector_specs,
                 *session_id, AesCtrPrngFactory());

  SecAggServerPrngRunningState state(
      std::move(impl),
      1,   // number_of_clients_failed_after_sending_masked_input
      0,   // number_of_clients_failed_before_sending_masked_input
      1);  // number_of_clients_terminated_without_unmasking

  MockPrngDone prng_done;
  EXPECT_CALL(prng_done, Callback());

  state.EnterState();
  state.SetAsyncCallback([&]() { prng_done.Callback(); });

  EXPECT_THAT(state.ReadyForNextRound(), Eq(true));

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state.ok(), Eq(true));
  ASSERT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::COMPLETED));
  auto result = next_state.value()->Result();
  ASSERT_THAT(result.ok(), Eq(true));
  EXPECT_THAT(*result.value(),
              testing::MatchesSecAggVectorMap(*expected_map_of_masks));
}

TEST(SecaggServerPrngRunningStateTest,
     PrngGetsRightMasksWithOneDeadClientBeforeSendingInput) {
  // In this test, client 1 died before sending its masked input but after other
  // clients computed theirs, so its pairwise key will need to be canceled out.
  //
  // First, set up necessary data for the SecAggServerPrngRunningState.
  auto input_vector_specs = std::vector<InputVectorSpecification>();
  input_vector_specs.push_back(InputVectorSpecification("foobar", 4, 32));
  auto sender = std::make_unique<MockSendToClientsInterface>();
  FakePrng prng;
  ShamirSecretSharing sharer;
  auto pairwise_shamir_share_table = std::make_unique<
      absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>();
  auto self_shamir_share_table = std::make_unique<
      absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>();

  auto impl = CreateSecAggServerProtocolImpl(input_vector_specs, sender.get());
  impl->set_client_status(1, ClientStatus::DEAD_AFTER_SHARE_KEYS_RECEIVED);

  auto aborted_client_ids = std::make_unique<absl::flat_hash_set<uint32_t>>();
  aborted_client_ids->insert(1);

  EcdhPregeneratedTestKeys ecdh_keys;
  for (int i = 0; i < 4; ++i) {
    if (i == 1) {
      // Client 1 died in the previous step, so the other clients will have sent
      // shares of its pairwise key instead.
      pairwise_shamir_share_table->insert(
          std::make_pair(i, sharer.Share(3, 4, ecdh_keys.GetPrivateKey(i))));
      // Blank out the share in position 1 because it would not have been sent.
      (*pairwise_shamir_share_table)[i][1] = {""};
    } else {
      self_shamir_share_table->insert(std::make_pair(
          i, sharer.Share(3, 4,
                          MakeAesKey(absl::StrCat(
                              "test 32 byte AES key for user #", i)))));
      // Blank out the share in position 1 because it would not have been sent.
      (*self_shamir_share_table)[i][1] = {""};
    }
  }

  auto zero_map = std::make_unique<SecAggUnpackedVectorMap>();
  zero_map->emplace("foobar", SecAggUnpackedVector({0, 0, 0, 0}, 32));
  impl->set_masked_input(std::move(zero_map));
  impl->set_pairwise_shamir_share_table(std::move(pairwise_shamir_share_table));
  impl->set_self_shamir_share_table(std::move(self_shamir_share_table));

  // Generate the expected (negative) sum of masking vectors using MapofMasks.
  // We should subtract the self masks of clients 0, 2, and 3. We should
  // subtract the pairwise mask 2 and 3 added for 1, and add the pairwise mask
  // that 0 subtracted for 1.
  auto aborted_client_key_agreement =
      EcdhKeyAgreement::CreateFromPrivateKey(ecdh_keys.GetPrivateKey(1));
  std::vector<AesKey> prng_keys_to_add;
  std::vector<AesKey> prng_keys_to_subtract;
  for (int i = 0; i < 4; ++i) {
    if (i == 1) {
      continue;
    }
    prng_keys_to_subtract.push_back(
        MakeAesKey(absl::StrCat("test 32 byte AES key for user #", i)));
    AesKey pairwise_key = aborted_client_key_agreement.value()
                              ->ComputeSharedSecret(ecdh_keys.GetPublicKey(i))
                              .value();
    if (i == 0) {
      prng_keys_to_add.push_back(pairwise_key);
    } else {
      prng_keys_to_subtract.push_back(pairwise_key);
    }
  }
  auto session_id = MakeTestSessionId();
  auto expected_map_of_masks =
      MapOfMasks(prng_keys_to_add, prng_keys_to_subtract, input_vector_specs,
                 *session_id, AesCtrPrngFactory());

  SecAggServerPrngRunningState state(
      std::move(impl),
      0,   // number_of_clients_failed_after_sending_masked_input
      1,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  MockPrngDone prng_done;
  EXPECT_CALL(prng_done, Callback());

  state.EnterState();
  state.SetAsyncCallback([&]() { prng_done.Callback(); });

  EXPECT_THAT(state.ReadyForNextRound(), Eq(true));

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state.ok(), Eq(true));
  ASSERT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::COMPLETED));
  auto result = next_state.value()->Result();
  ASSERT_THAT(result.ok(), Eq(true));
  EXPECT_THAT(*result.value(),
              testing::MatchesSecAggVectorMap(*expected_map_of_masks));
}

TEST(SecaggServerPrngRunningStateTest,
     PrngGetsRightMasksAndCallsCallbackIfSpecified) {
  // In this test, there is now a callback that should be called when the PRNG
  // is done running.
  //
  // First, set up necessary data for the SecAggServerPrngRunningState.
  auto input_vector_specs = std::vector<InputVectorSpecification>();
  input_vector_specs.push_back(InputVectorSpecification("foobar", 4, 32));
  auto sender = std::make_unique<MockSendToClientsInterface>();
  FakePrng prng;
  ShamirSecretSharing sharer;
  auto pairwise_shamir_share_table = std::make_unique<
      absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>();
  auto self_shamir_share_table = std::make_unique<
      absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>();

  auto impl = CreateSecAggServerProtocolImpl(input_vector_specs, sender.get());
  impl->set_client_status(1, ClientStatus::DEAD_AFTER_SHARE_KEYS_RECEIVED);

  auto aborted_client_ids = std::make_unique<absl::flat_hash_set<uint32_t>>();
  aborted_client_ids->insert(1);

  EcdhPregeneratedTestKeys ecdh_keys;
  for (int i = 0; i < 4; ++i) {
    if (i == 1) {
      // Client 1 died in the previous step, so the other clients will have sent
      // shares of its pairwise key instead.
      pairwise_shamir_share_table->insert(
          std::make_pair(i, sharer.Share(3, 4, ecdh_keys.GetPrivateKey(i))));
      // Blank out the share in position 1 because it would not have been sent.
      (*pairwise_shamir_share_table)[i][1] = {""};
    } else {
      self_shamir_share_table->insert(std::make_pair(
          i, sharer.Share(3, 4,
                          MakeAesKey(absl::StrCat(
                              "test 32 byte AES key for user #", i)))));
      // Blank out the share in position 1 because it would not have been sent.
      (*self_shamir_share_table)[i][1] = {""};
    }
  }

  auto zero_map = std::make_unique<SecAggUnpackedVectorMap>();
  zero_map->emplace("foobar", SecAggUnpackedVector({0, 0, 0, 0}, 32));
  impl->set_masked_input(std::move(zero_map));
  impl->set_pairwise_shamir_share_table(std::move(pairwise_shamir_share_table));
  impl->set_self_shamir_share_table(std::move(self_shamir_share_table));

  // Generate the expected (negative) sum of masking vectors using MapofMasks.
  // We should subtract the self masks of clients 0, 2, and 3. We should
  // subtract the pairwise mask 2 and 3 added for 1, and add the pairwise mask
  // that 0 subtracted for 1.
  auto aborted_client_key_agreement =
      EcdhKeyAgreement::CreateFromPrivateKey(ecdh_keys.GetPrivateKey(1));
  std::vector<AesKey> prng_keys_to_add;
  std::vector<AesKey> prng_keys_to_subtract;
  for (int i = 0; i < 4; ++i) {
    if (i == 1) {
      continue;
    }
    prng_keys_to_subtract.push_back(
        MakeAesKey(absl::StrCat("test 32 byte AES key for user #", i)));
    AesKey pairwise_key = aborted_client_key_agreement.value()
                              ->ComputeSharedSecret(ecdh_keys.GetPublicKey(i))
                              .value();
    if (i == 0) {
      prng_keys_to_add.push_back(pairwise_key);
    } else {
      prng_keys_to_subtract.push_back(pairwise_key);
    }
  }
  auto session_id = MakeTestSessionId();
  auto expected_map_of_masks =
      MapOfMasks(prng_keys_to_add, prng_keys_to_subtract, input_vector_specs,
                 *session_id, AesCtrPrngFactory());

  SecAggServerPrngRunningState state(
      std::move(impl),
      0,   // number_of_clients_failed_after_sending_masked_input
      1,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  MockPrngDone prng_done;
  EXPECT_CALL(prng_done, Callback());

  state.EnterState();
  state.SetAsyncCallback([&]() { prng_done.Callback(); });

  EXPECT_THAT(state.ReadyForNextRound(), Eq(true));

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state.ok(), Eq(true));
  ASSERT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::COMPLETED));
  auto result = next_state.value()->Result();
  ASSERT_THAT(result.ok(), Eq(true));
  EXPECT_THAT(*result.value(),
              testing::MatchesSecAggVectorMap(*expected_map_of_masks));
}

TEST(SecaggServerPrngRunningStateTest, SetAsyncCallbackCanBeCalledTwice) {
  // StartPrng should have the property that it can be called after it has
  // already run successfully without any problems. It should just return OK
  // again.
  //
  // First, set up necessary data for the SecAggServerPrngRunningState.
  auto input_vector_specs = std::vector<InputVectorSpecification>();
  input_vector_specs.push_back(InputVectorSpecification("foobar", 4, 32));
  auto sender = std::make_unique<MockSendToClientsInterface>();
  FakePrng prng;
  ShamirSecretSharing sharer;
  auto pairwise_shamir_share_table = std::make_unique<
      absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>();
  auto self_shamir_share_table = std::make_unique<
      absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>();

  auto impl = CreateSecAggServerProtocolImpl(input_vector_specs, sender.get());
  impl->set_client_status(1, ClientStatus::DEAD_AFTER_SHARE_KEYS_RECEIVED);

  auto aborted_client_ids = std::make_unique<absl::flat_hash_set<uint32_t>>();
  aborted_client_ids->insert(1);

  EcdhPregeneratedTestKeys ecdh_keys;
  for (int i = 0; i < 4; ++i) {
    if (i == 1) {
      // Client 1 died in the previous step, so the other clients will have sent
      // shares of its pairwise key instead.
      pairwise_shamir_share_table->insert(
          std::make_pair(i, sharer.Share(3, 4, ecdh_keys.GetPrivateKey(i))));
      // Blank out the share in position 1 because it would not have been sent.
      (*pairwise_shamir_share_table)[i][1] = {""};
    } else {
      self_shamir_share_table->insert(std::make_pair(
          i, sharer.Share(3, 4,
                          MakeAesKey(absl::StrCat(
                              "test 32 byte AES key for user #", i)))));
      // Blank out the share in position 1 because it would not have been sent.
      (*self_shamir_share_table)[i][1] = {""};
    }
  }

  auto zero_map = std::make_unique<SecAggUnpackedVectorMap>();
  zero_map->emplace("foobar", SecAggUnpackedVector({0, 0, 0, 0}, 32));
  impl->set_masked_input(std::move(zero_map));
  impl->set_pairwise_shamir_share_table(std::move(pairwise_shamir_share_table));
  impl->set_self_shamir_share_table(std::move(self_shamir_share_table));

  // Generate the expected (negative) sum of masking vectors using MapofMasks.
  // We should subtract the self masks of clients 0, 2, and 3. We should
  // subtract the pairwise mask 2 and 3 added for 1, and add the pairwise mask
  // that 0 subtracted for 1.
  auto aborted_client_key_agreement =
      EcdhKeyAgreement::CreateFromPrivateKey(ecdh_keys.GetPrivateKey(1));
  std::vector<AesKey> prng_keys_to_add;
  std::vector<AesKey> prng_keys_to_subtract;
  for (int i = 0; i < 4; ++i) {
    if (i == 1) {
      continue;
    }
    prng_keys_to_subtract.push_back(
        MakeAesKey(absl::StrCat("test 32 byte AES key for user #", i)));
    AesKey pairwise_key = aborted_client_key_agreement.value()
                              ->ComputeSharedSecret(ecdh_keys.GetPublicKey(i))
                              .value();
    if (i == 0) {
      prng_keys_to_add.push_back(pairwise_key);
    } else {
      prng_keys_to_subtract.push_back(pairwise_key);
    }
  }
  auto session_id = MakeTestSessionId();
  auto expected_map_of_masks =
      MapOfMasks(prng_keys_to_add, prng_keys_to_subtract, input_vector_specs,
                 *session_id, AesCtrPrngFactory());

  SecAggServerPrngRunningState state(
      std::move(impl),
      0,   // number_of_clients_failed_after_sending_masked_input
      1,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  MockPrngDone prng_done;
  EXPECT_CALL(prng_done, Callback());

  state.EnterState();
  state.SetAsyncCallback([&]() { prng_done.Callback(); });

  EXPECT_THAT(state.ReadyForNextRound(), Eq(true));

  // Make sure we can call SetAsyncCallback again.
  MockPrngDone prng_done_2;
  EXPECT_CALL(prng_done_2, Callback());
  state.SetAsyncCallback([&]() { prng_done_2.Callback(); });

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state.ok(), Eq(true));
  ASSERT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::COMPLETED));
  auto result = next_state.value()->Result();
  ASSERT_THAT(result.ok(), Eq(true));
  EXPECT_THAT(*result.value(),
              testing::MatchesSecAggVectorMap(*expected_map_of_masks));
}

TEST(SecaggServerPrngRunningStateTest,
     PrngGetsRightMasksWhenClientsUse16BSelfKeys) {
  // TODO(team): This test is only for ensuring Java compatibility.
  // First, set up necessary data for the SecAggServerPrngRunningState
  auto input_vector_specs = std::vector<InputVectorSpecification>();
  input_vector_specs.push_back(InputVectorSpecification("foobar", 4, 32));
  auto sender = std::make_unique<MockSendToClientsInterface>();
  FakePrng prng;
  ShamirSecretSharing sharer;
  auto self_shamir_share_table = std::make_unique<
      absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>();
  for (int i = 0; i < 4; ++i) {
    self_shamir_share_table->insert(std::make_pair(
        i, sharer.Share(3, 4,
                        AesKey(reinterpret_cast<const uint8_t*>(
                                   absl::StrCat("16B key of user", i).c_str()),
                               16))));
  }

  // Generate the expected (negative) sum of masking vectors using MapofMasks.
  std::vector<AesKey> prng_keys_to_add;
  std::vector<AesKey> prng_keys_to_subtract;
  for (int i = 0; i < 4; ++i) {
    prng_keys_to_subtract.push_back(
        AesKey(reinterpret_cast<const uint8_t*>(
                   absl::StrCat("16B key of user", i).c_str()),
               16));
  }
  auto session_id = MakeTestSessionId();
  auto expected_map_of_masks =
      MapOfMasks(prng_keys_to_add, prng_keys_to_subtract, input_vector_specs,
                 *session_id, AesCtrPrngFactory());

  auto impl = CreateSecAggServerProtocolImpl(input_vector_specs, sender.get());
  auto zero_map = std::make_unique<SecAggUnpackedVectorMap>();
  zero_map->emplace("foobar", SecAggUnpackedVector({0, 0, 0, 0}, 32));
  impl->set_masked_input(std::move(zero_map));
  impl->set_pairwise_shamir_share_table(
      std::make_unique<
          absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>());
  impl->set_self_shamir_share_table(std::move(self_shamir_share_table));

  SecAggServerPrngRunningState state(
      std::move(impl),
      0,   // number_of_clients_failed_after_sending_masked_input
      0,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  MockPrngDone prng_done;
  EXPECT_CALL(prng_done, Callback());

  state.EnterState();
  state.SetAsyncCallback([&]() { prng_done.Callback(); });

  EXPECT_THAT(state.ReadyForNextRound(), Eq(true));

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state.ok(), Eq(true));
  ASSERT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::COMPLETED));
  auto result = next_state.value()->Result();
  ASSERT_THAT(result.ok(), Eq(true));
  EXPECT_THAT(*result.value(),
              testing::MatchesSecAggVectorMap(*expected_map_of_masks));
}

TEST(SecaggServerPrngRunningStateTest, TimingMetricsAreRecorded) {
  // First, set up necessary data for the SecAggServerPrngRunningState
  TestTracingRecorder tracing_recorder;
  auto input_vector_specs = std::vector<InputVectorSpecification>();
  input_vector_specs.push_back(InputVectorSpecification("foobar", 4, 32));
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();
  auto sender = std::make_unique<MockSendToClientsInterface>();
  FakePrng prng;
  ShamirSecretSharing sharer;
  auto self_shamir_share_table = std::make_unique<
      absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>();
  for (int i = 0; i < 4; ++i) {
    self_shamir_share_table->insert(std::make_pair(
        i, sharer.Share(3, 4,
                        MakeAesKey(absl::StrCat(
                            "test 32 byte AES key for user #", i)))));
  }

  auto impl =
      CreateSecAggServerProtocolImpl(input_vector_specs, sender.get(), metrics);
  auto zero_map = std::make_unique<SecAggUnpackedVectorMap>();
  zero_map->emplace("foobar", SecAggUnpackedVector({0, 0, 0, 0}, 32));
  impl->set_masked_input(std::move(zero_map));
  impl->set_pairwise_shamir_share_table(
      std::make_unique<
          absl::flat_hash_map<uint32_t, std::vector<ShamirShare>>>());
  impl->set_self_shamir_share_table(std::move(self_shamir_share_table));

  SecAggServerPrngRunningState state(
      std::move(impl),
      0,   // number_of_clients_failed_after_sending_masked_input
      0,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  MockPrngDone prng_done;
  EXPECT_CALL(prng_done, Callback());

  EXPECT_CALL(*metrics, PrngExpansionTimes(Ge(0)));
  EXPECT_CALL(*metrics, RoundTimes(Eq(SecAggServerStateKind::PRNG_RUNNING),
                                   Eq(true), Ge(0)));
  EXPECT_CALL(*metrics, ShamirReconstructionTimes(Ge(0)));

  state.EnterState();
  state.SetAsyncCallback([&]() { prng_done.Callback(); });
  EXPECT_THAT(state.ReadyForNextRound(), Eq(true));

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state.ok(), Eq(true));
  ASSERT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::COMPLETED));
  EXPECT_THAT(tracing_recorder.FindAllEvents<ShamirReconstruction>(),
              ElementsAre(IsEvent<ShamirReconstruction>(Ge(0))));
  EXPECT_THAT(tracing_recorder.FindAllEvents<PrngExpansion>(),
              ElementsAre(IsEvent<PrngExpansion>(Ge(0))));
}

}  // namespace
}  // namespace secagg
}  // namespace fcp
