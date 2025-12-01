/*
 * Copyright 2019 Google LLC
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

#include "fcp/secagg/server/secagg_server_r2_masked_input_coll_state.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/node_hash_set.h"
#include "absl/strings/str_cat.h"
#include "fcp/base/monitoring.h"
#include "fcp/secagg/server/aes/aes_secagg_server_protocol_impl.h"
#include "fcp/secagg/server/experiments_interface.h"
#include "fcp/secagg/server/experiments_names.h"
#include "fcp/secagg/server/secagg_server_state.h"
#include "fcp/secagg/server/secret_sharing_graph_factory.h"
#include "fcp/secagg/server/send_to_clients_interface.h"
#include "fcp/secagg/shared/aes_ctr_prng_factory.h"
#include "fcp/secagg/shared/ecdh_key_agreement.h"
#include "fcp/secagg/shared/ecdh_keys.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/secagg/shared/shamir_secret_sharing.h"
#include "fcp/secagg/testing/ecdh_pregenerated_test_keys.h"
#include "fcp/secagg/testing/fake_prng.h"
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
using ::testing::Ge;
using ::testing::IsFalse;
using ::testing::IsTrue;

class FakeScheduler : public Scheduler {
 public:
  void Schedule(std::function<void()> job) override { jobs_.push_back(job); }

  void WaitUntilIdle() override {}

  void Run() {
    for (auto& job : jobs_) {
      job();
    }
    jobs_.clear();
  }

 private:
  std::vector<std::function<void()>> jobs_;
};

// Default test session_id.
SessionId session_id = {"session id number, 32 bytes long"};

struct SecAggR2StateTestParams {
  const std::string test_name;
  // Enables asymchronous processing of round 2 messages by the server.
  bool enable_async_r2;
};

class SecaggServerR2MaskedInputCollStateTest
    : public ::testing::TestWithParam<SecAggR2StateTestParams> {
 protected:
  std::unique_ptr<AesSecAggServerProtocolImpl> CreateSecAggServerProtocolImpl(
      int minimum_number_of_clients_to_proceed, int total_number_of_clients,
      MockSendToClientsInterface* sender,
      MockSecAggServerMetricsListener* metrics_listener = nullptr,
      bool enable_async_r2 = true) {
    auto input_vector_specs = std::vector<InputVectorSpecification>();
    input_vector_specs.push_back(InputVectorSpecification("foobar", 4, 32));
    SecretSharingGraphFactory factory;
    auto impl = std::make_unique<AesSecAggServerProtocolImpl>(
        factory.CreateCompleteGraph(total_number_of_clients,
                                    minimum_number_of_clients_to_proceed),
        minimum_number_of_clients_to_proceed, input_vector_specs,
        std::unique_ptr<MockSecAggServerMetricsListener>(metrics_listener),
        std::make_unique<AesCtrPrngFactory>(), sender,
        std::make_unique<SecAggScheduler>(&parallel_scheduler_,
                                          &sequential_scheduler_),
        std::vector<ClientStatus>(total_number_of_clients,
                                  ClientStatus::SHARE_KEYS_RECEIVED),
        ServerVariant::NATIVE_V1,
        enable_async_r2
            ? std::make_unique<TestSecAggExperiment>(
                  TestSecAggExperiment(kSecAggAsyncRound2Experiment))
            : std::make_unique<TestSecAggExperiment>(TestSecAggExperiment()));
    impl->set_session_id(std::make_unique<SessionId>(session_id));
    EcdhPregeneratedTestKeys ecdh_keys;
    for (int i = 0; i < total_number_of_clients; ++i) {
      impl->SetPairwisePublicKeys(i, ecdh_keys.GetPublicKey(i));
    }

    return impl;
  }

  void RunSchedulers() {
    parallel_scheduler_.Run();
    sequential_scheduler_.Run();
  }

 private:
  FakeScheduler parallel_scheduler_;
  FakeScheduler sequential_scheduler_;
};

TEST_P(SecaggServerR2MaskedInputCollStateTest, IsAbortedReturnsFalse) {
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR2MaskedInputCollState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get(),
                                     nullptr /* metrics_listener */,
                                     GetParam().enable_async_r2),
      0,  // number_of_clients_failed_after_sending_masked_input
      0,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_THAT(state.IsAborted(), IsFalse());
}

