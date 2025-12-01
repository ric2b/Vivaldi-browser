/*
 * Copyright 2019 Google LLC
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

#include "fcp/secagg/server/secagg_server.h"

#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "fcp/testing/parse_text_proto.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "fcp/base/monitoring.h"
#include "fcp/secagg/server/secagg_server_enums.pb.h"
#include "fcp/secagg/server/secagg_server_state.h"
#include "fcp/secagg/server/tracing_schema.h"
#include "fcp/secagg/shared/aes_ctr_prng_factory.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/secagg/testing/ecdh_pregenerated_test_keys.h"
#include "fcp/secagg/testing/server/mock_secagg_server_metrics_listener.h"
#include "fcp/secagg/testing/server/mock_send_to_clients_interface.h"
#include "fcp/secagg/testing/server/test_secagg_experiments.h"
#include "fcp/testing/testing.h"
#include "fcp/tracing/test_tracing_recorder.h"

namespace fcp {
namespace secagg {
namespace {

using ::testing::_;
using ::testing::Eq;

std::unique_ptr<SecAggServer> CreateServer(
    SendToClientsInterface* sender,
    SecAggServerMetricsListener* metrics =
        new MockSecAggServerMetricsListener(),
    std::unique_ptr<TestSecAggExperiment> experiments =
        std::make_unique<TestSecAggExperiment>()) {
  SecureAggregationRequirements threat_model;
  threat_model.set_adversary_class(AdversaryClass::CURIOUS_SERVER);
  threat_model.set_adversarial_client_rate(.3);
  threat_model.set_estimated_dropout_rate(.3);
  std::unique_ptr<AesPrngFactory> prng_factory;
  std::vector<InputVectorSpecification> input_vector_specs;
  input_vector_specs.push_back(InputVectorSpecification("foobar", 4, 32));
  auto status_or_server = SecAggServer::Create(
      100,   // minimum_number_of_clients_to_proceed
      1000,  // total_number_of_clients
      input_vector_specs, sender,
      std::unique_ptr<SecAggServerMetricsListener>(metrics),
      /*prng_runner=*/nullptr, std::move(experiments), threat_model);
  EXPECT_THAT(status_or_server.ok(), true) << status_or_server.status();
  return std::move(status_or_server.value());
}

template <typename... M>
auto TraceRecorderHas(const M&... matchers) {
  return ElementsAre(AllOf(
      IsSpan<CreateSecAggServer>(),
      ElementsAre(
          IsEvent<SubGraphServerParameters>(
              1000,    // number_of_clients
              219,     // degree
              116,     // threshold
              700,     // minimum_number_of_clients_to_proceed
              false),  // is_r2_async_aggregation_enabled
          AllOf(IsSpan<SecureAggServerSession>(), ElementsAre(matchers...)))));
}

TEST(SecaggServerTest, ConstructedWithCorrectState) {
  TestTracingRecorder tracing_recorder;
  auto sender = std::make_unique<MockSendToClientsInterface>();
  auto server = CreateServer(sender.get());

  EXPECT_THAT(server->IsAborted(), Eq(false));
  EXPECT_THAT(server->NumberOfNeighbors(), Eq(219));
  EXPECT_THAT(server->IsCompletedSuccessfully(), Eq(false));
  EXPECT_THAT(server->State(), Eq(SecAggServerStateKind::R0_ADVERTISE_KEYS));
  EXPECT_THAT(tracing_recorder.root(),
              TraceRecorderHas(IsSpan<SecureAggServerState>(
                  SecAggServerTraceState_R0AdvertiseKeys)));
}

