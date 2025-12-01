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

#include "fcp/secagg/server/secagg_server_r3_unmasking_state.h"

#include <memory>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "fcp/base/monitoring.h"
#include "fcp/secagg/server/aes/aes_secagg_server_protocol_impl.h"
#include "fcp/secagg/server/secagg_server_state.h"
#include "fcp/secagg/server/secret_sharing_graph_factory.h"
#include "fcp/secagg/shared/aes_ctr_prng_factory.h"
#include "fcp/secagg/shared/ecdh_keys.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/secagg/testing/ecdh_pregenerated_test_keys.h"
#include "fcp/secagg/testing/fake_prng.h"
#include "fcp/secagg/testing/server/mock_secagg_server_metrics_listener.h"
#include "fcp/secagg/testing/server/mock_send_to_clients_interface.h"
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
using ::testing::Ne;

// Default test session_id.
SessionId session_id = {"session id number, 32 bytes long"};

std::unique_ptr<AesSecAggServerProtocolImpl> CreateSecAggServerProtocolImpl(
    int minimum_number_of_clients_to_proceed, int total_number_of_clients,
    MockSendToClientsInterface* sender,
    MockSecAggServerMetricsListener* metrics_listener = nullptr) {
  auto input_vector_specs = std::vector<InputVectorSpecification>();
  input_vector_specs.push_back(InputVectorSpecification("foobar", 4, 32));
  SecretSharingGraphFactory factory;
  auto impl = std::make_unique<AesSecAggServerProtocolImpl>(
      factory.CreateCompleteGraph(total_number_of_clients,
                                  minimum_number_of_clients_to_proceed),
      minimum_number_of_clients_to_proceed, input_vector_specs,
      std::unique_ptr<MockSecAggServerMetricsListener>(metrics_listener),
      std::make_unique<AesCtrPrngFactory>(), sender,
      nullptr,  // prng_runner
      std::vector<ClientStatus>(total_number_of_clients,
                                ClientStatus::MASKED_INPUT_RESPONSE_RECEIVED),
      ServerVariant::NATIVE_V1);
  impl->set_session_id(std::make_unique<SessionId>(session_id));
  EcdhPregeneratedTestKeys ecdh_keys;

  for (int i = 0; i < total_number_of_clients; ++i) {
    impl->SetPairwisePublicKeys(i, ecdh_keys.GetPublicKey(i));
  }

  return impl;
}

TEST(SecaggServerR3UnmaskingStateTest, IsAbortedReturnsFalse) {
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR3UnmaskingState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get()),
      0,   // number_of_clients_failed_after_sending_masked_input
      0,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  EXPECT_THAT(state.IsAborted(), IsFalse());
}

TEST(SecaggServerR3UnmaskingStateTest, IsCompletedSuccessfullyReturnsFalse) {
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR3UnmaskingState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get()),
      0,   // number_of_clients_failed_after_sending_masked_input
      0,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  EXPECT_THAT(state.IsCompletedSuccessfully(), IsFalse());
}

TEST(SecaggServerR3UnmaskingStateTest, ErrorMessageRaisesErrorStatus) {
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR3UnmaskingState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get()),
      0,   // number_of_clients_failed_after_sending_masked_input
      0,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  EXPECT_THAT(state.ErrorMessage().ok(), IsFalse());
}

TEST(SecaggServerR3UnmaskingStateTest, ResultRaisesErrorStatus) {
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR3UnmaskingState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get()),
      0,   // number_of_clients_failed_after_sending_masked_input
      0,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  EXPECT_THAT(state.Result().ok(), IsFalse());
}

