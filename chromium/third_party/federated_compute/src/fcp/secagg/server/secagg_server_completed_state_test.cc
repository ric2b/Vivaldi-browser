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

#include "fcp/secagg/server/secagg_server_completed_state.h"

#include <memory>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/node_hash_set.h"
#include "fcp/base/monitoring.h"
#include "fcp/secagg/server/aes/aes_secagg_server_protocol_impl.h"
#include "fcp/secagg/server/secagg_server_enums.pb.h"
#include "fcp/secagg/server/secret_sharing_graph_factory.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/secagg/testing/server/mock_secagg_server_metrics_listener.h"
#include "fcp/secagg/testing/server/mock_send_to_clients_interface.h"
#include "fcp/secagg/testing/test_matchers.h"
#include "fcp/tracing/test_tracing_recorder.h"

namespace fcp {
namespace secagg {
namespace {

using ::testing::Eq;

std::unique_ptr<AesSecAggServerProtocolImpl> CreateSecAggServerProtocolImpl(
    MockSendToClientsInterface* sender,
    MockSecAggServerMetricsListener* metrics_listener = nullptr) {
  int total_number_of_clients = 4;
  SecretSharingGraphFactory factory;
  return std::make_unique<AesSecAggServerProtocolImpl>(
      factory.CreateCompleteGraph(total_number_of_clients, 3),
      3,  // minimum_number_of_clients_to_proceed
      std::vector<InputVectorSpecification>(),
      std::unique_ptr<MockSecAggServerMetricsListener>(metrics_listener),
      nullptr,  // prng_factory
      sender,
      nullptr,  // prng_runner
      std::vector<ClientStatus>(total_number_of_clients,
                                ClientStatus::UNMASKING_RESPONSE_RECEIVED),
      ServerVariant::NATIVE_V1);
}

SecAggServerCompletedState CreateState(
    MockSendToClientsInterface* sender,
    int number_of_clients_failed_after_sending_masked_input = 0,
    int number_of_clients_failed_before_sending_masked_input = 0,
    int number_of_clients_terminated_without_unmasking = 0,
    std::unique_ptr<SecAggVectorMap> map = std::unique_ptr<SecAggVectorMap>(),
    MockSecAggServerMetricsListener* metrics_listener = nullptr) {
  std::unique_ptr<AesSecAggServerProtocolImpl> impl =
      CreateSecAggServerProtocolImpl(sender, metrics_listener);
  impl->SetResult(std::move(map));
  return SecAggServerCompletedState(
      std::move(impl), number_of_clients_failed_after_sending_masked_input,
      number_of_clients_failed_before_sending_masked_input,
      number_of_clients_terminated_without_unmasking);
}

TEST(SecAggServerCompletedStateTest, IsAbortedReturnsFalse) {
  auto sender = std::make_unique<MockSendToClientsInterface>();
  SecAggServerCompletedState completed_state = CreateState(sender.get());
  EXPECT_THAT(completed_state.IsAborted(), Eq(false));
}

TEST(SecAggServerCompletedStateTest, IsCompletedSuccessfullyReturnsTrue) {
  auto sender = std::make_unique<MockSendToClientsInterface>();
  SecAggServerCompletedState completed_state = CreateState(sender.get());
  EXPECT_THAT(completed_state.IsCompletedSuccessfully(), Eq(true));
}

TEST(SecAggServerCompletedStateTest, ErrorMessageRaisesError) {
  auto sender = std::make_unique<MockSendToClientsInterface>();
  SecAggServerCompletedState completed_state = CreateState(sender.get());
  EXPECT_THAT(completed_state.ErrorMessage().ok(), Eq(false));
}

TEST(SecAggServerCompletedStateTest, ReadyForNextRoundReturnsFalse) {
  auto sender = std::make_unique<MockSendToClientsInterface>();
  SecAggServerCompletedState completed_state = CreateState(sender.get());
  EXPECT_THAT(completed_state.ReadyForNextRound(), Eq(false));
}

TEST(SecAggServerCompletedStateTest,
     NumberOfMessagesReceivedInThisRoundReturnsZero) {
  auto sender = std::make_unique<MockSendToClientsInterface>();
  SecAggServerCompletedState completed_state = CreateState(sender.get());
  EXPECT_THAT(completed_state.NumberOfMessagesReceivedInThisRound(), Eq(0));
}

TEST(SecAggServerCompletedStateTest,
     NumberOfClientsReadyForNextRoundReturnsZero) {
  auto sender = std::make_unique<MockSendToClientsInterface>();
  SecAggServerCompletedState completed_state = CreateState(sender.get());
  EXPECT_THAT(completed_state.NumberOfClientsReadyForNextRound(), Eq(0));
}

TEST(SecAggServerCompletedStateTest, NumberOfAliveClientsIsAccurate) {
  auto sender = std::make_unique<MockSendToClientsInterface>();
  SecAggServerCompletedState completed_state = CreateState(
      sender.get(), 0,  // number_of_clients_failed_after_sending_masked_input
      0,                // number_of_clients_failed_before_sending_masked_input
      1);               // number_of_clients_terminated_without_unmasking
  EXPECT_THAT(completed_state.NumberOfAliveClients(), Eq(3));
}

TEST(SecAggServerCompletedStateTest,
     NumberOfClientsFailedBeforeSendingMaskedInputIsAccurate) {
  auto sender = std::make_unique<MockSendToClientsInterface>();
  SecAggServerCompletedState completed_state = CreateState(
      sender.get(), 0,  // number_of_clients_failed_after_sending_masked_input
      1,                // number_of_clients_failed_before_sending_masked_input
      0);               // number_of_clients_terminated_without_unmasking
  EXPECT_THAT(completed_state.NumberOfClientsFailedBeforeSendingMaskedInput(),
              Eq(1));
}

TEST(SecAggServerCompletedStateTest,
     NumberOfClientsFailedAfterSendingMaskedInputIsAccurate) {
  auto sender = std::make_unique<MockSendToClientsInterface>();
  SecAggServerCompletedState completed_state = CreateState(
      sender.get(), 1,  // number_of_clients_failed_after_sending_masked_input
      0,                // number_of_clients_failed_before_sending_masked_input
      0);               // number_of_clients_terminated_without_unmasking
  EXPECT_THAT(completed_state.NumberOfClientsFailedAfterSendingMaskedInput(),
              Eq(1));
}

TEST(SecAggServerCompletedStateTest,
     NumberOfClientsTerminatedWithoutUnmaskingIsAccurate) {
  auto sender = std::make_unique<MockSendToClientsInterface>();
  SecAggServerCompletedState completed_state = CreateState(
      sender.get(), 0,  // number_of_clients_failed_after_sending_masked_input
      0,                // number_of_clients_failed_before_sending_masked_input
      1);               // number_of_clients_terminated_without_unmasking
  EXPECT_THAT(completed_state.NumberOfClientsTerminatedWithoutUnmasking(),
              Eq(1));
}

TEST(SecAggServerCompletedStateTest, NumberOfPendingClientsReturnsZero) {
  auto sender = std::make_unique<MockSendToClientsInterface>();
  SecAggServerCompletedState completed_state = CreateState(sender.get());
  EXPECT_THAT(completed_state.NumberOfPendingClients(), Eq(0));
}

TEST(SecAggServerCompletedStateTest, NumberOfIncludedInputsIsAccurate) {
  auto sender = std::make_unique<MockSendToClientsInterface>();
  SecAggServerCompletedState completed_state = CreateState(
      sender.get(), 1,  // number_of_clients_failed_after_sending_masked_input
      0,                // number_of_clients_failed_before_sending_masked_input
      0);               // number_of_clients_terminated_without_unmasking
  EXPECT_THAT(completed_state.NumberOfIncludedInputs(), Eq(4));

  SecAggServerCompletedState completed_state_2 = CreateState(
      sender.get(), 0,  // number_of_clients_failed_after_sending_masked_input
      1,                // number_of_clients_failed_before_sending_masked_input
      0);               // number_of_clients_terminated_without_unmasking
  EXPECT_THAT(completed_state_2.NumberOfIncludedInputs(), Eq(3));
}

TEST(SecAggServerCompletedStateTest,
     IsNumberOfIncludedInputsCommittedReturnsTrue) {
  auto sender = std::make_unique<MockSendToClientsInterface>();
  SecAggServerCompletedState completed_state = CreateState(sender.get());
  EXPECT_THAT(completed_state.IsNumberOfIncludedInputsCommitted(), Eq(true));
}

TEST(SecAggServerCompletedStateTest,
     MinimumMessagesNeededForNextRoundReturnsZero) {
  auto sender = std::make_unique<MockSendToClientsInterface>();
  SecAggServerCompletedState completed_state = CreateState(sender.get());
  EXPECT_THAT(completed_state.MinimumMessagesNeededForNextRound(), Eq(0));
}

TEST(SecAggServerCompletedStateTest,
     MinimumNumberOfClientsToProceedIsAccurate) {
  auto sender = std::make_unique<MockSendToClientsInterface>();
  SecAggServerCompletedState completed_state = CreateState(sender.get());
  EXPECT_THAT(completed_state.minimum_number_of_clients_to_proceed(), Eq(3));
}

TEST(SecAggServerCompletedStateTest, HandleMessageRaisesError) {
  auto sender = std::make_unique<MockSendToClientsInterface>();
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();

  SecAggServerCompletedState completed_state = CreateState(
      sender.get(), 0,  // number_of_clients_failed_after_sending_masked_input
      0,                // number_of_clients_failed_before_sending_masked_input
      0,                // number_of_clients_terminated_without_unmasking
      std::unique_ptr<SecAggVectorMap>(), metrics);

  ClientToServerWrapperMessage client_message;
  EXPECT_CALL(*metrics, MessageReceivedSizes(
                            Eq(ClientToServerWrapperMessage::
                                   MessageContentCase::MESSAGE_CONTENT_NOT_SET),
                            Eq(false), Eq(client_message.ByteSizeLong())));
  EXPECT_THAT(completed_state.HandleMessage(0, client_message).ok(), Eq(false));
}

TEST(SecAggServerCompletedStateTest, ProceedToNextRoundRaisesError) {
  auto sender = std::make_unique<MockSendToClientsInterface>();
  SecAggServerCompletedState completed_state = CreateState(sender.get());
  EXPECT_THAT(completed_state.ProceedToNextRound().ok(), Eq(false));
}

TEST(SecAggServerCompletedStateTest, ResultGivesStoredResult) {
  std::vector<uint64_t> vec = {1, 3, 6, 10};
  auto result_map = std::make_unique<SecAggVectorMap>();
  auto sender = std::make_unique<MockSendToClientsInterface>();
  result_map->emplace("foobar", SecAggVector(vec, 32));
  SecAggServerCompletedState completed_state =
      CreateState(sender.get(),
                  0,  // number_of_clients_failed_after_sending_masked_input
                  0,  // number_of_clients_failed_before_sending_masked_input
                  0,  // number_of_clients_terminated_without_unmasking
                  std::move(result_map));

  auto result = completed_state.Result();
  ASSERT_THAT(result.ok(), Eq(true));
  EXPECT_THAT(*result.value(),
              testing::MatchesSecAggVector("foobar", SecAggVector(vec, 32)));
}

TEST(SecAggServerCompletedStateTest, ConstructorRecordsSuccessMetric) {
  TestTracingRecorder tracing_recorder;
  auto sender = std::make_unique<MockSendToClientsInterface>();
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();

  EXPECT_CALL(*metrics, ProtocolOutcomes(Eq(SecAggServerOutcome::SUCCESS)));
  SecAggServerCompletedState completed_state =
      CreateState(sender.get(),
                  0,  // number_of_clients_failed_after_sending_masked_input
                  0,  // number_of_clients_failed_before_sending_masked_input
                  0,  // number_of_clients_terminated_without_unmasking
                  std::unique_ptr<SecAggVectorMap>(), metrics);

  EXPECT_THAT(tracing_recorder.FindAllEvents<SecAggProtocolOutcome>(),
              ElementsAre(IsEvent<SecAggProtocolOutcome>(
                  Eq(TracingSecAggServerOutcome_Success))));
}

}  // namespace
}  // namespace secagg
}  // namespace fcp