TEST(SecaggServerTest, FullgraphSecAggExperimentTakesEffect) {
  // Tests FullgraphSecAggExperiment by instatiating
  // a server under that experiment , and
  // checking that it results in the expected number of neighbors for the given
  // setting (1000 clients) and threat model (.3 dropout rate and .3 adversarial
  // client rate).
  SecureAggregationRequirements threat_model;
  threat_model.set_adversary_class(AdversaryClass::CURIOUS_SERVER);
  threat_model.set_adversarial_client_rate(.3);
  threat_model.set_estimated_dropout_rate(.3);
  std::unique_ptr<AesPrngFactory> prng_factory;
  std::vector<InputVectorSpecification> input_vector_specs;
  input_vector_specs.push_back(InputVectorSpecification("foobar", 4, 32));
  auto sender = std::make_unique<MockSendToClientsInterface>();
  std::set<std::string> experiment_names = {kFullgraphSecAggExperiment};
  auto status_or_server = SecAggServer::Create(
      100,   // minimum_number_of_clients_to_proceed
      1000,  // total_number_of_clients
      input_vector_specs, sender.get(),
      std::unique_ptr<SecAggServerMetricsListener>(
          new MockSecAggServerMetricsListener()),
      /*prng_runner=*/nullptr,
      std::make_unique<TestSecAggExperiment>(experiment_names), threat_model);
  EXPECT_THAT(status_or_server.ok(), true) << status_or_server.status();
  EXPECT_THAT(status_or_server.value()->NumberOfNeighbors(), Eq(1000));
  EXPECT_THAT(status_or_server.value()->IsAborted(), Eq(false));
  EXPECT_THAT(status_or_server.value()->IsCompletedSuccessfully(), Eq(false));
  EXPECT_THAT(status_or_server.value()->State(),
              Eq(SecAggServerStateKind::R0_ADVERTISE_KEYS));
}

TEST(SecaggServerTest, SubgraphSecAggResortsToFullGraphOnSmallCohorts) {
  // Tests that a small number of clients for which subgraph-secagg does not
  // have favorable parameters results in executiong the full-graph varian
  SecureAggregationRequirements threat_model;
  threat_model.set_adversary_class(AdversaryClass::CURIOUS_SERVER);
  threat_model.set_adversarial_client_rate(.45);
  threat_model.set_estimated_dropout_rate(.45);
  std::unique_ptr<AesPrngFactory> prng_factory;
  std::vector<InputVectorSpecification> input_vector_specs;
  auto sender = std::make_unique<MockSendToClientsInterface>();
  input_vector_specs.push_back(InputVectorSpecification("foobar", 4, 32));
  std::set<std::string> experiment_names = {};
  auto status_or_server = SecAggServer::Create(
      5,   // minimum_number_of_clients_to_proceed
      25,  // total_number_of_clients
      input_vector_specs, sender.get(),
      std::unique_ptr<SecAggServerMetricsListener>(
          new MockSecAggServerMetricsListener()),
      /*prng_runner=*/nullptr,
      std::make_unique<TestSecAggExperiment>(experiment_names), threat_model);
  EXPECT_THAT(status_or_server.ok(), true) << status_or_server.status();
  EXPECT_THAT(status_or_server.value()->NumberOfNeighbors(), Eq(25));
  EXPECT_THAT(
      status_or_server.value()->MinimumSurvivingNeighborsForReconstruction(),
      Eq(14));
  EXPECT_THAT(status_or_server.value()->IsAborted(), Eq(false));
  EXPECT_THAT(status_or_server.value()->IsCompletedSuccessfully(), Eq(false));
  EXPECT_THAT(status_or_server.value()->State(),
              Eq(SecAggServerStateKind::R0_ADVERTISE_KEYS));
}

TEST(SecaggServerTest, AbortClientWithInvalidIdThrowsError) {
  TestTracingRecorder tracing_recorder;
  auto sender = std::make_unique<MockSendToClientsInterface>();
  auto server = CreateServer(sender.get());

  EXPECT_THAT(
      server->AbortClient(1001, ClientAbortReason::CONNECTION_DROPPED).code(),
      Eq(FAILED_PRECONDITION));
}