TEST(SecaggServerR3UnmaskingStateTest,
     AbortClientAfterUnmaskingResponseReceived) {
  auto sender = std::make_unique<MockSendToClientsInterface>();
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();
  auto impl = CreateSecAggServerProtocolImpl(3, 4, sender.get(), metrics);
  impl->set_client_status(2, ClientStatus::UNMASKING_RESPONSE_RECEIVED);
  SecAggServerR3UnmaskingState state(
      std::move(impl),
      0,   // number_of_clients_failed_after_sending_masked_input
      0,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  state.AbortClient(2, "close client message.",
                    ClientDropReason::SENT_ABORT_MESSAGE, false);
  ASSERT_THAT(state.NumberOfClientsFailedAfterSendingMaskedInput(), Eq(0));
  // Metrics are not logged
  EXPECT_CALL(*metrics, ClientsDropped(_, _)).Times(0);
  // Client is not notified
  EXPECT_CALL(*sender, Send(_, _)).Times(0);
  ASSERT_THAT(state.AbortedClientIds().contains(2), Eq(true));
}

TEST(SecaggServerR3UnmaskingStateTest,
     AbortReturnsValidStateAndNotifiesClients) {
  TestTracingRecorder tracing_recorder;
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR3UnmaskingState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get(), metrics),
      0,   // number_of_clients_failed_after_sending_masked_input
      0,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

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

TEST(SecaggServerR3UnmaskingStateTest,
     StateProceedsCorrectlyWithNoAbortsAndAllCorrectMessagesReceived) {
  // In this test, no clients abort or aborted at any point, and all four
  // clients send unmasking responses to the server before ProceedToNextRound is
  // called.
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR3UnmaskingState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get()),
      0,   // number_of_clients_failed_after_sending_masked_input
      0,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  // Set up correct responses
  std::vector<ClientToServerWrapperMessage> unmasking_responses(4);
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      NoiseOrPrfKeyShare* share = unmasking_responses[i]
                                      .mutable_unmasking_response()
                                      ->add_noise_or_prf_key_shares();
      share->set_prf_sk_share(
          absl::StrCat("Test key share for client ", j, " from client ", i));
    }
  }

  // No clients should actually get a message in this round.
  EXPECT_CALL(*sender, Send(_, _)).Times(0);

  // i is the number of messages received so far
  for (int i = 0; i <= 4; ++i) {
    EXPECT_THAT(state.NeedsToAbort(), IsFalse());
    EXPECT_THAT(state.NumberOfAliveClients(), Eq(4));
    EXPECT_THAT(state.NumberOfClientsReadyForNextRound(), Eq(i));
    EXPECT_THAT(state.NumberOfMessagesReceivedInThisRound(), Eq(i));
    EXPECT_THAT(state.NumberOfPendingClients(), Eq(4 - i));
    if (i < 3) {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(3 - i));
      EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
    } else {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(0));
      EXPECT_THAT(state.ReadyForNextRound(), IsTrue());
    }
    if (i < 4) {
      ASSERT_THAT(state.HandleMessage(i, unmasking_responses[i]), IsOk());
      EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 2));
    }
  }

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::PRNG_RUNNING));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedAfterSendingMaskedInput(),
      Eq(0));
  EXPECT_THAT(next_state.value()->NumberOfClientsTerminatedWithoutUnmasking(),
              Eq(0));
}

TEST(SecaggServerR3UnmaskingStateTest,
     StateProceedsCorrectlyWithMinimumCorrectMessagesReceived) {
  // In this test, no clients abort or aborted at any point, but
  // ProceedToNextRound is called after only 3 clients have submitted masked
  // input responses. This is perfectly valid because the threshold is 3.
  auto sender = std::make_unique<MockSendToClientsInterface>();
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();

  SecAggServerR3UnmaskingState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get(), metrics),
      0,   // number_of_clients_failed_after_sending_masked_input
      0,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  // Set up correct responses
  std::vector<ClientToServerWrapperMessage> unmasking_responses(4);
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      NoiseOrPrfKeyShare* share = unmasking_responses[i]
                                      .mutable_unmasking_response()
                                      ->add_noise_or_prf_key_shares();
      share->set_prf_sk_share(
          absl::StrCat("Test key share for client ", j, " from client ", i));
    }
  }

  // Only client 3 should get a message this round.
  ServerToClientWrapperMessage abort_message;
  abort_message.mutable_abort()->set_early_success(true);
  abort_message.mutable_abort()->set_diagnostic_info(
      "Client did not send unmasking response but protocol completed "
      "successfully.");
  EXPECT_CALL(*sender, Send(_, _)).Times(0);
  EXPECT_CALL(*sender, Send(3, EqualsProto(abort_message))).Times(1);
  EXPECT_CALL(*sender, Send(Ne(3), _)).Times(0);
  EXPECT_CALL(*metrics,
              ClientsDropped(
                  Eq(ClientStatus::DEAD_AFTER_MASKED_INPUT_RESPONSE_RECEIVED),
                  Eq(ClientDropReason::EARLY_SUCCESS)));

  // i is the number of messages received so far. Stop after 3
  for (int i = 0; i <= 3; ++i) {
    EXPECT_THAT(state.NeedsToAbort(), IsFalse());
    EXPECT_THAT(state.NumberOfAliveClients(), Eq(4));
    EXPECT_THAT(state.NumberOfClientsReadyForNextRound(), Eq(i));
    EXPECT_THAT(state.NumberOfMessagesReceivedInThisRound(), Eq(i));
    EXPECT_THAT(state.NumberOfPendingClients(), Eq(4 - i));
    if (i < 3) {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(3 - i));
      EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
    } else {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(0));
      EXPECT_THAT(state.ReadyForNextRound(), IsTrue());
    }
    if (i < 3) {
      ASSERT_THAT(state.HandleMessage(i, unmasking_responses[i]), IsOk());
      EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 2));
    }
  }

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::PRNG_RUNNING));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedAfterSendingMaskedInput(),
      Eq(0));
  EXPECT_THAT(next_state.value()->NumberOfClientsTerminatedWithoutUnmasking(),
              Eq(1));
}

