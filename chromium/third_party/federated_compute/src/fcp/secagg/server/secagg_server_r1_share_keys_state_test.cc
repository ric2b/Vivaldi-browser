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

#include "fcp/secagg/server/secagg_server_r1_share_keys_state.h"

#include <memory>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "fcp/secagg/server/aes/aes_secagg_server_protocol_impl.h"
#include "fcp/secagg/server/secagg_server_state.h"
#include "fcp/secagg/server/secret_sharing_graph_factory.h"
#include "fcp/secagg/shared/aes_ctr_prng_factory.h"
#include "fcp/secagg/shared/compute_session_id.h"
#include "fcp/secagg/shared/ecdh_keys.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/secagg/shared/shamir_secret_sharing.h"
#include "fcp/secagg/testing/ecdh_pregenerated_test_keys.h"
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

// Default test session_id.
SessionId session_id = {"session id number, 32 bytes long"};

std::unique_ptr<AesSecAggServerProtocolImpl> CreateSecAggServerProtocolImpl(
    int minimum_number_of_clients_to_proceed, int total_number_of_clients,
    MockSendToClientsInterface* sender,
    MockSecAggServerMetricsListener* metrics_listener = nullptr) {
  auto input_vector_specs = std::vector<InputVectorSpecification>();
  SecretSharingGraphFactory factory;
  input_vector_specs.push_back(InputVectorSpecification("foobar", 4, 32));
  auto impl = std::make_unique<AesSecAggServerProtocolImpl>(
      factory.CreateCompleteGraph(total_number_of_clients,
                                  minimum_number_of_clients_to_proceed),
      minimum_number_of_clients_to_proceed, input_vector_specs,
      std::unique_ptr<MockSecAggServerMetricsListener>(metrics_listener),
      std::make_unique<AesCtrPrngFactory>(), sender,
      std::make_unique<SecAggScheduler>(
          /*sequential_scheduler=*/nullptr,
          /*parallel_scheduler=*/nullptr),
      std::vector<ClientStatus>(total_number_of_clients,
                                ClientStatus::ADVERTISE_KEYS_RECEIVED),
      ServerVariant::NATIVE_V1);
  impl->set_session_id(std::make_unique<SessionId>(session_id));
  EcdhPregeneratedTestKeys ecdh_keys;
  for (int i = 0; i < total_number_of_clients; i++) {
    impl->SetPairwisePublicKeys(i, ecdh_keys.GetPublicKey(i));
  }
  return impl;
}

TEST(SecaggServerR1ShareKeysStateTest, IsAbortedReturnsFalse) {
  auto sender = std::make_shared<MockSendToClientsInterface>();

  SecAggServerR1ShareKeysState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get()),
      0,  // number_of_clients_failed_after_sending_masked_input
      0,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_THAT(state.IsAborted(), IsFalse());
}

TEST(SecaggServerR1ShareKeysStateTest, IsCompletedSuccessfullyReturnsFalse) {
  auto sender = std::make_shared<MockSendToClientsInterface>();

  SecAggServerR1ShareKeysState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get()),
      0,  // number_of_clients_failed_after_sending_masked_input
      0,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_THAT(state.IsCompletedSuccessfully(), IsFalse());
}

TEST(SecaggServerR1ShareKeysStateTest, ErrorMessageRaisesErrorStatus) {
  auto sender = std::make_shared<MockSendToClientsInterface>();

  SecAggServerR1ShareKeysState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get()),
      0,  // number_of_clients_failed_after_sending_masked_input
      0,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_THAT(state.ErrorMessage().ok(), IsFalse());
}

TEST(SecaggServerR1ShareKeysStateTest, ResultRaisesErrorStatus) {
  auto sender = std::make_shared<MockSendToClientsInterface>();

  SecAggServerR1ShareKeysState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get()),
      0,  // number_of_clients_failed_after_sending_masked_input
      0,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_THAT(state.Result().ok(), IsFalse());
}