TEST(SecaggServerTest, ReceiveMessageWithInvalidIdThrowsError) {
  TestTracingRecorder tracing_recorder;
  auto sender = std::make_unique<MockSendToClientsInterface>();
  auto server = CreateServer(sender.get());

  ClientToServerWrapperMessage client_abort_message;
  client_abort_message.mutable_abort()->set_diagnostic_info("Abort for test.");
  EXPECT_THAT(
      server
          ->ReceiveMessage(1001, std::make_unique<ClientToServerWrapperMessage>(
                                     client_abort_message))
          .status()
          .code(),
      Eq(FAILED_PRECONDITION));
}

TEST(SecaggServerTest, AbortCausesStateTransitionAndMessageToBeSent) {
  TestTracingRecorder tracing_recorder;
  auto sender = std::make_unique<MockSendToClientsInterface>();
  auto server = CreateServer(sender.get());

  const ServerToClientWrapperMessage abort_message = PARSE_TEXT_PROTO(R"pb(
    abort: {
      early_success: false
      diagnostic_info: "Abort upon external request."
    })pb");

  EXPECT_CALL(*sender, Send(_, _)).Times(0);
  for (int i = 0; i < 1000; ++i) {
    EXPECT_CALL(*sender, Send(i, EqualsProto(abort_message))).Times(1);
  }
  Status result = server->Abort();

  EXPECT_THAT(result.code(), Eq(OK));
  EXPECT_THAT(server->IsAborted(), Eq(true));
  EXPECT_THAT(server->State(), Eq(SecAggServerStateKind::ABORTED));
  ASSERT_THAT(server->ErrorMessage().ok(), Eq(true));
  EXPECT_THAT(server->ErrorMessage().value(),
              Eq("Abort upon external request."));
  EXPECT_THAT(
      tracing_recorder.root(),
      TraceRecorderHas(
          AllOf(IsSpan<SecureAggServerState>(
                    SecAggServerTraceState_R0AdvertiseKeys),
                ElementsAre(
                    IsSpan<AbortSecAggServer>("Abort upon external request."))),
          IsSpan<SecureAggServerState>(SecAggServerTraceState_Aborted)));
}

TEST(SecaggServerTest, AbortWithReasonCausesStateTransitionAndMessageToBeSent) {
  TestTracingRecorder tracing_recorder;
  auto sender = std::make_unique<MockSendToClientsInterface>();
  auto server = CreateServer(sender.get());

  const ServerToClientWrapperMessage abort_message = PARSE_TEXT_PROTO(R"pb(
    abort: {
      early_success: false
      diagnostic_info: "Abort upon external request for reason <Test reason.>."
    })pb");

  EXPECT_CALL(*sender, Send(_, _)).Times(0);
  for (int i = 0; i < 1000; ++i) {
    EXPECT_CALL(*sender, Send(i, EqualsProto(abort_message))).Times(1);
  }
  Status result =
      server->Abort("Test reason.", SecAggServerOutcome::EXTERNAL_REQUEST);

  EXPECT_THAT(result.code(), Eq(OK));
  EXPECT_THAT(server->IsAborted(), Eq(true));
  EXPECT_THAT(server->State(), Eq(SecAggServerStateKind::ABORTED));
  ASSERT_THAT(server->ErrorMessage().ok(), Eq(true));
  EXPECT_THAT(server->ErrorMessage().value(),
              Eq("Abort upon external request for reason <Test reason.>."));
  EXPECT_THAT(
      tracing_recorder.root(),
      TraceRecorderHas(
          AllOf(IsSpan<SecureAggServerState>(
                    SecAggServerTraceState_R0AdvertiseKeys),
                ElementsAre(IsSpan<AbortSecAggServer>(
                    "Abort upon external request for reason <Test "
                    "reason.>."))),
          IsSpan<SecureAggServerState>(SecAggServerTraceState_Aborted)));
}