TEST(SecaggServerR3UnmaskingStateTest, StateProceedsCorrectlyWithOneFailure) {
  // In this test, no clients abort or aborted at any point, but client 0 sends
  // an invalid message. It should be aborted, but the other 3 clients should be
  // enough to proceed.
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR3UnmaskingState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get()),
      0,   // number_of_clients_failed_after_sending_masked_input
      0,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  // Set up correct responses
  std::vector<ClientToServerWrapperMessage> unmasking_responses(4);
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      NoiseOrPrfKeyShare* share = unmasking_responses[i]
                                      .mutable_unmasking_response()
                                      ->add_noise_or_prf_key_shares();
      share->set_prf_sk_share(
          absl::StrCat("Test key share for client ", j, " from client ", i));
    }
  }
  // Add an incorrect response.
  unmasking_responses[0]
      .mutable_unmasking_response()
      ->mutable_noise_or_prf_key_shares(2)
      ->set_noise_sk_share("This is the wrong type of share!");

  // Only client 0 should get a message this round.
  ServerToClientWrapperMessage abort_message;
  abort_message.mutable_abort()->set_diagnostic_info(
      "Client did not include the correct type of key share.");
  abort_message.mutable_abort()->set_early_success(false);
  EXPECT_CALL(*sender, Send(0, EqualsProto(abort_message))).Times(1);
  EXPECT_CALL(*sender, Send(Ne(0), _)).Times(0);

  EXPECT_THAT(state.HandleMessage(0, unmasking_responses[0]), IsOk());
  EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
  EXPECT_THAT(state.NumberOfClientsFailedAfterSendingMaskedInput(), Eq(1));
  EXPECT_THAT(state.AbortedClientIds().contains(0), IsTrue());

  // i is the number of messages received so far.
  for (int i = 1; i <= 4; ++i) {
    EXPECT_THAT(state.NeedsToAbort(), IsFalse());
    EXPECT_THAT(state.NumberOfAliveClients(), Eq(3));
    EXPECT_THAT(state.NumberOfClientsReadyForNextRound(), Eq(i - 1));
    EXPECT_THAT(state.NumberOfMessagesReceivedInThisRound(), Eq(i - 1));
    EXPECT_THAT(state.NumberOfPendingClients(), Eq(4 - i));
    if (i < 4) {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(4 - i));
      EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
    } else {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(0));
      EXPECT_THAT(state.ReadyForNextRound(), IsTrue());
    }
    if (i < 4) {
      ASSERT_THAT(state.HandleMessage(i, unmasking_responses[i]), IsOk());
      EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 3));
    }
  }

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::PRNG_RUNNING));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedAfterSendingMaskedInput(),
      Eq(1));
  EXPECT_THAT(next_state.value()->NumberOfClientsTerminatedWithoutUnmasking(),
              Eq(0));
}