TEST_P(SecaggServerR2MaskedInputCollStateTest,
       IsCompletedSuccessfullyReturnsFalse) {
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR2MaskedInputCollState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get(),
                                     nullptr /* metrics_listener */,
                                     GetParam().enable_async_r2),
      0,  // number_of_clients_failed_after_sending_masked_input
      0,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_THAT(state.IsCompletedSuccessfully(), IsFalse());
}

TEST_P(SecaggServerR2MaskedInputCollStateTest, ErrorMessageRaisesErrorStatus) {
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR2MaskedInputCollState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get(),
                                     nullptr /* metrics_listener */,
                                     GetParam().enable_async_r2),
      0,  // number_of_clients_failed_after_sending_masked_input
      0,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_THAT(state.ErrorMessage().ok(), IsFalse());
}

TEST_P(SecaggServerR2MaskedInputCollStateTest, ResultRaisesErrorStatus) {
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR2MaskedInputCollState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get(),
                                     nullptr /* metrics_listener */,
                                     GetParam().enable_async_r2),
      0,  // number_of_clients_failed_after_sending_masked_input
      0,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_THAT(state.Result().ok(), IsFalse());
}

TEST_P(SecaggServerR2MaskedInputCollStateTest,
       AbortReturnsValidStateAndNotifiesClients) {
  TestTracingRecorder tracing_recorder;
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR2MaskedInputCollState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get(), metrics,
                                     GetParam().enable_async_r2),
      0,  // number_of_clients_failed_after_sending_masked_input
      0,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  ServerToClientWrapperMessage abort_message;
  abort_message.mutable_abort()->set_early_success(false);
  abort_message.mutable_abort()->set_diagnostic_info("test abort reason");

  EXPECT_CALL(*metrics,
              ProtocolOutcomes(Eq(SecAggServerOutcome::EXTERNAL_REQUEST)));
  EXPECT_CALL(*sender, Send(_, _)).Times(0);
  for (int i = 0; i < 4; ++i) {
    EXPECT_CALL(*sender, Send(i, EqualsProto(abort_message))).Times(1);
  }
  auto next_state =
      state.Abort("test abort reason", SecAggServerOutcome::EXTERNAL_REQUEST);

  ASSERT_THAT(next_state->State(), Eq(SecAggServerStateKind::ABORTED));
  ASSERT_THAT(next_state->ErrorMessage(), IsOk());
  EXPECT_THAT(next_state->ErrorMessage().value(), Eq("test abort reason"));
  EXPECT_THAT(tracing_recorder.FindAllEvents<BroadcastMessageSent>(),
              ElementsAre(IsEvent<BroadcastMessageSent>(
                  Eq(ServerToClientMessageType_Abort),
                  Eq(abort_message.ByteSizeLong()))));
}

TEST_P(SecaggServerR2MaskedInputCollStateTest,
       StateProceedsCorrectlyWithAllClientsValid) {
  // In this test, all clients send in their valid masked inputs, and then the
  // server proceeds to the next state.
  TestTracingRecorder tracing_recorder;
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR2MaskedInputCollState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get(),
                                     nullptr /* metrics_listener */,
                                     GetParam().enable_async_r2),
      0,  // number_of_clients_failed_after_sending_masked_input
      0,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  for (int i = 0; i < 5; ++i) {
    EXPECT_THAT(state.NeedsToAbort(), IsFalse());
    EXPECT_THAT(state.NumberOfAliveClients(), Eq(4));
    EXPECT_THAT(state.NumberOfClientsReadyForNextRound(), Eq(i));
    EXPECT_THAT(state.NumberOfMessagesReceivedInThisRound(), Eq(i));
    EXPECT_THAT(state.NumberOfPendingClients(), Eq(4 - i));
    if (i < 3) {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(3 - i));
    } else {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(0));
    }
    if (GetParam().enable_async_r2) {
      EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
    } else {
      EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 3));
    }

    if (i < 4) {
      // Have client send a vector of the correct size to the server
      auto client_message = std::make_unique<ClientToServerWrapperMessage>();
      MaskedInputVector encoded_vector;
      SecAggVector masked_vector(std::vector<uint64_t>(4, i + 1), 32);
      encoded_vector.set_encoded_vector(masked_vector.GetAsPackedBytes());
      (*client_message->mutable_masked_input_response()
            ->mutable_vectors())["foobar"] = encoded_vector;
      ASSERT_THAT(state.HandleMessage(i, std::move(client_message)), IsOk());
      if (GetParam().enable_async_r2) {
        EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
      } else {
        EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 2));
      }
    }
  }

  if (GetParam().enable_async_r2) {
    RunSchedulers();
    EXPECT_THAT(state.ReadyForNextRound(), IsTrue());
  }

  ServerToClientWrapperMessage server_message;
  server_message.mutable_unmasking_request()
      ->mutable_dead_3_client_ids()
      ->Clear();  // Just to set it to an empty vector
  EXPECT_CALL(*sender, Send(_, _)).Times(0);
  for (int i = 0; i < 4; ++i) {
    EXPECT_CALL(*sender, Send(i, EqualsProto(server_message))).Times(1);
  }

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::R3_UNMASKING));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedAfterSendingMaskedInput(),
      Eq(0));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedBeforeSendingMaskedInput(),
      Eq(0));
  EXPECT_THAT(next_state.value()->NumberOfClientsTerminatedWithoutUnmasking(),
              Eq(0));
  EXPECT_THAT(tracing_recorder.FindAllEvents<IndividualMessageSent>(),
              ElementsAre(IsEvent<IndividualMessageSent>(
                              0, Eq(ServerToClientMessageType_UnmaskingRequest),
                              Eq(server_message.ByteSizeLong())),
                          IsEvent<IndividualMessageSent>(
                              1, Eq(ServerToClientMessageType_UnmaskingRequest),
                              Eq(server_message.ByteSizeLong())),
                          IsEvent<IndividualMessageSent>(
                              2, Eq(ServerToClientMessageType_UnmaskingRequest),
                              Eq(server_message.ByteSizeLong())),
                          IsEvent<IndividualMessageSent>(
                              3, Eq(ServerToClientMessageType_UnmaskingRequest),
                              Eq(server_message.ByteSizeLong()))));
}