TEST(SecaggServerR1ShareKeysStateTest,
     AbortReturnsValidStateAndNotifiesClients) {
  TestTracingRecorder tracing_recorder;
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();
  auto sender = std::make_shared<MockSendToClientsInterface>();

  SecAggServerR1ShareKeysState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get(), metrics),
      0,  // number_of_clients_failed_after_sending_masked_input
      0,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  ServerToClientWrapperMessage abort_message;
  abort_message.mutable_abort()->set_early_success(false);
  abort_message.mutable_abort()->set_diagnostic_info("test abort reason");

  EXPECT_CALL(*metrics,
              ProtocolOutcomes(Eq(SecAggServerOutcome::EXTERNAL_REQUEST)));
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

TEST(SecaggServerR1ShareKeysStateTest,
     StateProceedsCorrectlyWithAllClientsValid) {
  // In this test, all clients send inputs for the correct clients, and then the
  // server proceeds to the next state. (The inputs aren't actually encrypted
  // shared keys, but that doesn't matter for this test.)
  auto sender = std::make_shared<MockSendToClientsInterface>();

  SecAggServerR1ShareKeysState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get()),
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
      EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
    } else {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(0));
      EXPECT_THAT(state.ReadyForNextRound(), IsTrue());
    }
    if (i < 4) {
      // Have one client send the right vector of "encrypted keys" to the
      // server.
      ClientToServerWrapperMessage client_message;
      for (int j = 0; j < 4; ++j) {
        if (i == j) {
          client_message.mutable_share_keys_response()
              ->add_encrypted_key_shares("");
        } else {
          client_message.mutable_share_keys_response()
              ->add_encrypted_key_shares(
                  absl::StrCat("encrypted key shares from ", i, " to ", j));
        }
      }
      ASSERT_THAT(state.HandleMessage(i, client_message), IsOk());
      EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 2));
    }
  }
  std::vector<ServerToClientWrapperMessage> server_messages(4);
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      if (i == j) {
        server_messages[i]
            .mutable_masked_input_request()
            ->add_encrypted_key_shares("");
      } else {
        server_messages[i]
            .mutable_masked_input_request()
            ->add_encrypted_key_shares(
                absl::StrCat("encrypted key shares from ", j, " to ", i));
      }
    }
    EXPECT_CALL(*sender, Send(Eq(i), EqualsProto(server_messages[i]))).Times(1);
  }

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::R2_MASKED_INPUT_COLLECTION));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedAfterSendingMaskedInput(),
      Eq(0));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedBeforeSendingMaskedInput(),
      Eq(0));
  EXPECT_THAT(next_state.value()->NumberOfClientsTerminatedWithoutUnmasking(),
              Eq(0));
}

TEST(SecaggServerR1ShareKeysStateTest,
     StateProceedsCorrectlyWithOnePreviousDropout) {
  // In this test, client 3 dropped out in round 0, so clients should not send
  // key shares for it. All other clients proceed normally.
  auto sender = std::make_shared<MockSendToClientsInterface>();
  auto impl = CreateSecAggServerProtocolImpl(3, 4, sender.get());
  impl->set_client_status(3, ClientStatus::DEAD_BEFORE_SENDING_ANYTHING);

  SecAggServerR1ShareKeysState state(
      std::move(impl),
      0,  // number_of_clients_failed_after_sending_masked_input
      1,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  for (int i = 0; i < 4; ++i) {
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
      // Have one client send the right vector of "encrypted keys" to the
      // server.
      ClientToServerWrapperMessage client_message;
      for (int j = 0; j < 4; ++j) {
        if (i == j || j == 3) {
          client_message.mutable_share_keys_response()
              ->add_encrypted_key_shares("");
        } else {
          client_message.mutable_share_keys_response()
              ->add_encrypted_key_shares(
                  absl::StrCat("encrypted key shares from ", i, " to ", j));
        }
      }
      ASSERT_THAT(state.HandleMessage(i, client_message), IsOk());
      EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 2));
    }
  }
  std::vector<ServerToClientWrapperMessage> server_messages(3);
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 4; ++j) {
      if (i == j || j == 3) {
        server_messages[i]
            .mutable_masked_input_request()
            ->add_encrypted_key_shares("");
      } else {
        server_messages[i]
            .mutable_masked_input_request()
            ->add_encrypted_key_shares(
                absl::StrCat("encrypted key shares from ", j, " to ", i));
      }
    }
    EXPECT_CALL(*sender, Send(Eq(i), EqualsProto(server_messages[i]))).Times(1);
  }
  EXPECT_CALL(*sender, Send(Eq(3), _)).Times(0);

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::R2_MASKED_INPUT_COLLECTION));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedAfterSendingMaskedInput(),
      Eq(0));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedBeforeSendingMaskedInput(),
      Eq(1));
  EXPECT_THAT(next_state.value()->NumberOfClientsTerminatedWithoutUnmasking(),
              Eq(0));
}