TEST(SecaggServerR3UnmaskingStateTest,
     StateProceedsCorrectlyWithAnAbortInRound2) {
  // In this test, client 3 never sent a masked input, so clients should send
  // the pairwise key share for client 3.
  auto sender = std::make_unique<MockSendToClientsInterface>();
  auto impl = CreateSecAggServerProtocolImpl(3, 4, sender.get());
  impl->set_client_status(3, ClientStatus::DEAD_AFTER_SHARE_KEYS_RECEIVED);

  SecAggServerR3UnmaskingState state(
      std::move(impl),
      0,   // number_of_clients_failed_after_sending_masked_input
      1,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  // Set up correct responses
  std::vector<ClientToServerWrapperMessage> unmasking_responses(3);
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      NoiseOrPrfKeyShare* share = unmasking_responses[i]
                                      .mutable_unmasking_response()
                                      ->add_noise_or_prf_key_shares();
      share->set_prf_sk_share(
          absl::StrCat("Test key share for client ", j, " from client ", i));
    }
    NoiseOrPrfKeyShare* share = unmasking_responses[i]
                                    .mutable_unmasking_response()
                                    ->add_noise_or_prf_key_shares();
    share->set_noise_sk_share(
        absl::StrCat("Test key share for client ", 3, " from client ", i));
  }

  // No clients should actually get a message in this round.
  EXPECT_CALL(*sender, Send(_, _)).Times(0);

  // i is the number of messages received so far
  for (int i = 0; i <= 3; ++i) {
    EXPECT_THAT(state.NeedsToAbort(), IsFalse());
    EXPECT_THAT(state.NumberOfAliveClients(), Eq(3));
    EXPECT_THAT(state.NumberOfClientsReadyForNextRound(), Eq(i));
    EXPECT_THAT(state.NumberOfMessagesReceivedInThisRound(), Eq(i));
    EXPECT_THAT(state.NumberOfPendingClients(), Eq(3 - i));
    if (i < 3) {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(3 - i));
      EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
    } else {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(0));
      EXPECT_THAT(state.ReadyForNextRound(), IsTrue());
    }
    if (i < 3) {
      ASSERT_THAT(state.HandleMessage(i, unmasking_responses[i]), IsOk());
      EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 2));
    }
  }

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::PRNG_RUNNING));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedAfterSendingMaskedInput(),
      Eq(0));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedBeforeSendingMaskedInput(),
      Eq(1));
  EXPECT_THAT(next_state.value()->NumberOfClientsTerminatedWithoutUnmasking(),
              Eq(0));
}

TEST(SecaggServerR3UnmaskingStateTest,
     StateProceedsCorrectlyWithAnAbortInRound1) {
  // In this test, client 3 never even finished the key share round, so the
  // other clients should send no key share for client 3.
  auto sender = std::make_unique<MockSendToClientsInterface>();
  auto impl = CreateSecAggServerProtocolImpl(3, 4, sender.get());
  impl->set_client_status(3, ClientStatus::DEAD_AFTER_ADVERTISE_KEYS_RECEIVED);

  SecAggServerR3UnmaskingState state(
      std::move(impl),
      0,   // number_of_clients_failed_after_sending_masked_input
      1,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  // Set up correct responses
  std::vector<ClientToServerWrapperMessage> unmasking_responses(3);
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      NoiseOrPrfKeyShare* share = unmasking_responses[i]
                                      .mutable_unmasking_response()
                                      ->add_noise_or_prf_key_shares();
      share->set_prf_sk_share(
          absl::StrCat("Test key share for client ", j, " from client ", i));
    }
    unmasking_responses[i]
        .mutable_unmasking_response()
        ->add_noise_or_prf_key_shares();
  }

  // No clients should actually get a message in this round.
  EXPECT_CALL(*sender, Send(_, _)).Times(0);

  // i is the number of messages received so far
  for (int i = 0; i <= 3; ++i) {
    EXPECT_THAT(state.NeedsToAbort(), IsFalse());
    EXPECT_THAT(state.NumberOfAliveClients(), Eq(3));
    EXPECT_THAT(state.NumberOfClientsReadyForNextRound(), Eq(i));
    EXPECT_THAT(state.NumberOfMessagesReceivedInThisRound(), Eq(i));
    EXPECT_THAT(state.NumberOfPendingClients(), Eq(3 - i));
    if (i < 3) {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(3 - i));
      EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
    } else {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(0));
      EXPECT_THAT(state.ReadyForNextRound(), IsTrue());
    }
    if (i < 3) {
      ASSERT_THAT(state.HandleMessage(i, unmasking_responses[i]), IsOk());
      EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 2));
    }
  }

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::PRNG_RUNNING));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedAfterSendingMaskedInput(),
      Eq(0));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedBeforeSendingMaskedInput(),
      Eq(1));
  EXPECT_THAT(next_state.value()->NumberOfClientsTerminatedWithoutUnmasking(),
              Eq(0));
}

