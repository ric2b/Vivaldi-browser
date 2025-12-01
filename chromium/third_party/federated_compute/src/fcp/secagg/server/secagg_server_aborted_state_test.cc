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

#include "fcp/secagg/server/secagg_server_aborted_state.h"

#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/node_hash_set.h"
#include "fcp/base/monitoring.h"
#include "fcp/secagg/server/aes/aes_secagg_server_protocol_impl.h"
#include "fcp/secagg/server/secret_sharing_graph_factory.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/secagg/testing/server/mock_secagg_server_metrics_listener.h"
#include "fcp/secagg/testing/server/mock_send_to_clients_interface.h"
#include "fcp/tracing/test_tracing_recorder.h"

namespace fcp {
namespace secagg {
namespace {

using ::testing::Eq;

std::unique_ptr<AesSecAggServerProtocolImpl> CreateSecAggServerProtocolImpl(
    MockSecAggServerMetricsListener* metrics_listener = nullptr) {
  auto sender = std::unique_ptr<SendToClientsInterface>();
  SecretSharingGraphFactory factory;
  return std::make_unique<AesSecAggServerProtocolImpl>(
      factory.CreateCompleteGraph(4, 3),  // total number of clients is 4
      3,  // minimum_number_of_clients_to_proceed,
      std::vector<InputVectorSpecification>(),  // input_vector_specs
      std::unique_ptr<MockSecAggServerMetricsListener>(metrics_listener),
      nullptr,  // prng_factory
      sender.get(),
      nullptr,  // prng_runner
      std::vector<ClientStatus>(
          4, DEAD_AFTER_SHARE_KEYS_RECEIVED),  // client_statuses
      ServerVariant::NATIVE_V1);
}

TEST(SecaggServerAbortedStateTest, IsAbortedReturnsTrue) {
  std::string test_error_message = "test error message";

  SecAggServerAbortedState aborted_state(
      test_error_message, CreateSecAggServerProtocolImpl(),
      0,  // number_of_clients_failed_after_sending_masked_input
      4,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_THAT(aborted_state.IsAborted(), Eq(true));
}

TEST(SecaggServerAbortedStateTest, IsCompletedSuccessfullyReturnsFalse) {
  std::string test_error_message = "test error message";

  SecAggServerAbortedState aborted_state(
      test_error_message, CreateSecAggServerProtocolImpl(),
      0,  // number_of_clients_failed_after_sending_masked_input
      4,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_THAT(aborted_state.IsCompletedSuccessfully(), Eq(false));
}

TEST(SecaggServerAbortedStateTest, ErrorMessageReturnsSelectedMessage) {
  std::string test_error_message = "test error message";

  SecAggServerAbortedState aborted_state(
      test_error_message, CreateSecAggServerProtocolImpl(),
      0,  // number_of_clients_failed_after_sending_masked_input
      4,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_THAT(aborted_state.ErrorMessage().value(), Eq(test_error_message));
}

TEST(SecaggServerAbortedStateTest, ReadyForNextRoundReturnsFalse) {
  std::string test_error_message = "test error message";

  SecAggServerAbortedState aborted_state(
      test_error_message, CreateSecAggServerProtocolImpl(),
      0,  // number_of_clients_failed_after_sending_masked_input
      4,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_THAT(aborted_state.ReadyForNextRound(), Eq(false));
}

TEST(SecaggServerAbortedStateTest,
     NumberOfMessagesReceivedInThisRoundReturnsZero) {
  std::string test_error_message = "test error message";

  SecAggServerAbortedState aborted_state(
      test_error_message, CreateSecAggServerProtocolImpl(),
      0,  // number_of_clients_failed_after_sending_masked_input
      4,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_THAT(aborted_state.NumberOfMessagesReceivedInThisRound(), Eq(0));
}

TEST(SecaggServerAbortedStateTest,
     NumberOfClientsReadyForNextRoundReturnsZero) {
  std::string test_error_message = "test error message";

  SecAggServerAbortedState aborted_state(
      test_error_message, CreateSecAggServerProtocolImpl(),
      0,  // number_of_clients_failed_after_sending_masked_input
      4,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_THAT(aborted_state.NumberOfClientsReadyForNextRound(), Eq(0));
}

TEST(SecaggServerAbortedStateTest, NumberOfAliveClientsIsZero) {
  std::string test_error_message = "test error message";

  SecAggServerAbortedState aborted_state(
      test_error_message, CreateSecAggServerProtocolImpl(),
      0,  // number_of_clients_failed_after_sending_masked_input
      4,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_THAT(aborted_state.NumberOfAliveClients(), Eq(0));
}

TEST(SecaggServerAbortedStateTest,
     NumberOfClientsFailedBeforeSendingMaskedInputIsAccurate) {
  std::string test_error_message = "test error message";

  SecAggServerAbortedState aborted_state(
      test_error_message, CreateSecAggServerProtocolImpl(),
      0,  // number_of_clients_failed_after_sending_masked_input
      4,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_THAT(aborted_state.NumberOfClientsFailedBeforeSendingMaskedInput(),
              Eq(4));
}

TEST(SecaggServerAbortedStateTest,
     NumberOfClientsFailedAfterSendingMaskedInputReturnsZero) {
  std::string test_error_message = "test error message";

  SecAggServerAbortedState aborted_state(
      test_error_message, CreateSecAggServerProtocolImpl(),
      0,  // number_of_clients_failed_after_sending_masked_input
      4,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_THAT(aborted_state.NumberOfClientsFailedAfterSendingMaskedInput(),
              Eq(0));
}

TEST(SecaggServerAbortedStateTest,
     NumberOfClientsTerminatedWithoutUnmaskingReturnsZero) {
  std::string test_error_message = "test error message";

  SecAggServerAbortedState aborted_state(
      test_error_message, CreateSecAggServerProtocolImpl(),
      0,  // number_of_clients_failed_after_sending_masked_input
      4,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_THAT(aborted_state.NumberOfClientsTerminatedWithoutUnmasking(), Eq(0));
}

TEST(SecaggServerAbortedStateTest, NumberOfPendingClientsReturnsZero) {
  std::string test_error_message = "test error message";

  SecAggServerAbortedState aborted_state(
      test_error_message, CreateSecAggServerProtocolImpl(),
      0,  // number_of_clients_failed_after_sending_masked_input
      4,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_THAT(aborted_state.NumberOfPendingClients(), Eq(0));
}

TEST(SecaggServerAbortedStateTest, NumberOfIncludedInputsReturnsZero) {
  std::string test_error_message = "test error message";

  SecAggServerAbortedState aborted_state(
      test_error_message, CreateSecAggServerProtocolImpl(),
      0,  // number_of_clients_failed_after_sending_masked_input
      4,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_THAT(aborted_state.NumberOfIncludedInputs(), Eq(0));
}

TEST(SecaggServerAbortedStateTest,
     IsNumberOfIncludedInputsCommittedReturnsTrue) {
  std::string test_error_message = "test error message";

  SecAggServerAbortedState aborted_state(
      test_error_message, CreateSecAggServerProtocolImpl(),
      0,  // number_of_clients_failed_after_sending_masked_input
      4,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_THAT(aborted_state.IsNumberOfIncludedInputsCommitted(), Eq(true));
}

TEST(SecaggServerAbortedStateTest,
     MinimumMessagesNeededForNextRoundReturnsZero) {
  std::string test_error_message = "test error message";

  SecAggServerAbortedState aborted_state(
      test_error_message, CreateSecAggServerProtocolImpl(),
      0,  // number_of_clients_failed_after_sending_masked_input
      4,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_THAT(aborted_state.MinimumMessagesNeededForNextRound(), Eq(0));
}

TEST(SecaggServerAbortedStateTest,
     minimum_number_of_clients_to_proceedIsAccurate) {
  std::string test_error_message = "test error message";

  SecAggServerAbortedState aborted_state(
      test_error_message, CreateSecAggServerProtocolImpl(),
      0,  // number_of_clients_failed_after_sending_masked_input
      4,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_THAT(aborted_state.minimum_number_of_clients_to_proceed(), Eq(3));
}

TEST(SecaggServerAbortedStateTest, HandleMessageRaisesError) {
  std::string test_error_message = "test error message";
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();

  SecAggServerAbortedState aborted_state(
      test_error_message, CreateSecAggServerProtocolImpl(metrics),
      0,  // number_of_clients_failed_after_sending_masked_input
      4,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  ClientToServerWrapperMessage client_message;
  EXPECT_CALL(*metrics, MessageReceivedSizes(
                            Eq(ClientToServerWrapperMessage::
                                   MessageContentCase::MESSAGE_CONTENT_NOT_SET),
                            Eq(false), Eq(client_message.ByteSizeLong())));
  EXPECT_THAT(aborted_state.HandleMessage(0, client_message).ok(), Eq(false));
}

TEST(SecaggServerAbortedStateTest, ProceedToNextRoundRaisesError) {
  std::string test_error_message = "test error message";

  SecAggServerAbortedState aborted_state(
      test_error_message, CreateSecAggServerProtocolImpl(),
      0,  // number_of_clients_failed_after_sending_masked_input
      4,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_THAT(aborted_state.ProceedToNextRound().ok(), Eq(false));
}

TEST(SecaggServerAbortedStateTest, ResultRaisesErrorStatus) {
  std::string test_error_message = "test error message";

  SecAggServerAbortedState aborted_state(
      test_error_message, CreateSecAggServerProtocolImpl(),
      0,  // number_of_clients_failed_after_sending_masked_input
      4,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_THAT(aborted_state.Result().ok(), Eq(false));
}

}  // namespace
}  // namespace secagg
}  // namespace fcp