TEST(SecaggServerR1ShareKeysStateTest,
     StateProceedsCorrectlyWithAnAbortAfterSendingShares) {
  // In this test, all clients send inputs for the correct clients, but then
  // client 2 aborts. This should cause that client's message shared keys not to
  // appear in the messages sent later.
  auto sender = std::make_shared<MockSendToClientsInterface>();

  SecAggServerR1ShareKeysState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get()),
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
      EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
    } else {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(0));
      EXPECT_THAT(state.ReadyForNextRound(), IsTrue());
    }
    if (i < 4) {
      // Have one client send the right vector of "encrypted key shares" to
      // the server.
      ClientToServerWrapperMessage client_message;
      for (int j = 0; j < 4; ++j) {
        if (i == j) {
          client_message.mutable_share_keys_response()
              ->add_encrypted_key_shares("");
        } else {
          client_message.mutable_share_keys_response()
              ->add_encrypted_key_shares(
                  absl::StrCat("encrypted key shares from ", i, " to ", j));
        }
      }
      ASSERT_THAT(state.HandleMessage(i, client_message), IsOk());
      EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 2));
    }
  }

  ClientToServerWrapperMessage abort_message;
  abort_message.mutable_abort()->set_diagnostic_info("aborting for test");
  ASSERT_THAT(state.HandleMessage(2, abort_message), IsOk());
  EXPECT_THAT(state.ReadyForNextRound(), IsTrue());

  std::vector<ServerToClientWrapperMessage> server_messages(4);
  for (int i = 0; i < 4; ++i) {
    if (i == 2) {
      EXPECT_CALL(*sender, Send(Eq(2), _)).Times(0);
      continue;
    }
    for (int j = 0; j < 4; ++j) {
      if (i == j || j == 2) {
        server_messages[i]
            .mutable_masked_input_request()
            ->add_encrypted_key_shares("");
      } else {
        server_messages[i]
            .mutable_masked_input_request()
            ->add_encrypted_key_shares(
                absl::StrCat("encrypted key shares from ", j, " to ", i));
      }
    }
    EXPECT_CALL(*sender, Send(Eq(i), EqualsProto(server_messages[i]))).Times(1);
  }

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::R2_MASKED_INPUT_COLLECTION));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedAfterSendingMaskedInput(),
      Eq(0));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedBeforeSendingMaskedInput(),
      Eq(1));
  EXPECT_THAT(next_state.value()->NumberOfClientsTerminatedWithoutUnmasking(),
              Eq(0));
}