TEST(SecaggServerR3UnmaskingStateTest,
     StateProceedsCorrectlyEvenIfClientsAbortAfterSendingMessage) {
  // In this test, clients 0 and 1 send valid messages but then abort. But since
  // they sent valid messages, the server should proceed regardless.
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR3UnmaskingState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get()),
      0,   // number_of_clients_failed_after_sending_masked_input
      0,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  // Set up correct responses
  std::vector<ClientToServerWrapperMessage> unmasking_responses(4);
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      NoiseOrPrfKeyShare* share = unmasking_responses[i]
                                      .mutable_unmasking_response()
                                      ->add_noise_or_prf_key_shares();
      share->set_prf_sk_share(
          absl::StrCat("Test key share for client ", j, " from client ", i));
    }
  }

  // No clients should actually get a message in this round.
  EXPECT_CALL(*sender, Send(_, _)).Times(0);

  ClientToServerWrapperMessage abort_message;
  abort_message.mutable_abort();

  // i is the number of messages received so far
  for (int i = 0; i <= 4; ++i) {
    EXPECT_THAT(state.NeedsToAbort(), IsFalse());
    EXPECT_THAT(state.NumberOfAliveClients(), Eq(4));
    EXPECT_THAT(state.NumberOfClientsReadyForNextRound(), Eq(i));
    EXPECT_THAT(state.NumberOfMessagesReceivedInThisRound(), Eq(i));
    EXPECT_THAT(state.NumberOfPendingClients(), Eq(4 - i));
    if (i < 3) {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(3 - i));
      EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
    } else {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(0));
      EXPECT_THAT(state.ReadyForNextRound(), IsTrue());
    }
    if (i < 4) {
      ASSERT_THAT(state.HandleMessage(i, unmasking_responses[i]), IsOk());
      EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 2));
    }
  }
  // These should not change anything.
  EXPECT_THAT(state.HandleMessage(0, abort_message), IsOk());
  EXPECT_THAT(state.ReadyForNextRound(), IsTrue());
  EXPECT_THAT(state.HandleMessage(1, abort_message), IsOk());
  EXPECT_THAT(state.ReadyForNextRound(), IsTrue());
  EXPECT_THAT(state.NumberOfAliveClients(), Eq(4));
  EXPECT_THAT(state.NumberOfClientsReadyForNextRound(), Eq(4));
  EXPECT_THAT(state.NumberOfMessagesReceivedInThisRound(), Eq(4));
  EXPECT_THAT(state.ReadyForNextRound(), IsTrue());

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::PRNG_RUNNING));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedAfterSendingMaskedInput(),
      Eq(0));
  EXPECT_THAT(next_state.value()->NumberOfClientsTerminatedWithoutUnmasking(),
              Eq(0));
}

TEST(SecaggServerR3UnmaskingStateTest, StateAbortsIfTooManyClientsAbort) {
  // In this test, clients 0 and 1 send abort messages rather than valid
  // unmasking responses, so the server must abort
  TestTracingRecorder tracing_recorder;
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR3UnmaskingState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get()),
      0,   // number_of_clients_failed_after_sending_masked_input
      0,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  // Set up correct responses
  std::vector<ClientToServerWrapperMessage> unmasking_responses(4);
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      NoiseOrPrfKeyShare* share = unmasking_responses[i]
                                      .mutable_unmasking_response()
                                      ->add_noise_or_prf_key_shares();
      share->set_prf_sk_share(
          absl::StrCat("Test key share for client ", j, " from client ", i));
    }
  }

  ServerToClientWrapperMessage server_abort_message;
  server_abort_message.mutable_abort()->set_diagnostic_info(
      "Too many clients aborted.");
  server_abort_message.mutable_abort()->set_early_success(false);
  EXPECT_CALL(*sender, Send(_, _)).Times(0);
  for (int i = 0; i < 4; ++i) {
    EXPECT_CALL(*sender, Send(i, EqualsProto(server_abort_message)))
        .Times(static_cast<int>(i >= 2));
  }

  ClientToServerWrapperMessage client_abort_message;
  client_abort_message.mutable_abort();

  ASSERT_THAT(state.HandleMessage(0, client_abort_message), IsOk());
  EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
  EXPECT_THAT(state.NeedsToAbort(), IsFalse());
  EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
  ASSERT_THAT(state.HandleMessage(1, client_abort_message), IsOk());
  EXPECT_THAT(state.ReadyForNextRound(), IsTrue());
  EXPECT_THAT(state.NeedsToAbort(), IsTrue());
  EXPECT_THAT(state.ReadyForNextRound(), IsTrue());

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(), Eq(SecAggServerStateKind::ABORTED));
  EXPECT_THAT(next_state.value()->ErrorMessage(), IsOk());
  EXPECT_THAT(next_state.value()->ErrorMessage().value(),
              Eq("Too many clients aborted."));
  EXPECT_THAT(tracing_recorder.FindAllEvents<BroadcastMessageSent>(),
              ElementsAre(IsEvent<BroadcastMessageSent>(
                  Eq(ServerToClientMessageType_Abort),
                  Eq(server_abort_message.ByteSizeLong()))));
}