TEST_P(SecaggServerR2MaskedInputCollStateTest,
       StateProceedsCorrectlyWithoutAllClients) {
  // In this test, clients 0 through 2 send in valid masked inputs, and then we
  // proceed to the next step even without client 3.
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR2MaskedInputCollState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get(),
                                     nullptr /* metrics_listener */,
                                     GetParam().enable_async_r2),
      0,  // number_of_clients_failed_after_sending_masked_input
      0,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  for (int i = 0; i < 4; ++i) {
    EXPECT_THAT(state.NeedsToAbort(), IsFalse());
    EXPECT_THAT(state.NumberOfAliveClients(), Eq(4));
    EXPECT_THAT(state.NumberOfClientsReadyForNextRound(), Eq(i));
    EXPECT_THAT(state.NumberOfMessagesReceivedInThisRound(), Eq(i));
    EXPECT_THAT(state.NumberOfPendingClients(), Eq(4 - i));
    if (i < 3) {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(3 - i));
    } else {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(0));
    }
    if (GetParam().enable_async_r2) {
      EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
    } else {
      EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 3));
    }

    if (i < 3) {
      // Have client send a vector of the correct size to the server
      auto client_message = std::make_unique<ClientToServerWrapperMessage>();
      MaskedInputVector encoded_vector;
      SecAggVector masked_vector(std::vector<uint64_t>(4, i + 1), 32);
      encoded_vector.set_encoded_vector(masked_vector.GetAsPackedBytes());
      (*client_message->mutable_masked_input_response()
            ->mutable_vectors())["foobar"] = encoded_vector;
      ASSERT_THAT(state.HandleMessage(i, std::move(client_message)), IsOk());
      if (GetParam().enable_async_r2) {
        EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
      } else {
        EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 2));
      }
    }
  }

  if (GetParam().enable_async_r2) {
    RunSchedulers();
    EXPECT_THAT(state.ReadyForNextRound(), IsTrue());
  }

  ServerToClientWrapperMessage server_message;
  // TODO(team): 4 -> 3 below, once backwards compatibility not needed.
  server_message.mutable_unmasking_request()->add_dead_3_client_ids(4);
  ServerToClientWrapperMessage abort_message;
  abort_message.mutable_abort()->set_early_success(false);
  abort_message.mutable_abort()->set_diagnostic_info(
      "Client did not send MaskedInputCollectionResponse before round "
      "transition.");
  EXPECT_CALL(*sender, Send(_, _)).Times(0);
  for (int i = 0; i < 3; ++i) {
    EXPECT_CALL(*sender, Send(i, EqualsProto(server_message))).Times(1);
  }
  EXPECT_CALL(*sender, Send(3, EqualsProto(abort_message))).Times(1);

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::R3_UNMASKING));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedAfterSendingMaskedInput(),
      Eq(0));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedBeforeSendingMaskedInput(),
      Eq(1));
  EXPECT_THAT(next_state.value()->NumberOfClientsTerminatedWithoutUnmasking(),
              Eq(0));
}