TEST(SecaggServerR1ShareKeysStateTest,
     StateProceedsCorrectlyWithOneClientSendingInvalidShares) {
  // In this test, all clients send encrypted shares, but client 0 omits an
  // encrypted share for client 1. This should force client 0 to abort.
  auto sender = std::make_shared<MockSendToClientsInterface>();

  SecAggServerR1ShareKeysState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get()),
      0,  // number_of_clients_failed_after_sending_masked_input
      0,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  std::vector<ServerToClientWrapperMessage> server_messages(4);
  server_messages[0].mutable_abort()->set_early_success(false);
  server_messages[0].mutable_abort()->set_diagnostic_info(
      "Client omitted a key share that was expected.");
  EXPECT_CALL(*sender, Send(Eq(0), EqualsProto(server_messages[0]))).Times(1);
  for (int i = 1; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      if (i == j || j == 0) {
        server_messages[i]
            .mutable_masked_input_request()
            ->add_encrypted_key_shares("");
      } else {
        server_messages[i]
            .mutable_masked_input_request()
            ->add_encrypted_key_shares(
                absl::StrCat("encrypted key shares from ", j, " to ", i));
      }
    }
    EXPECT_CALL(*sender, Send(Eq(i), EqualsProto(server_messages[i]))).Times(1);
  }

  ClientToServerWrapperMessage bad_message;
  bad_message.mutable_share_keys_response()->add_encrypted_key_shares("");
  bad_message.mutable_share_keys_response()->add_encrypted_key_shares("");
  bad_message.mutable_share_keys_response()->add_encrypted_key_shares(
      "encrypted key shares from 0 to 2");
  bad_message.mutable_share_keys_response()->add_encrypted_key_shares(
      "encrypted key shares from 0 to 3");
  ASSERT_THAT(state.HandleMessage(0, bad_message), IsOk());
  EXPECT_THAT(state.ReadyForNextRound(), IsFalse());

  for (int i = 1; i < 5; ++i) {
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
      // Have one client send the right vector of "encrypted key shares" to
      // the server.
      ClientToServerWrapperMessage client_message;
      for (int j = 0; j < 4; ++j) {
        if (i == j) {
          client_message.mutable_share_keys_response()
              ->add_encrypted_key_shares("");
        } else {
          client_message.mutable_share_keys_response()
              ->add_encrypted_key_shares(
                  absl::StrCat("encrypted key shares from ", i, " to ", j));
        }
      }
      ASSERT_THAT(state.HandleMessage(i, client_message), IsOk());
      EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 3));
    }
  }

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::R2_MASKED_INPUT_COLLECTION));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedAfterSendingMaskedInput(),
      Eq(0));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedBeforeSendingMaskedInput(),
      Eq(1));
  EXPECT_THAT(next_state.value()->NumberOfClientsTerminatedWithoutUnmasking(),
              Eq(0));
}

TEST(SecaggServerR1ShareKeysStateTest, StateAbortsIfTooManyClientsAbort) {
  // In this test, clients 0 and 1 send abort messages. This should cause the
  // server state to register that it needs to abort immediately.
  TestTracingRecorder tracing_recorder;
  auto sender = std::make_shared<MockSendToClientsInterface>();

  SecAggServerR1ShareKeysState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get()),
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
      ClientToServerWrapperMessage abort_message;
      abort_message.mutable_abort()->set_diagnostic_info("Aborting for test");
      ASSERT_THAT(state.HandleMessage(i, abort_message), IsOk());
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

TEST(SecaggServerR1ShareKeysStateTest, MetricsRecordsMessageSizes) {
  // In this test, all clients send inputs for the correct clients, and then the
  // server proceeds to the next state. (The inputs aren't actually encrypted
  // shared keys, but that doesn't matter for this test.)
  TestTracingRecorder tracing_recorder;
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();
  auto sender = std::make_shared<MockSendToClientsInterface>();

  SecAggServerR1ShareKeysState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get(), metrics),
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
      EXPECT_THAT(state.ReadyForNextRound(), IsFalse());
    } else {
      EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(0));
      EXPECT_THAT(state.ReadyForNextRound(), IsTrue());
    }
    if (i < 4) {
      // Have one client send the right vector of "encrypted keys" to the
      // server.
      ClientToServerWrapperMessage client_message;
      for (int j = 0; j < 4; ++j) {
        if (i == j) {
          client_message.mutable_share_keys_response()
              ->add_encrypted_key_shares("");
        } else {
          client_message.mutable_share_keys_response()
              ->add_encrypted_key_shares(
                  absl::StrCat("encrypted key shares from ", i, " to ", j));
        }
      }
      EXPECT_CALL(*metrics, MessageReceivedSizes(
                                Eq(ClientToServerWrapperMessage::
                                       MessageContentCase::kShareKeysResponse),
                                Eq(true), Eq(client_message.ByteSizeLong())));
      ASSERT_THAT(state.HandleMessage(i, client_message), IsOk());
      EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 2));
      EXPECT_THAT(tracing_recorder.root()[i],
                  IsEvent<ClientMessageReceived>(
                      Eq(ClientToServerMessageType_ShareKeysResponse),
                      Eq(client_message.ByteSizeLong()), Eq(true), Ge(0)));
    }
  }
  std::vector<ServerToClientWrapperMessage> server_messages(4);
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      if (i == j) {
        server_messages[i]
            .mutable_masked_input_request()
            ->add_encrypted_key_shares("");
      } else {
        server_messages[i]
            .mutable_masked_input_request()
            ->add_encrypted_key_shares(
                absl::StrCat("encrypted key shares from ", j, " to ", i));
      }
    }
    EXPECT_CALL(*sender, Send(Eq(i), EqualsProto(server_messages[i])));
  }
  EXPECT_CALL(*metrics, IndividualMessageSizes(
                            Eq(ServerToClientWrapperMessage::
                                   MessageContentCase::kMaskedInputRequest),
                            Eq(server_messages[0].ByteSizeLong())))
      .Times(4);

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::R2_MASKED_INPUT_COLLECTION));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedAfterSendingMaskedInput(),
      Eq(0));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedBeforeSendingMaskedInput(),
      Eq(0));
  EXPECT_THAT(next_state.value()->NumberOfClientsTerminatedWithoutUnmasking(),
              Eq(0));
  EXPECT_THAT(
      tracing_recorder.FindAllEvents<IndividualMessageSent>(),
      ElementsAre(IsEvent<IndividualMessageSent>(
                      0, Eq(ServerToClientMessageType_MaskedInputRequest),
                      Eq(server_messages[0].ByteSizeLong())),
                  IsEvent<IndividualMessageSent>(
                      1, Eq(ServerToClientMessageType_MaskedInputRequest),
                      Eq(server_messages[1].ByteSizeLong())),
                  IsEvent<IndividualMessageSent>(
                      2, Eq(ServerToClientMessageType_MaskedInputRequest),
                      Eq(server_messages[2].ByteSizeLong())),
                  IsEvent<IndividualMessageSent>(
                      3, Eq(ServerToClientMessageType_MaskedInputRequest),
                      Eq(server_messages[3].ByteSizeLong()))));
}