TEST(SecaggServerR3UnmaskingStateTest, MetricsRecordsMessageSizes) {
  // In this test, client 3 never sent a masked input, so clients should send
  // the pairwise key share for client 3.
  TestTracingRecorder tracing_recorder;
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();
  auto sender = std::make_unique<MockSendToClientsInterface>();
  auto impl = CreateSecAggServerProtocolImpl(3, 4, sender.get(), metrics);
  impl->set_client_status(3, ClientStatus::DEAD_AFTER_SHARE_KEYS_RECEIVED);

  SecAggServerR3UnmaskingState state(
      std::move(impl),
      0,   // number_of_clients_failed_after_sending_masked_input
      1,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  // Set up correct responses
  std::vector<ClientToServerWrapperMessage> unmasking_responses(3);
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      NoiseOrPrfKeyShare* share = unmasking_responses[i]
                                      .mutable_unmasking_response()
                                      ->add_noise_or_prf_key_shares();
      share->set_prf_sk_share(
          absl::StrCat("Test key share for client ", j, " from client ", i));
    }
    NoiseOrPrfKeyShare* share = unmasking_responses[i]
                                    .mutable_unmasking_response()
                                    ->add_noise_or_prf_key_shares();
    share->set_noise_sk_share(
        absl::StrCat("Test key share for client ", 3, " from client ", i));
  }

  // No clients should actually get a message in this round.
  EXPECT_CALL(*sender, Send(_, _)).Times(0);
  EXPECT_CALL(
      *metrics,
      MessageReceivedSizes(Eq(ClientToServerWrapperMessage::MessageContentCase::
                                  kUnmaskingResponse),
                           Eq(true), Eq(unmasking_responses[0].ByteSizeLong())))
      .Times(3);

  // i is the number of messages received so far
  for (int i = 0; i <= 3; ++i) {
    EXPECT_THAT(state.NeedsToAbort(), IsFalse());
    EXPECT_THAT(state.NumberOfAliveClients(), Eq(3));
    EXPECT_THAT(state.NumberOfClientsReadyForNextRound(), Eq(i));
    EXPECT_THAT(state.NumberOfMessagesReceivedInThisRound(), Eq(i));
    EXPECT_THAT(state.NumberOfPendingClients(), Eq(3 - i));
    if (i < 3) {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(3 - i));
      EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
    } else {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(0));
      EXPECT_THAT(state.ReadyForNextRound(), IsTrue());
    }
    if (i < 3) {
      ASSERT_THAT(state.HandleMessage(i, unmasking_responses[i]), IsOk());
      EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 2));
      EXPECT_THAT(
          tracing_recorder.root()[i],
          IsEvent<ClientMessageReceived>(
              Eq(ClientToServerMessageType_UnmaskingResponse),
              Eq(unmasking_responses[i].ByteSizeLong()), Eq(true), Ge(0)));
    }
  }

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::PRNG_RUNNING));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedAfterSendingMaskedInput(),
      Eq(0));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedBeforeSendingMaskedInput(),
      Eq(1));
  EXPECT_THAT(next_state.value()->NumberOfClientsTerminatedWithoutUnmasking(),
              Eq(0));
}