TEST_P(SecaggServerR2MaskedInputCollStateTest,
       StateProceedsCorrectlyWithOneClientSendingInvalidInput) {
  // In this test, client 0 sends an invalid masked input, so it is aborted. The
  // rest of the round goes normally.
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR2MaskedInputCollState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get(),
                                     nullptr /* metrics_listener */,
                                     GetParam().enable_async_r2),
      0,  // number_of_clients_failed_after_sending_masked_input
      0,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  ServerToClientWrapperMessage server_message;
  // TODO(team): 1 -> 0 below, once backwards compatibility not needed.
  server_message.mutable_unmasking_request()->add_dead_3_client_ids(1);
  ServerToClientWrapperMessage abort_message;
  abort_message.mutable_abort()->set_early_success(false);
  abort_message.mutable_abort()->set_diagnostic_info(
      "Masked input does not match input vector specification - vector is "
      "wrong size.");

  EXPECT_CALL(*sender, Send(_, _)).Times(0);
  for (int i = 1; i < 4; ++i) {
    EXPECT_CALL(*sender, Send(i, EqualsProto(server_message))).Times(1);
  }
  EXPECT_CALL(*sender, Send(0, EqualsProto(abort_message))).Times(1);

  // Have client 0 send an invalid message.
  auto invalid_message = std::make_unique<ClientToServerWrapperMessage>();
  MaskedInputVector encoded_vector;
  encoded_vector.set_encoded_vector("not a real masked input vector - invalid");
  (*invalid_message->mutable_masked_input_response()
        ->mutable_vectors())["foobar"] = encoded_vector;
  ASSERT_THAT(state.HandleMessage(0, std::move(invalid_message)), IsOk());
  EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
  for (int i = 1; i < 5; ++i) {
    EXPECT_THAT(state.NeedsToAbort(), IsFalse());
    EXPECT_THAT(state.NumberOfAliveClients(), Eq(3));
    EXPECT_THAT(state.NumberOfClientsReadyForNextRound(), Eq(i - 1));
    EXPECT_THAT(state.NumberOfMessagesReceivedInThisRound(), Eq(i - 1));
    EXPECT_THAT(state.NumberOfPendingClients(), Eq(4 - i));
    if (i < 4) {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(4 - i));
    } else {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(0));
    }
    if (GetParam().enable_async_r2) {
      EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
    } else {
      EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 4));
    }

    if (i < 4) {
      // Have client send a vector of the correct size to the server
      auto client_message = std::make_unique<ClientToServerWrapperMessage>();
      MaskedInputVector encoded_vector;
      SecAggVector masked_vector(std::vector<uint64_t>(4, i + 1), 32);
      encoded_vector.set_encoded_vector(masked_vector.GetAsPackedBytes());
      (*client_message->mutable_masked_input_response()
            ->mutable_vectors())["foobar"] = encoded_vector;
      ASSERT_THAT(state.HandleMessage(i, std::move(client_message)), IsOk());
      if (GetParam().enable_async_r2) {
        EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
      } else {
        EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 3));
      }
    }
  }

  if (GetParam().enable_async_r2) {
    RunSchedulers();
    EXPECT_THAT(state.ReadyForNextRound(), IsTrue());
  }

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::R3_UNMASKING));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedAfterSendingMaskedInput(),
      Eq(0));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedBeforeSendingMaskedInput(),
      Eq(1));
  EXPECT_THAT(next_state.value()->NumberOfClientsTerminatedWithoutUnmasking(),
              Eq(0));
}