TEST(SecaggServerR1ShareKeysStateTest,
     ServerAndClientAbortsAreRecordedCorrectly) {
  // In this test clients abort for a variety of reasons, and then ultimately
  // the server aborts. Metrics should record all of these events.
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();
  auto sender = std::make_shared<MockSendToClientsInterface>();

  SecAggServerR1ShareKeysState state(
      CreateSecAggServerProtocolImpl(2, 7, sender.get(), metrics),
      0,  // number_of_clients_failed_after_sending_masked_input
      0,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_CALL(
      *metrics,
      ClientsDropped(Eq(ClientStatus::DEAD_AFTER_ADVERTISE_KEYS_RECEIVED),
                     Eq(ClientDropReason::SENT_ABORT_MESSAGE)));
  EXPECT_CALL(
      *metrics,
      ClientsDropped(Eq(ClientStatus::DEAD_AFTER_ADVERTISE_KEYS_RECEIVED),
                     Eq(ClientDropReason::SHARE_KEYS_UNEXPECTED)));
  EXPECT_CALL(
      *metrics,
      ClientsDropped(Eq(ClientStatus::DEAD_AFTER_ADVERTISE_KEYS_RECEIVED),
                     Eq(ClientDropReason::UNEXPECTED_MESSAGE_TYPE)));
  EXPECT_CALL(
      *metrics,
      ClientsDropped(Eq(ClientStatus::DEAD_AFTER_ADVERTISE_KEYS_RECEIVED),
                     Eq(ClientDropReason::INVALID_SHARE_KEYS_RESPONSE)))
      .Times(3);
  EXPECT_CALL(
      *metrics,
      ProtocolOutcomes(Eq(SecAggServerOutcome::NOT_ENOUGH_CLIENTS_REMAINING)));

  ClientToServerWrapperMessage abort_message;
  abort_message.mutable_abort()->set_diagnostic_info("Aborting for test");
  ClientToServerWrapperMessage valid_message;  // from client 1
  for (int j = 0; j < 7; ++j) {
    if (1 == j) {
      valid_message.mutable_share_keys_response()->add_encrypted_key_shares("");
    } else {
      valid_message.mutable_share_keys_response()->add_encrypted_key_shares(
          absl::StrCat("encrypted key shares from ", 1, " to ", j));
    }
  }

  ClientToServerWrapperMessage invalid_message_wrong_number;  // from client 2
  for (int j = 0; j <= 7; ++j) {  // goes one past the end
    if (2 == j) {
      invalid_message_wrong_number.mutable_share_keys_response()
          ->add_encrypted_key_shares("");
    } else {
      invalid_message_wrong_number.mutable_share_keys_response()
          ->add_encrypted_key_shares(
              absl::StrCat("encrypted key shares from ", 2, " to ", j));
    }
  }

  ClientToServerWrapperMessage invalid_message_missing_share;  // from client 3
  for (int j = 0; j < 7; ++j) {
    if (3 == j || 0 == j) {  // missing share for 0
      invalid_message_missing_share.mutable_share_keys_response()
          ->add_encrypted_key_shares("");
    } else {
      invalid_message_missing_share.mutable_share_keys_response()
          ->add_encrypted_key_shares(
              absl::StrCat("encrypted key shares from ", 3, " to ", j));
    }
  }

  ClientToServerWrapperMessage invalid_message_extra_share;  // from client 4
  for (int j = 0; j < 7; ++j) {
    // including share for self, which is wrong
    invalid_message_extra_share.mutable_share_keys_response()
        ->add_encrypted_key_shares(
            absl::StrCat("encrypted key shares from ", 4, " to ", j));
  }

  ClientToServerWrapperMessage wrong_message;
  wrong_message.mutable_advertise_keys();  // wrong type of message

  state.HandleMessage(0, abort_message).IgnoreError();
  state.HandleMessage(1, valid_message).IgnoreError();
  state.HandleMessage(1, valid_message).IgnoreError();
  state.HandleMessage(2, invalid_message_wrong_number).IgnoreError();
  state.HandleMessage(3, invalid_message_missing_share).IgnoreError();
  state.HandleMessage(4, invalid_message_extra_share).IgnoreError();
  state.HandleMessage(5, wrong_message).IgnoreError();
  state.ProceedToNextRound().IgnoreError();  // causes server abort
}

TEST(SecaggServerR1ShareKeysStateTest, MetricsAreRecorded) {
  // In this test, all clients send inputs for the correct clients, and then the
  // server proceeds to the next state. (The inputs aren't actually encrypted
  // shared keys, but that doesn't matter for this test.)
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();
  auto sender = std::make_shared<MockSendToClientsInterface>();

  SecAggServerR1ShareKeysState state(
      CreateSecAggServerProtocolImpl(3, 4, sender.get(), metrics),
      0,  // number_of_clients_failed_after_sending_masked_input
      0,  // number_of_clients_failed_before_sending_masked_input
      0   // number_of_clients_terminated_without_unmasking
  );

  EXPECT_CALL(*metrics, ClientResponseTimes(
                            Eq(ClientToServerWrapperMessage::
                                   MessageContentCase::kShareKeysResponse),
                            Ge(0)))
      .Times(4);

  for (int i = 0; i < 5; ++i) {
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
      // Have one client send the right vector of "encrypted keys" to the
      // server.
      ClientToServerWrapperMessage client_message;
      for (int j = 0; j < 4; ++j) {
        if (i == j) {
          client_message.mutable_share_keys_response()
              ->add_encrypted_key_shares("");
        } else {
          client_message.mutable_share_keys_response()
              ->add_encrypted_key_shares(
                  absl::StrCat("encrypted key shares from ", i, " to ", j));
        }
      }
      ASSERT_THAT(state.HandleMessage(i, client_message), IsOk());
      EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 2));
    }
  }
  std::vector<ServerToClientWrapperMessage> server_messages(4);
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      if (i == j) {
        server_messages[i]
            .mutable_masked_input_request()
            ->add_encrypted_key_shares("");
      } else {
        server_messages[i]
            .mutable_masked_input_request()
            ->add_encrypted_key_shares(
                absl::StrCat("encrypted key shares from ", j, " to ", i));
      }
    }
    EXPECT_CALL(*sender, Send(Eq(i), EqualsProto(server_messages[i]))).Times(1);
  }
  EXPECT_CALL(*metrics, RoundTimes(Eq(SecAggServerStateKind::R1_SHARE_KEYS),
                                   Eq(true), Ge(0)));
  EXPECT_CALL(*metrics, RoundSurvivingClients(
                            Eq(SecAggServerStateKind::R1_SHARE_KEYS), Eq(4)));

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::R2_MASKED_INPUT_COLLECTION));
}
}  // namespace
}  // namespace secagg
}  // namespace fcp