TEST(SecaggServerR3UnmaskingStateTest,
     ServerAndClientAbortsAreRecordedCorrectly) {
  // In this test clients abort for a variety of reasons, and then ultimately
  // the server aborts. Metrics should record all of these events.
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();
  auto sender = std::make_unique<MockSendToClientsInterface>();
  auto impl = CreateSecAggServerProtocolImpl(2, 8, sender.get(), metrics);
  impl->ErasePublicKeysForClient(7);
  impl->set_client_status(6, ClientStatus::DEAD_AFTER_SHARE_KEYS_RECEIVED);
  impl->set_client_status(7, ClientStatus::DEAD_BEFORE_SENDING_ANYTHING);

  SecAggServerR3UnmaskingState state(
      std::move(impl),
      0,   // number_of_clients_failed_after_sending_masked_input
      2,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  EXPECT_CALL(*metrics,
              ClientsDropped(
                  Eq(ClientStatus::DEAD_AFTER_MASKED_INPUT_RESPONSE_RECEIVED),
                  Eq(ClientDropReason::SENT_ABORT_MESSAGE)));
  EXPECT_CALL(*metrics,
              ClientsDropped(
                  Eq(ClientStatus::DEAD_AFTER_UNMASKING_RESPONSE_RECEIVED), _))
      .Times(0);
  EXPECT_CALL(*metrics,
              ClientsDropped(
                  Eq(ClientStatus::DEAD_AFTER_MASKED_INPUT_RESPONSE_RECEIVED),
                  Eq(ClientDropReason::UNEXPECTED_MESSAGE_TYPE)));
  EXPECT_CALL(*metrics,
              ClientsDropped(
                  Eq(ClientStatus::DEAD_AFTER_MASKED_INPUT_RESPONSE_RECEIVED),
                  Eq(ClientDropReason::INVALID_UNMASKING_RESPONSE)))
      .Times(3);
  EXPECT_CALL(
      *metrics,
      ProtocolOutcomes(Eq(SecAggServerOutcome::NOT_ENOUGH_CLIENTS_REMAINING)));

  ClientToServerWrapperMessage abort_message;
  abort_message.mutable_abort()->set_diagnostic_info("Aborting for test");

  ClientToServerWrapperMessage valid_message;  // from client 1
  for (int j = 0; j < 6; ++j) {
    NoiseOrPrfKeyShare* share = valid_message.mutable_unmasking_response()
                                    ->add_noise_or_prf_key_shares();
    share->set_prf_sk_share(
        absl::StrCat("Test key share for client ", j, " from client 1"));
  }
  NoiseOrPrfKeyShare* share =
      valid_message.mutable_unmasking_response()->add_noise_or_prf_key_shares();
  share->set_noise_sk_share(
      absl::StrCat("Test key share for client ", 6, " from client 1"));
  share =
      valid_message.mutable_unmasking_response()->add_noise_or_prf_key_shares();

  ClientToServerWrapperMessage invalid_noise_instead_of_prf;  // from client 2
  for (int j = 0; j < 5; ++j) {
    share = invalid_noise_instead_of_prf.mutable_unmasking_response()
                ->add_noise_or_prf_key_shares();
    share->set_prf_sk_share(
        absl::StrCat("Test key share for client ", j, " from client 2"));
  }
  for (int j = 5; j < 7; ++j) {  // client 5 should not be included here
    share = invalid_noise_instead_of_prf.mutable_unmasking_response()
                ->add_noise_or_prf_key_shares();
    share->set_noise_sk_share(
        absl::StrCat("Test key share for client ", j, " from client 2"));
  }
  share = invalid_noise_instead_of_prf.mutable_unmasking_response()
              ->add_noise_or_prf_key_shares();

  ClientToServerWrapperMessage invalid_prf_instead_of_noise;  // from client 3
  for (int j = 0; j < 7; ++j) {  // client 6 should not be included here
    share = invalid_prf_instead_of_noise.mutable_unmasking_response()
                ->add_noise_or_prf_key_shares();
    share->set_prf_sk_share(
        absl::StrCat("Test key share for client ", j, " from client 3"));
  }
  share = invalid_prf_instead_of_noise.mutable_unmasking_response()
              ->add_noise_or_prf_key_shares();

  ClientToServerWrapperMessage invalid_noise_instead_of_blank;  // from client 4
  for (int j = 0; j < 6; ++j) {
    share = invalid_noise_instead_of_blank.mutable_unmasking_response()
                ->add_noise_or_prf_key_shares();
    share->set_prf_sk_share(
        absl::StrCat("Test key share for client ", j, " from client 4"));
  }
  for (int j = 6; j < 8; ++j) {  // client 7 should not be included here
    share = invalid_noise_instead_of_blank.mutable_unmasking_response()
                ->add_noise_or_prf_key_shares();
    share->set_noise_sk_share(
        absl::StrCat("Test key share for client ", j, " from client 4"));
  }

  ClientToServerWrapperMessage wrong_message;
  wrong_message.mutable_advertise_keys();  // wrong type of message

  state.HandleMessage(0, abort_message).IgnoreError();
  state.HandleMessage(1, valid_message).IgnoreError();
  state.HandleMessage(1, valid_message).IgnoreError();
  state.HandleMessage(2, invalid_noise_instead_of_prf).IgnoreError();
  state.HandleMessage(3, invalid_prf_instead_of_noise).IgnoreError();
  state.HandleMessage(4, invalid_noise_instead_of_blank).IgnoreError();
  state.HandleMessage(5, wrong_message).IgnoreError();
  state.ProceedToNextRound().IgnoreError();  // causes server abort
}

TEST(SecaggServerR3UnmaskingStateTest, MetricsAreRecorded) {
  // In this test, no clients abort or aborted at any point, but
  // ProceedToNextRound is called after only 3 clients have submitted masked
  // input responses. This is perfectly valid because the threshold is 3.
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR3UnmaskingState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get(), metrics),
      0,   // number_of_clients_failed_after_sending_masked_input
      0,   // number_of_clients_failed_before_sending_masked_input
      0);  // number_of_clients_terminated_without_unmasking

  // Set up correct responses
  std::vector<ClientToServerWrapperMessage> unmasking_responses(4);
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      NoiseOrPrfKeyShare* share = unmasking_responses[i]
                                      .mutable_unmasking_response()
                                      ->add_noise_or_prf_key_shares();
      share->set_prf_sk_share(
          absl::StrCat("Test key share for client ", j, " from client ", i));
    }
  }

  // Only client 3 should get a message this round.
  ServerToClientWrapperMessage abort_message;
  abort_message.mutable_abort()->set_diagnostic_info(
      "Client did not send unmasking response but protocol completed "
      "successfully.");
  abort_message.mutable_abort()->set_early_success(true);
  EXPECT_CALL(*sender, Send(3, EqualsProto(abort_message))).Times(1);
  EXPECT_CALL(*sender, Send(Ne(3), _)).Times(0);

  EXPECT_CALL(*metrics, RoundTimes(Eq(SecAggServerStateKind::R3_UNMASKING),
                                   Eq(true), Ge(0)));
  EXPECT_CALL(*metrics, RoundSurvivingClients(
                            Eq(SecAggServerStateKind::R3_UNMASKING), Eq(3)));
  EXPECT_CALL(*metrics, ClientResponseTimes(
                            Eq(ClientToServerWrapperMessage::
                                   MessageContentCase::kUnmaskingResponse),
                            Ge(0)))
      .Times(3);
  EXPECT_CALL(*metrics,
              ClientsDropped(
                  Eq(ClientStatus::DEAD_AFTER_MASKED_INPUT_RESPONSE_RECEIVED),
                  Eq(ClientDropReason::EARLY_SUCCESS)));

  // i is the number of messages received so far. Stop after 3
  for (int i = 0; i <= 3; ++i) {
    EXPECT_THAT(state.NeedsToAbort(), IsFalse());
    EXPECT_THAT(state.NumberOfAliveClients(), Eq(4));
    EXPECT_THAT(state.NumberOfClientsReadyForNextRound(), Eq(i));
    EXPECT_THAT(state.NumberOfMessagesReceivedInThisRound(), Eq(i));
    EXPECT_THAT(state.NumberOfPendingClients(), Eq(4 - i));
    if (i < 3) {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(3 - i));
      EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
    } else {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(0));
      EXPECT_THAT(state.ReadyForNextRound(), IsTrue());
    }
    if (i < 3) {
      ASSERT_THAT(state.HandleMessage(i, unmasking_responses[i]), IsOk());
      EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 2));
    }
  }

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::PRNG_RUNNING));
}

}  // namespace
}  // namespace secagg
}  // namespace fcp