TEST_P(SecaggServerR2MaskedInputCollStateTest,
       StateProceedsCorrectlyWithOneClientAbortingAfterSendingInput) {
  // In this test, all clients send in their valid masked inputs, but then
  // client 2 aborts before the server proceeds to the next state.
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR2MaskedInputCollState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get(),
                                     nullptr /* metrics_listener */,
                                     GetParam().enable_async_r2),
      0,  // number_of_clients_failed_after_sending_masked_input
      0,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  for (int i = 0; i < 5; ++i) {
    EXPECT_THAT(state.NeedsToAbort(), IsFalse());
    EXPECT_THAT(state.NumberOfAliveClients(), Eq(4));
    EXPECT_THAT(state.NumberOfClientsReadyForNextRound(), Eq(i));
    EXPECT_THAT(state.NumberOfMessagesReceivedInThisRound(), Eq(i));
    EXPECT_THAT(state.NumberOfPendingClients(), Eq(4 - i));
    if (i < 3) {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(3 - i));
    } else {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(0));
    }
    if (GetParam().enable_async_r2) {
      EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
    } else {
      EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 3));
    }
    if (i < 4) {
      // Have client send a vector of the correct size to the server
      auto client_message = std::make_unique<ClientToServerWrapperMessage>();
      MaskedInputVector encoded_vector;
      SecAggVector masked_vector(std::vector<uint64_t>(4, i + 1), 32);
      encoded_vector.set_encoded_vector(masked_vector.GetAsPackedBytes());
      (*client_message->mutable_masked_input_response()
            ->mutable_vectors())["foobar"] = encoded_vector;
      ASSERT_THAT(state.HandleMessage(i, std::move(client_message)), IsOk());
      if (GetParam().enable_async_r2) {
        EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
      } else {
        EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 2));
      }
    }
  }

  if (GetParam().enable_async_r2) {
    RunSchedulers();
    EXPECT_THAT(state.ReadyForNextRound(), IsTrue());
  }

  auto abort_message = std::make_unique<ClientToServerWrapperMessage>();
  abort_message->mutable_abort()->set_diagnostic_info("Aborting for test");
  ASSERT_THAT(state.HandleMessage(2, std::move(abort_message)), IsOk());
  EXPECT_THAT(state.ReadyForNextRound(), IsTrue());
  EXPECT_THAT(state.NeedsToAbort(), IsFalse());
  EXPECT_THAT(state.NumberOfAliveClients(), Eq(3));
  EXPECT_THAT(state.NumberOfClientsReadyForNextRound(), Eq(3));
  EXPECT_THAT(state.NumberOfMessagesReceivedInThisRound(), Eq(4));
  EXPECT_THAT(state.NumberOfPendingClients(), Eq(0));
  EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(0));
  EXPECT_THAT(state.ReadyForNextRound(), IsTrue());

  ServerToClientWrapperMessage server_message;
  server_message.mutable_unmasking_request()
      ->mutable_dead_3_client_ids()
      ->Clear();  // Just to set it to an empty vector
  EXPECT_CALL(*sender, Send(_, _)).Times(0);
  for (int i = 0; i < 4; ++i) {
    if (i != 2) {
      EXPECT_CALL(*sender, Send(i, EqualsProto(server_message))).Times(1);
    }
  }

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::R3_UNMASKING));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedAfterSendingMaskedInput(),
      Eq(1));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedBeforeSendingMaskedInput(),
      Eq(0));
  EXPECT_THAT(next_state.value()->NumberOfClientsTerminatedWithoutUnmasking(),
              Eq(0));
}

TEST_P(SecaggServerR2MaskedInputCollStateTest,
       StateForcesAbortIfTooManyClientsAbort) {
  // In this test, clients 0 and 1 abort, so the state aborts.
  TestTracingRecorder tracing_recorder;
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR2MaskedInputCollState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get(),
                                     nullptr /* metrics_listener */,
                                     GetParam().enable_async_r2),
      0,  // number_of_clients_failed_after_sending_masked_input
      0,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  for (int i = 0; i < 3; ++i) {
    EXPECT_THAT(state.NeedsToAbort(), Eq(i >= 2));
    EXPECT_THAT(state.NumberOfAliveClients(), Eq(4 - i));
    EXPECT_THAT(state.NumberOfClientsReadyForNextRound(), Eq(0));
    EXPECT_THAT(state.NumberOfMessagesReceivedInThisRound(), Eq(0));
    EXPECT_THAT(state.NumberOfPendingClients(), Eq(4 - i));
    EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(3));
    EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 2));
    if (i < 2) {
      // Have client abort
      auto abort_message = std::make_unique<ClientToServerWrapperMessage>();
      abort_message->mutable_abort()->set_diagnostic_info("Aborting for test");
      ASSERT_THAT(state.HandleMessage(i, std::move(abort_message)), IsOk());
      EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 1));
    }
  }

  ServerToClientWrapperMessage server_message;
  server_message.mutable_abort()->set_early_success(false);
  server_message.mutable_abort()->set_diagnostic_info(
      "Too many clients aborted.");
  EXPECT_CALL(*sender, Send(_, _)).Times(0);
  for (int i = 0; i < 4; ++i) {
    EXPECT_CALL(*sender, Send(i, EqualsProto(server_message)))
        .Times(static_cast<int>(i >= 2));
  }

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(), Eq(SecAggServerStateKind::ABORTED));
  ASSERT_THAT(next_state.value()->ErrorMessage(), IsOk());
  EXPECT_THAT(next_state.value()->ErrorMessage().value(),
              Eq("Too many clients aborted."));
  EXPECT_THAT(tracing_recorder.FindAllEvents<BroadcastMessageSent>(),
              ElementsAre(IsEvent<BroadcastMessageSent>(
                  Eq(ServerToClientMessageType_Abort),
                  Eq(server_message.ByteSizeLong()))));
}