TEST(SecaggServerTest, AbortClientNotCheckedIn) {
  TestTracingRecorder tracing_recorder;
  auto sender = std::make_unique<MockSendToClientsInterface>();
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();
  auto server = CreateServer(sender.get(), metrics);

  EXPECT_CALL(*metrics, ClientsDropped(
                            Eq(ClientStatus::DEAD_BEFORE_SENDING_ANYTHING),
                            Eq(ClientDropReason::SERVER_PROTOCOL_ABORT_CLIENT)))
      .Times(0);
  // Client is not notified
  EXPECT_CALL(*sender, Send(_, _)).Times(0);
  Status result = server->AbortClient(2, ClientAbortReason::NOT_CHECKED_IN);

  EXPECT_THAT(result.code(), Eq(OK));
  EXPECT_THAT(server->AbortedClientIds().contains(2), Eq(true));
  EXPECT_THAT(
      tracing_recorder.root(),
      TraceRecorderHas(AllOf(
          IsSpan<SecureAggServerState>(SecAggServerTraceState_R0AdvertiseKeys),
          ElementsAre(IsSpan<AbortSecAggClient>(2, "NOT_CHECKED_IN")))));
}

TEST(SecaggServerTest, AbortClientWhenConnectionDropped) {
  TestTracingRecorder tracing_recorder;
  auto sender = std::make_unique<MockSendToClientsInterface>();
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();
  auto server = CreateServer(sender.get(), metrics);

  EXPECT_CALL(*metrics,
              ClientsDropped(Eq(ClientStatus::DEAD_BEFORE_SENDING_ANYTHING),
                             Eq(ClientDropReason::CONNECTION_CLOSED)));
  // Client is not notified
  EXPECT_CALL(*sender, Send(_, _)).Times(0);
  Status result = server->AbortClient(2, ClientAbortReason::CONNECTION_DROPPED);

  EXPECT_THAT(result.code(), Eq(OK));
  EXPECT_THAT(server->AbortedClientIds().contains(2), Eq(true));
  EXPECT_THAT(
      tracing_recorder.root(),
      TraceRecorderHas(AllOf(
          IsSpan<SecureAggServerState>(SecAggServerTraceState_R0AdvertiseKeys),
          ElementsAre(IsSpan<AbortSecAggClient>(2, "CONNECTION_DROPPED")))));
}

TEST(SecaggServerTest, AbortClientWhenInvalidMessageSent) {
  TestTracingRecorder tracing_recorder;
  auto sender = std::make_unique<MockSendToClientsInterface>();
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();
  auto server = CreateServer(sender.get(), metrics);

  const ServerToClientWrapperMessage message = PARSE_TEXT_PROTO(R"pb(
    abort: {
      early_success: false
      diagnostic_info: "The protocol is closing client with ClientAbortReason <INVALID_MESSAGE>."
    })pb");
  EXPECT_CALL(*sender, Send(2, EqualsProto(message)));

  EXPECT_CALL(
      *metrics,
      ClientsDropped(Eq(ClientStatus::DEAD_BEFORE_SENDING_ANYTHING),
                     Eq(ClientDropReason::SERVER_PROTOCOL_ABORT_CLIENT)));
  Status result = server->AbortClient(2, ClientAbortReason::INVALID_MESSAGE);

  EXPECT_THAT(result.code(), Eq(OK));
  EXPECT_THAT(server->AbortedClientIds().contains(2), Eq(true));
  EXPECT_THAT(
      tracing_recorder.root(),
      TraceRecorderHas(AllOf(
          IsSpan<SecureAggServerState>(SecAggServerTraceState_R0AdvertiseKeys),
          ElementsAre(IsSpan<AbortSecAggClient>(2, "INVALID_MESSAGE")))));
}