TEST_P(SecaggServerR2MaskedInputCollStateTest, MetricsRecordsMessageSizes) {
  // In this test, all clients send in their valid masked inputs, but then
  // client 2 aborts before the server proceeds to the next state.
  TestTracingRecorder tracing_recorder;
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR2MaskedInputCollState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get(), metrics,
                                     GetParam().enable_async_r2),
      0,  // number_of_clients_failed_after_sending_masked_input
      0,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  for (int i = 0; i < 5; ++i) {
    EXPECT_THAT(state.NeedsToAbort(), IsFalse());
    EXPECT_THAT(state.NumberOfAliveClients(), Eq(4));
    EXPECT_THAT(state.NumberOfClientsReadyForNextRound(), Eq(i));
    EXPECT_THAT(state.NumberOfMessagesReceivedInThisRound(), Eq(i));
    EXPECT_THAT(state.NumberOfPendingClients(), Eq(4 - i));
    if (i < 3) {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(3 - i));
    } else {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(0));
    }
    if (GetParam().enable_async_r2) {
      EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
    } else {
      EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 3));
    }
    if (i < 4) {
      // Have client send a vector of the correct size to the server
      auto client_message = std::make_unique<ClientToServerWrapperMessage>();
      MaskedInputVector encoded_vector;
      SecAggVector masked_vector(std::vector<uint64_t>(4, i + 1), 32);
      encoded_vector.set_encoded_vector(masked_vector.GetAsPackedBytes());
      (*client_message->mutable_masked_input_response()
            ->mutable_vectors())["foobar"] = encoded_vector;
      EXPECT_CALL(
          *metrics,
          MessageReceivedSizes(Eq(ClientToServerWrapperMessage::
                                      MessageContentCase::kMaskedInputResponse),
                               Eq(true), Eq(client_message->ByteSizeLong())));
      ASSERT_THAT(state.HandleMessage(i, std::move(client_message)), IsOk());
      if (GetParam().enable_async_r2) {
        EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
      } else {
        EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 2));
      }
    }
  }

  if (GetParam().enable_async_r2) {
    RunSchedulers();
    EXPECT_THAT(state.ReadyForNextRound(), IsTrue());
  }

  auto abort_message = std::make_unique<ClientToServerWrapperMessage>();
  abort_message->mutable_abort()->set_diagnostic_info("Aborting for test");
  EXPECT_CALL(*metrics,
              MessageReceivedSizes(
                  Eq(ClientToServerWrapperMessage::MessageContentCase::kAbort),
                  Eq(false), Eq(abort_message->ByteSizeLong())));

  size_t abort_message_size = abort_message->ByteSizeLong();
  ASSERT_THAT(state.HandleMessage(2, std::move(abort_message)), IsOk());
  EXPECT_THAT(state.ReadyForNextRound(), IsTrue());
  EXPECT_THAT(state.NeedsToAbort(), IsFalse());
  EXPECT_THAT(state.NumberOfAliveClients(), Eq(3));
  EXPECT_THAT(state.NumberOfClientsReadyForNextRound(), Eq(3));
  EXPECT_THAT(state.NumberOfMessagesReceivedInThisRound(), Eq(4));
  EXPECT_THAT(state.NumberOfPendingClients(), Eq(0));
  EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(0));
  EXPECT_THAT(state.ReadyForNextRound(), IsTrue());
  EXPECT_THAT(tracing_recorder.root(),
              Contains(IsEvent<ClientMessageReceived>(
                  Eq(ClientToServerMessageType_Abort), Eq(abort_message_size),
                  Eq(false), Ge(0))));

  ServerToClientWrapperMessage server_message;
  server_message.mutable_unmasking_request()
      ->mutable_dead_3_client_ids()
      ->Clear();  // Just to set it to an empty vector
  EXPECT_CALL(*sender, Send(_, _)).Times(0);
  for (int i = 0; i < 4; ++i) {
    if (i != 2) {
      EXPECT_CALL(*sender, Send(i, EqualsProto(server_message))).Times(1);
    }
  }
  EXPECT_CALL(*metrics, IndividualMessageSizes(
                            Eq(ServerToClientWrapperMessage::
                                   MessageContentCase::kUnmaskingRequest),
                            Eq(server_message.ByteSizeLong())))
      .Times(3);

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::R3_UNMASKING));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedAfterSendingMaskedInput(),
      Eq(1));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedBeforeSendingMaskedInput(),
      Eq(0));
  EXPECT_THAT(next_state.value()->NumberOfClientsTerminatedWithoutUnmasking(),
              Eq(0));
}