TEST(SecaggServerTest, ReceiveMessageCausesServerToAbortIfTooManyClientsAbort) {
  // The actual behavior of the server upon receipt of messages is tested in the
  // state class test files, but this tests the special behavior that the server
  // should automatically transition to an abort state if it cannot continue.
  TestTracingRecorder tracing_recorder;
  auto sender = std::make_unique<MockSendToClientsInterface>();
  auto server = CreateServer(sender.get());
  StatusOr<int> clients_needed = server->MinimumMessagesNeededForNextRound();
  ASSERT_THAT(clients_needed.ok(), Eq(true));
  int maximum_number_of_aborts =
      server->NumberOfAliveClients() - clients_needed.value();
  EcdhPregeneratedTestKeys ecdh_keys;
  ClientToServerWrapperMessage client_abort_message;
  client_abort_message.mutable_abort()->set_diagnostic_info("Abort for test.");

  // Receiving `maximum_number_of_aborts - 1` aborts should not cause the entire
  // protocol to abort.
  std::vector<Matcher<const TestTracingRecorder::SpanOrEvent&>> matchers;
  for (int i = 0; i < maximum_number_of_aborts; ++i) {
    StatusOr<bool> result = server->ReceiveMessage(
        i,
        std::make_unique<ClientToServerWrapperMessage>(client_abort_message));
    matchers.push_back(IsSpan<ReceiveSecAggMessage>(i));
    ASSERT_THAT(result.ok(), Eq(true));
    EXPECT_THAT(result.value(), Eq(false));
    EXPECT_THAT(server->IsAborted(), Eq(false));
    EXPECT_THAT(
        tracing_recorder.root(),
        TraceRecorderHas(AllOf(IsSpan<SecureAggServerState>(
                                   SecAggServerTraceState_R0AdvertiseKeys),
                               ElementsAreArray(matchers))));
  }
  // Receiving `maximum_number_of_aborts` aborts means the protocol is ready to
  // proceed to the aborted state, which is indicated by ReceiveMessage
  // returning true.
  StatusOr<bool> result = server->ReceiveMessage(
      maximum_number_of_aborts,
      std::make_unique<ClientToServerWrapperMessage>(client_abort_message));
  matchers.push_back(IsSpan<ReceiveSecAggMessage>(maximum_number_of_aborts));
  ASSERT_THAT(result.ok(), Eq(true));
  EXPECT_THAT(result.value(), Eq(true));
  // However the server is not aborted until ProceedToNextRound is called.
  EXPECT_THAT(server->IsAborted(), Eq(false));

  EXPECT_THAT(server->ProceedToNextRound(), IsOk());
  matchers.push_back(IsSpan<ProceedToNextSecAggRound>());
  EXPECT_THAT(server->IsAborted(), Eq(true));
  EXPECT_THAT(server->State(), Eq(SecAggServerStateKind::ABORTED));

  EXPECT_THAT(
      tracing_recorder.root(),
      TraceRecorderHas(
          AllOf(IsSpan<SecureAggServerState>(
                    SecAggServerTraceState_R0AdvertiseKeys),
                ElementsAreArray(matchers)),
          IsSpan<SecureAggServerState>(SecAggServerTraceState_Aborted)));
}

TEST(SecaggServerTest, VerifyErrorsInAbortedState) {
  TestTracingRecorder tracing_recorder;
  auto sender = std::make_unique<MockSendToClientsInterface>();
  auto server = CreateServer(sender.get());
  EXPECT_THAT(server->Abort(), IsOk());

  EXPECT_THAT(
      server->ReceiveMessage(1, std::make_unique<ClientToServerWrapperMessage>(
                                    ClientToServerWrapperMessage{})),
      IsCode(FAILED_PRECONDITION));
  EXPECT_THAT(server->ProceedToNextRound(), IsCode(FAILED_PRECONDITION));
  EXPECT_THAT(server->MinimumMessagesNeededForNextRound(),
              IsCode(FAILED_PRECONDITION));
  EXPECT_THAT(server->NumberOfMessagesReceivedInThisRound(),
              IsCode(FAILED_PRECONDITION));
  EXPECT_THAT(server->ReadyForNextRound(), IsCode(FAILED_PRECONDITION));
}

}  // namespace
}  // namespace secagg
}  // namespace fcp