TEST_P(SecaggServerR2MaskedInputCollStateTest,
       ServerAndClientAbortsAreRecordedCorrectly) {
  // In this test clients abort for a variety of reasons, and then ultimately
  // the server aborts. Metrics should record all of these events.
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR2MaskedInputCollState state(
      CreateSecAggServerProtocolImpl(2, 7, sender.get(), metrics,
                                     GetParam().enable_async_r2),
      0,  // number_of_clients_failed_after_sending_masked_input
      0,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_CALL(*metrics,
              ClientsDropped(Eq(ClientStatus::DEAD_AFTER_SHARE_KEYS_RECEIVED),
                             Eq(ClientDropReason::SENT_ABORT_MESSAGE)));
  EXPECT_CALL(*metrics,
              ClientsDropped(
                  Eq(ClientStatus::DEAD_AFTER_MASKED_INPUT_RESPONSE_RECEIVED),
                  Eq(ClientDropReason::MASKED_INPUT_UNEXPECTED)));
  EXPECT_CALL(*metrics,
              ClientsDropped(Eq(ClientStatus::DEAD_AFTER_SHARE_KEYS_RECEIVED),
                             Eq(ClientDropReason::UNEXPECTED_MESSAGE_TYPE)));
  EXPECT_CALL(*metrics,
              ClientsDropped(Eq(ClientStatus::DEAD_AFTER_SHARE_KEYS_RECEIVED),
                             Eq(ClientDropReason::INVALID_MASKED_INPUT)))
      .Times(3);
  EXPECT_CALL(
      *metrics,
      ProtocolOutcomes(Eq(SecAggServerOutcome::NOT_ENOUGH_CLIENTS_REMAINING)));

  auto abort_message = std::make_unique<ClientToServerWrapperMessage>();
  abort_message->mutable_abort()->set_diagnostic_info("Aborting for test");

  ClientToServerWrapperMessage valid_message;
  MaskedInputVector encoded_vector;
  SecAggVector masked_vector(std::vector<uint64_t>(4, 9), 32);
  encoded_vector.set_encoded_vector(masked_vector.GetAsPackedBytes());
  (*valid_message.mutable_masked_input_response()
        ->mutable_vectors())["foobar"] = encoded_vector;

  auto invalid_message_too_many_vectors =
      std::make_unique<ClientToServerWrapperMessage>();
  (*invalid_message_too_many_vectors->mutable_masked_input_response()
        ->mutable_vectors())["extra"] = encoded_vector;

  auto invalid_message_wrong_name =
      std::make_unique<ClientToServerWrapperMessage>();
  (*invalid_message_wrong_name->mutable_masked_input_response()
        ->mutable_vectors())["wrong"] = encoded_vector;

  auto invalid_message_wrong_size =
      std::make_unique<ClientToServerWrapperMessage>();
  MaskedInputVector large_encoded_vector;
  SecAggVector large_masked_vector(std::vector<uint64_t>(7, 9), 32);
  large_encoded_vector.set_encoded_vector(
      large_masked_vector.GetAsPackedBytes());
  (*invalid_message_wrong_size->mutable_masked_input_response()
        ->mutable_vectors())["foobar"] = large_encoded_vector;

  auto wrong_message = std::make_unique<ClientToServerWrapperMessage>();
  wrong_message->mutable_advertise_keys();  // wrong type of message

  state.HandleMessage(0, std::move(abort_message)).IgnoreError();
  state
      .HandleMessage(
          1, std::make_unique<ClientToServerWrapperMessage>(valid_message))
      .IgnoreError();
  state
      .HandleMessage(
          1, std::make_unique<ClientToServerWrapperMessage>(valid_message))
      .IgnoreError();
  state.HandleMessage(2, std::move(invalid_message_too_many_vectors))
      .IgnoreError();
  state.HandleMessage(3, std::move(invalid_message_wrong_name)).IgnoreError();
  state.HandleMessage(4, std::move(invalid_message_wrong_size)).IgnoreError();
  state.HandleMessage(5, std::move(wrong_message)).IgnoreError();

  if (GetParam().enable_async_r2) {
    RunSchedulers();
    EXPECT_THAT(state.ReadyForNextRound(), IsTrue());
  }

  state.ProceedToNextRound().IgnoreError();  // causes server abort
}

TEST_P(SecaggServerR2MaskedInputCollStateTest, MetricsAreRecorded) {
  // In this test, clients 0 through 2 send in valid masked inputs, and then we
  // proceed to the next step even without client 3.
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR2MaskedInputCollState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get(), metrics,
                                     GetParam().enable_async_r2),
      0,  // number_of_clients_failed_after_sending_masked_input
      0,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_CALL(*metrics, ClientResponseTimes(
                            Eq(ClientToServerWrapperMessage::
                                   MessageContentCase::kMaskedInputResponse),
                            Ge(0)))
      .Times(3);

  for (int i = 0; i < 4; ++i) {
    EXPECT_THAT(state.NeedsToAbort(), IsFalse());
    EXPECT_THAT(state.NumberOfAliveClients(), Eq(4));
    EXPECT_THAT(state.NumberOfClientsReadyForNextRound(), Eq(i));
    EXPECT_THAT(state.NumberOfMessagesReceivedInThisRound(), Eq(i));
    EXPECT_THAT(state.NumberOfPendingClients(), Eq(4 - i));
    if (i < 3) {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(3 - i));
    } else {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(0));
    }
    if (GetParam().enable_async_r2) {
      EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
    } else {
      EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 3));
    }
    if (i < 3) {
      // Have client send a vector of the correct size to the server
      auto client_message = std::make_unique<ClientToServerWrapperMessage>();
      MaskedInputVector encoded_vector;
      SecAggVector masked_vector(std::vector<uint64_t>(4, i + 1), 32);
      encoded_vector.set_encoded_vector(masked_vector.GetAsPackedBytes());
      (*client_message->mutable_masked_input_response()
            ->mutable_vectors())["foobar"] = encoded_vector;
      ASSERT_THAT(state.HandleMessage(i, std::move(client_message)), IsOk());
      if (GetParam().enable_async_r2) {
        EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
      } else {
        EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 2));
      }
    }
  }

  if (GetParam().enable_async_r2) {
    RunSchedulers();
    EXPECT_THAT(state.ReadyForNextRound(), IsTrue());
  }

  ServerToClientWrapperMessage server_message;
  // TODO(team): 4 -> 3 below, once backwards compatibility not needed.
  server_message.mutable_unmasking_request()->add_dead_3_client_ids(4);
  ServerToClientWrapperMessage abort_message;
  abort_message.mutable_abort()->set_early_success(false);
  abort_message.mutable_abort()->set_diagnostic_info(
      "Client did not send MaskedInputCollectionResponse before round "
      "transition.");
  EXPECT_CALL(*sender, Send(_, _)).Times(0);
  for (int i = 0; i < 3; ++i) {
    EXPECT_CALL(*sender, Send(i, EqualsProto(server_message))).Times(1);
  }
  EXPECT_CALL(*sender, Send(3, EqualsProto(abort_message))).Times(1);
  EXPECT_CALL(*metrics,
              RoundTimes(Eq(SecAggServerStateKind::R2_MASKED_INPUT_COLLECTION),
                         Eq(true), Ge(0)));
  EXPECT_CALL(
      *metrics,
      RoundSurvivingClients(
          Eq(SecAggServerStateKind::R2_MASKED_INPUT_COLLECTION), Eq(3)));

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::R3_UNMASKING));
}

INSTANTIATE_TEST_SUITE_P(
    SecaggServerR2MaskedInputCollStateTests,
    SecaggServerR2MaskedInputCollStateTest,
    ::testing::ValuesIn<SecAggR2StateTestParams>(
        {{"r2_async_processing_enabled", true},
         {"r2_async_processing_disabled", false}}),
    [](const ::testing::TestParamInfo<
        SecaggServerR2MaskedInputCollStateTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace secagg
}  // namespace fcp
