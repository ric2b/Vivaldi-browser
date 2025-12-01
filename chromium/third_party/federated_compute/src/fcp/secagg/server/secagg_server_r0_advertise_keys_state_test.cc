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

#include "fcp/secagg/server/secagg_server_r0_advertise_keys_state.h"

#include <memory>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "fcp/secagg/server/aes/aes_secagg_server_protocol_impl.h"
#include "fcp/secagg/server/secagg_server_enums.pb.h"
#include "fcp/secagg/server/secagg_server_state.h"
#include "fcp/secagg/server/secret_sharing_graph_factory.h"
#include "fcp/secagg/shared/aes_ctr_prng_factory.h"
#include "fcp/secagg/shared/compute_session_id.h"
#include "fcp/secagg/shared/ecdh_keys.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
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

std::unique_ptr<AesSecAggServerProtocolImpl> CreateAesSecAggServerProtocolImpl(
    MockSendToClientsInterface* sender,
    MockSecAggServerMetricsListener* metrics_listener = nullptr) {
  auto input_vector_specs = std::vector<InputVectorSpecification>();
  input_vector_specs.push_back(InputVectorSpecification("foobar", 4, 32));
  SecretSharingGraphFactory factory;

  return std::make_unique<AesSecAggServerProtocolImpl>(
      factory.CreateCompleteGraph(4, 3),  // total number of clients is 4
      3,  // minimum_number_of_clients_to_proceed
      input_vector_specs,
      std::unique_ptr<MockSecAggServerMetricsListener>(metrics_listener),
      std::make_unique<AesCtrPrngFactory>(), sender,
      nullptr,  // prng_runner
      std::vector<ClientStatus>(4, ClientStatus::READY_TO_START),
      ServerVariant::NATIVE_V1);
}

TEST(SecaggServerR0AdvertiseKeysStateTest, IsAbortedReturnsFalse) {
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR0AdvertiseKeysState state(
      CreateAesSecAggServerProtocolImpl(sender.get()));

  EXPECT_THAT(state.IsAborted(), IsFalse());
}

TEST(SecaggServerR0AdvertiseKeysStateTest,
     IsCompletedSuccessfullyReturnsFalse) {
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR0AdvertiseKeysState state(
      CreateAesSecAggServerProtocolImpl(sender.get()));

  EXPECT_THAT(state.IsCompletedSuccessfully(), IsFalse());
}

TEST(SecaggServerR0AdvertiseKeysStateTest, ErrorMessageRaisesErrorStatus) {
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR0AdvertiseKeysState state(
      CreateAesSecAggServerProtocolImpl(sender.get()));

  EXPECT_THAT(state.ErrorMessage().ok(), IsFalse());
}

TEST(SecaggServerR0AdvertiseKeysStateTest, ResultRaisesErrorStatus) {
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR0AdvertiseKeysState state(
      CreateAesSecAggServerProtocolImpl(sender.get()));

  EXPECT_THAT(state.Result().ok(), IsFalse());
}

TEST(SecaggServerR0AdvertiseKeysStateTest,
     AbortReturnsValidStateAndNotifiesClients) {
  TestTracingRecorder tracing_recorder;
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR0AdvertiseKeysState state(
      CreateAesSecAggServerProtocolImpl(sender.get(), metrics));

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

TEST(SecaggServerR0AdvertiseKeysStateTest,
     StateProceedsCorrectlyWithAllClientsValid) {
  // In this test, all clients send two valid ECDH public keys apiece, and then
  // the server proceeds to the next state.
  TestTracingRecorder tracing_recorder;
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR0AdvertiseKeysState state(
      CreateAesSecAggServerProtocolImpl(sender.get()));

  EcdhPregeneratedTestKeys ecdh_keys;
  auto pairwise_public_keys = std::make_unique<std::vector<EcdhPublicKey>>();
  std::vector<ClientToServerWrapperMessage> client_messages(4);
  ServerToClientWrapperMessage expected_server_message;
  for (int i = 0; i < 4; ++i) {
    PairOfPublicKeys* public_keys =
        expected_server_message.mutable_share_keys_request()
            ->add_pairs_of_public_keys();
    client_messages[i]
        .mutable_advertise_keys()
        ->mutable_pair_of_public_keys()
        ->set_enc_pk(ecdh_keys.GetPublicKeyString(i));
    client_messages[i]
        .mutable_advertise_keys()
        ->mutable_pair_of_public_keys()
        ->set_noise_pk(ecdh_keys.GetPublicKeyString(i + 4));
    public_keys->set_enc_pk(ecdh_keys.GetPublicKeyString(i));
    public_keys->set_noise_pk(ecdh_keys.GetPublicKeyString(i + 4));
  }
  expected_server_message.mutable_share_keys_request()->set_session_id(
      ComputeSessionId(expected_server_message.share_keys_request()).data);

  EXPECT_CALL(*sender, Send(_, _)).Times(0);
  for (int i = 0; i < 4; ++i) {
    EXPECT_CALL(*sender, Send(i, EqualsProto(expected_server_message)))
        .Times(1);
  }

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
      ASSERT_THAT(state.HandleMessage(i, client_messages[i]), IsOk());
      EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 2));
    }
  }

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::R1_SHARE_KEYS));
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
                              0, Eq(ServerToClientMessageType_ShareKeysRequest),
                              Eq(expected_server_message.ByteSizeLong())),
                          IsEvent<IndividualMessageSent>(
                              1, Eq(ServerToClientMessageType_ShareKeysRequest),
                              Eq(expected_server_message.ByteSizeLong())),
                          IsEvent<IndividualMessageSent>(
                              2, Eq(ServerToClientMessageType_ShareKeysRequest),
                              Eq(expected_server_message.ByteSizeLong())),
                          IsEvent<IndividualMessageSent>(
                              3, Eq(ServerToClientMessageType_ShareKeysRequest),
                              Eq(expected_server_message.ByteSizeLong()))));
}

TEST(SecaggServerR0AdvertiseKeysStateTest,
     StateProceedsCorrectlyWithInvalidKeysFromOneClient) {
  // In this test, client 3 sends invalid public keys, so it should be forced to
  // abort. But this should not stop the rest of the state proceeding normally.
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR0AdvertiseKeysState state(
      CreateAesSecAggServerProtocolImpl(sender.get()));

  EcdhPregeneratedTestKeys ecdh_keys;
  auto pairwise_public_keys = std::make_unique<std::vector<EcdhPublicKey>>();
  std::vector<ClientToServerWrapperMessage> client_messages(4);
  ServerToClientWrapperMessage expected_server_message;
  for (int i = 0; i < 3; ++i) {
    PairOfPublicKeys* public_keys =
        expected_server_message.mutable_share_keys_request()
            ->add_pairs_of_public_keys();
    client_messages[i]
        .mutable_advertise_keys()
        ->mutable_pair_of_public_keys()
        ->set_enc_pk(ecdh_keys.GetPublicKeyString(i));
    client_messages[i]
        .mutable_advertise_keys()
        ->mutable_pair_of_public_keys()
        ->set_noise_pk(ecdh_keys.GetPublicKeyString(i + 4));
    public_keys->set_enc_pk(ecdh_keys.GetPublicKeyString(i));
    public_keys->set_noise_pk(ecdh_keys.GetPublicKeyString(i + 4));
  }
  client_messages[3]
      .mutable_advertise_keys()
      ->mutable_pair_of_public_keys()
      ->set_enc_pk(ecdh_keys.GetPublicKeyString(3));
  client_messages[3]
      .mutable_advertise_keys()
      ->mutable_pair_of_public_keys()
      ->set_noise_pk("This is too long to be a valid key.");
  expected_server_message.mutable_share_keys_request()
      ->add_pairs_of_public_keys();  // this one will be empty

  expected_server_message.mutable_share_keys_request()->set_session_id(
      ComputeSessionId(expected_server_message.share_keys_request()).data);

  EXPECT_CALL(*sender, Send(_, _)).Times(0);
  for (int i = 0; i < 3; ++i) {
    EXPECT_CALL(*sender, Send(i, EqualsProto(expected_server_message)))
        .Times(1);
  }

  ServerToClientWrapperMessage abort_message;
  abort_message.mutable_abort()->set_early_success(false);
  abort_message.mutable_abort()->set_diagnostic_info(
      "A public key sent by the client was not the correct size.");

  EXPECT_CALL(*sender, Send(3, EqualsProto(abort_message)));

  for (int i = 0; i < 4; ++i) {
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
    ASSERT_THAT(state.HandleMessage(i, client_messages[i]), IsOk());
    EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 2));
  }
  EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(0));
  EXPECT_THAT(state.NeedsToAbort(), IsFalse());
  EXPECT_THAT(state.NumberOfAliveClients(), Eq(3));
  EXPECT_THAT(state.NumberOfClientsReadyForNextRound(), Eq(3));
  EXPECT_THAT(state.NumberOfMessagesReceivedInThisRound(), Eq(3));
  EXPECT_THAT(state.NumberOfPendingClients(), Eq(0));
  EXPECT_THAT(state.ReadyForNextRound(), IsTrue());

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::R1_SHARE_KEYS));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedAfterSendingMaskedInput(),
      Eq(0));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedBeforeSendingMaskedInput(),
      Eq(1));
  EXPECT_THAT(next_state.value()->NumberOfClientsTerminatedWithoutUnmasking(),
              Eq(0));
}

TEST(SecaggServerR0AdvertiseKeysStateTest,
     StateProceedsCorrectlyWithNoMessageFromOneClient) {
  // In this test, we proceed to the next state before client 3 sends any
  // message, so it should be forced to abort. But this should not stop the rest
  // of the state proceeding normally.
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR0AdvertiseKeysState state(
      CreateAesSecAggServerProtocolImpl(sender.get()));

  EcdhPregeneratedTestKeys ecdh_keys;
  auto pairwise_public_keys = std::make_unique<std::vector<EcdhPublicKey>>();
  std::vector<ClientToServerWrapperMessage> client_messages(3);
  ServerToClientWrapperMessage expected_server_message;
  for (int i = 0; i < 3; ++i) {
    PairOfPublicKeys* public_keys =
        expected_server_message.mutable_share_keys_request()
            ->add_pairs_of_public_keys();
    client_messages[i]
        .mutable_advertise_keys()
        ->mutable_pair_of_public_keys()
        ->set_enc_pk(ecdh_keys.GetPublicKeyString(i));
    client_messages[i]
        .mutable_advertise_keys()
        ->mutable_pair_of_public_keys()
        ->set_noise_pk(ecdh_keys.GetPublicKeyString(i + 4));
    public_keys->set_enc_pk(ecdh_keys.GetPublicKeyString(i));
    public_keys->set_noise_pk(ecdh_keys.GetPublicKeyString(i + 4));
  }
  expected_server_message.mutable_share_keys_request()
      ->add_pairs_of_public_keys();  // this one will be empty

  expected_server_message.mutable_share_keys_request()->set_session_id(
      ComputeSessionId(expected_server_message.share_keys_request()).data);

  EXPECT_CALL(*sender, Send(_, _)).Times(0);
  for (int i = 0; i < 3; ++i) {
    EXPECT_CALL(*sender, Send(i, EqualsProto(expected_server_message)))
        .Times(1);
  }
  ServerToClientWrapperMessage abort_message;
  abort_message.mutable_abort()->set_early_success(false);
  abort_message.mutable_abort()->set_diagnostic_info(
      "Client did not send AdvertiseKeys message before round transition.");

  EXPECT_CALL(*sender, Send(3, EqualsProto(abort_message)));

  for (int i = 0; i < 4; ++i) {
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
      ASSERT_THAT(state.HandleMessage(i, client_messages[i]), IsOk());
      EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 2));
    }
  }

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::R1_SHARE_KEYS));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedAfterSendingMaskedInput(),
      Eq(0));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedBeforeSendingMaskedInput(),
      Eq(1));
  EXPECT_THAT(next_state.value()->NumberOfClientsTerminatedWithoutUnmasking(),
              Eq(0));
}

TEST(SecaggServerR0AdvertiseKeysStateTest,
     StateNeedsToAbortIfTooManyClientsAbort) {
  // In this test, the first two clients send abort messages, so the server
  // should register that it needs to abort.
  TestTracingRecorder tracing_recorder;
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR0AdvertiseKeysState state(
      CreateAesSecAggServerProtocolImpl(sender.get()));

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

TEST(SecaggServerR0AdvertiseKeysStateTest,
     StateProceedsCorrectlyWithAllUncompressedClientMessages) {
  // In this test, all clients send two valid ECDH public keys apiece, and then
  // the server proceeds to the next state.
  auto sender = std::make_unique<MockSendToClientsInterface>();

  SecAggServerR0AdvertiseKeysState state(
      CreateAesSecAggServerProtocolImpl(sender.get()));

  EcdhPregeneratedTestKeys ecdh_keys;
  auto pairwise_public_keys = std::make_unique<std::vector<EcdhPublicKey>>();
  std::vector<ClientToServerWrapperMessage> client_messages(4);
  ServerToClientWrapperMessage expected_server_message;
  for (int i = 0; i < 4; ++i) {
    PairOfPublicKeys* public_keys =
        expected_server_message.mutable_share_keys_request()
            ->add_pairs_of_public_keys();
    client_messages[i]
        .mutable_advertise_keys()
        ->mutable_pair_of_public_keys()
        ->set_enc_pk(ecdh_keys.GetUncompressedPublicKeyString(i));
    client_messages[i]
        .mutable_advertise_keys()
        ->mutable_pair_of_public_keys()
        ->set_noise_pk(ecdh_keys.GetUncompressedPublicKeyString(i + 4));
    public_keys->set_enc_pk(ecdh_keys.GetUncompressedPublicKeyString(i));
    public_keys->set_noise_pk(ecdh_keys.GetUncompressedPublicKeyString(i + 4));
  }

  expected_server_message.mutable_share_keys_request()->set_session_id(
      ComputeSessionId(expected_server_message.share_keys_request()).data);

  EXPECT_CALL(*sender, Send(_, _)).Times(0);
  for (int i = 0; i < 4; ++i) {
    EXPECT_CALL(*sender, Send(i, EqualsProto(expected_server_message)))
        .Times(1);
  }

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
      ASSERT_THAT(state.HandleMessage(i, client_messages[i]), IsOk());
      EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 2));
    }
  }

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::R1_SHARE_KEYS));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedAfterSendingMaskedInput(),
      Eq(0));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedBeforeSendingMaskedInput(),
      Eq(0));
  EXPECT_THAT(next_state.value()->NumberOfClientsTerminatedWithoutUnmasking(),
              Eq(0));
}

TEST(SecaggServerR0AdvertiseKeysStateTest, MetricsRecordsStart) {
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();
  auto sender = std::make_unique<MockSendToClientsInterface>();

  EXPECT_CALL(*metrics, ProtocolStarts(_));

  SecAggServerR0AdvertiseKeysState state(
      CreateAesSecAggServerProtocolImpl(sender.get(), metrics));

  EXPECT_THAT(state.Result().ok(), IsFalse());
}

TEST(SecaggServerR0AdvertiseKeysStateTest, MetricsRecordsMessageSizes) {
  // In this test, client 3 sends invalid public keys, so it should be forced to
  // abort. But this should not stop the rest of the state proceeding normally.
  TestTracingRecorder tracing_recorder;
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();
  auto sender = std::make_unique<MockSendToClientsInterface>();

  EXPECT_CALL(*metrics, ProtocolStarts(_));

  SecAggServerR0AdvertiseKeysState state(
      CreateAesSecAggServerProtocolImpl(sender.get(), metrics));

  EcdhPregeneratedTestKeys ecdh_keys;
  auto pairwise_public_keys = std::make_unique<std::vector<EcdhPublicKey>>();
  std::vector<ClientToServerWrapperMessage> client_messages(4);
  ServerToClientWrapperMessage expected_server_message;
  for (int i = 0; i < 3; ++i) {
    PairOfPublicKeys* public_keys =
        expected_server_message.mutable_share_keys_request()
            ->add_pairs_of_public_keys();
    client_messages[i]
        .mutable_advertise_keys()
        ->mutable_pair_of_public_keys()
        ->set_enc_pk(ecdh_keys.GetPublicKeyString(i));
    client_messages[i]
        .mutable_advertise_keys()
        ->mutable_pair_of_public_keys()
        ->set_noise_pk(ecdh_keys.GetPublicKeyString(i + 4));
    public_keys->set_enc_pk(ecdh_keys.GetPublicKeyString(i));
    public_keys->set_noise_pk(ecdh_keys.GetPublicKeyString(i + 4));
  }
  client_messages[3]
      .mutable_advertise_keys()
      ->mutable_pair_of_public_keys()
      ->set_enc_pk(ecdh_keys.GetPublicKeyString(3));
  client_messages[3]
      .mutable_advertise_keys()
      ->mutable_pair_of_public_keys()
      ->set_noise_pk("This is too long to be a valid key.");
  expected_server_message.mutable_share_keys_request()
      ->add_pairs_of_public_keys();  // this one will be empty

  expected_server_message.mutable_share_keys_request()->set_session_id(
      ComputeSessionId(expected_server_message.share_keys_request()).data);

  EXPECT_CALL(*sender, Send(_, _)).Times(0);
  for (int i = 0; i < 3; ++i) {
    EXPECT_CALL(*sender, Send(i, EqualsProto(expected_server_message)))
        .Times(1);
  }
  ServerToClientWrapperMessage abort_message;
  abort_message.mutable_abort()->set_early_success(false);
  abort_message.mutable_abort()->set_diagnostic_info(
      "A public key sent by the client was not the correct size.");
  EXPECT_CALL(*sender, Send(3, EqualsProto(abort_message)));

  EXPECT_CALL(*metrics, IndividualMessageSizes(
                            Eq(ServerToClientWrapperMessage::
                                   MessageContentCase::kShareKeysRequest),
                            Eq(expected_server_message.ByteSizeLong())))
      .Times(3);
  EXPECT_CALL(*metrics,
              IndividualMessageSizes(
                  Eq(ServerToClientWrapperMessage::MessageContentCase::kAbort),
                  Eq(abort_message.ByteSizeLong())));
  EXPECT_CALL(
      *metrics,
      MessageReceivedSizes(
          Eq(ClientToServerWrapperMessage::MessageContentCase::kAdvertiseKeys),
          Eq(true), Eq(client_messages[0].ByteSizeLong())))
      .Times(3);
  EXPECT_CALL(
      *metrics,
      MessageReceivedSizes(
          Eq(ClientToServerWrapperMessage::MessageContentCase::kAdvertiseKeys),
          Eq(true), Eq(client_messages[3].ByteSizeLong())));

  for (int i = 0; i < 4; ++i) {
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
    ASSERT_THAT(state.HandleMessage(i, client_messages[i]), IsOk());
    EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 2));
    EXPECT_THAT(tracing_recorder.root()[i],
                IsEvent<ClientMessageReceived>(
                    Eq(ClientToServerMessageType_AdvertiseKeys),
                    Eq(client_messages[i].ByteSizeLong()), Eq(true), Ge(0)));
  }
  EXPECT_THAT(state.MinimumMessagesNeededForNextRound(), Eq(0));
  EXPECT_THAT(state.NeedsToAbort(), IsFalse());
  EXPECT_THAT(state.NumberOfAliveClients(), Eq(3));
  EXPECT_THAT(state.NumberOfClientsReadyForNextRound(), Eq(3));
  EXPECT_THAT(state.NumberOfMessagesReceivedInThisRound(), Eq(3));
  EXPECT_THAT(state.NumberOfPendingClients(), Eq(0));
  EXPECT_THAT(state.ReadyForNextRound(), IsTrue());

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::R1_SHARE_KEYS));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedAfterSendingMaskedInput(),
      Eq(0));
  EXPECT_THAT(
      next_state.value()->NumberOfClientsFailedBeforeSendingMaskedInput(),
      Eq(1));
  EXPECT_THAT(next_state.value()->NumberOfClientsTerminatedWithoutUnmasking(),
              Eq(0));
}

TEST(SecaggServerR0AdvertiseKeysStateTest,
     ServerAndClientAbortsAreRecordedCorrectly) {
  TestTracingRecorder tracing_recorder;
  // In this test clients abort for a variety of reasons, and then ultimately
  // the server aborts. Metrics should record all of these events.
  auto sender = std::make_unique<MockSendToClientsInterface>();
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();
  EcdhPregeneratedTestKeys ecdh_keys;

  SecAggServerR0AdvertiseKeysState state(
      CreateAesSecAggServerProtocolImpl(sender.get(), metrics));

  EXPECT_CALL(*metrics,
              ClientsDropped(Eq(ClientStatus::DEAD_BEFORE_SENDING_ANYTHING),
                             Eq(ClientDropReason::SENT_ABORT_MESSAGE)));
  EXPECT_CALL(*metrics,
              ClientsDropped(Eq(ClientStatus::DEAD_BEFORE_SENDING_ANYTHING),
                             Eq(ClientDropReason::ADVERTISE_KEYS_UNEXPECTED)));
  EXPECT_CALL(*metrics,
              ClientsDropped(Eq(ClientStatus::DEAD_BEFORE_SENDING_ANYTHING),
                             Eq(ClientDropReason::UNEXPECTED_MESSAGE_TYPE)));
  EXPECT_CALL(*metrics,
              ClientsDropped(Eq(ClientStatus::DEAD_BEFORE_SENDING_ANYTHING),
                             Eq(ClientDropReason::INVALID_PUBLIC_KEY)));
  EXPECT_CALL(
      *metrics,
      ProtocolOutcomes(Eq(SecAggServerOutcome::NOT_ENOUGH_CLIENTS_REMAINING)));

  ClientToServerWrapperMessage abort_message;
  abort_message.mutable_abort()->set_diagnostic_info("Aborting for test");
  ClientToServerWrapperMessage valid_message;
  valid_message.mutable_advertise_keys()
      ->mutable_pair_of_public_keys()
      ->set_enc_pk(ecdh_keys.GetPublicKeyString(0));
  valid_message.mutable_advertise_keys()
      ->mutable_pair_of_public_keys()
      ->set_noise_pk(ecdh_keys.GetPublicKeyString(4));
  ClientToServerWrapperMessage invalid_message;
  invalid_message.mutable_advertise_keys()
      ->mutable_pair_of_public_keys()
      ->set_enc_pk(ecdh_keys.GetPublicKeyString(3));
  invalid_message.mutable_advertise_keys()
      ->mutable_pair_of_public_keys()
      ->set_noise_pk("This is too long to be a valid key.");
  ClientToServerWrapperMessage wrong_message;
  wrong_message.mutable_share_keys_response();  // wrong type of message

  state.HandleMessage(0, abort_message).IgnoreError();
  state.HandleMessage(1, valid_message).IgnoreError();
  state.HandleMessage(1, valid_message).IgnoreError();
  state.HandleMessage(2, invalid_message).IgnoreError();
  state.HandleMessage(3, wrong_message).IgnoreError();
  state.ProceedToNextRound().IgnoreError();  // causes server abort

  EXPECT_THAT(tracing_recorder.FindAllEvents<SecAggProtocolOutcome>(),
              ElementsAre(IsEvent<SecAggProtocolOutcome>(
                  Eq(TracingSecAggServerOutcome_NotEnoughClientsRemaining))));
  EXPECT_THAT(
      tracing_recorder.FindAllEvents<ClientsDropped>(),
      ElementsAre(IsEvent<ClientsDropped>(
                      Eq(TracingClientStatus_DeadBeforeSendingAnything),
                      Eq(TracingClientDropReason_SentAbortMessage)),
                  IsEvent<ClientsDropped>(
                      Eq(TracingClientStatus_DeadBeforeSendingAnything),
                      Eq(TracingClientDropReason_AdvertiseKeysUnexpected)),
                  IsEvent<ClientsDropped>(
                      Eq(TracingClientStatus_DeadBeforeSendingAnything),
                      Eq(TracingClientDropReason_InvalidPublicKey)),
                  IsEvent<ClientsDropped>(
                      Eq(TracingClientStatus_DeadBeforeSendingAnything),
                      Eq(TracingClientDropReason_UnexpectedMessageType))));
}

TEST(SecaggServerR0AdvertiseKeysStateTest, MetricsAreRecorded) {
  // In this test, all clients send two valid ECDH public keys apiece, and then
  // the server proceeds to the next state.
  TestTracingRecorder tracing_recorder;
  MockSecAggServerMetricsListener* metrics =
      new MockSecAggServerMetricsListener();
  auto sender = std::make_unique<MockSendToClientsInterface>();
  SecAggServerR0AdvertiseKeysState state(
      CreateAesSecAggServerProtocolImpl(sender.get(), metrics));

  EcdhPregeneratedTestKeys ecdh_keys;
  auto pairwise_public_keys = std::make_unique<std::vector<EcdhPublicKey>>();
  std::vector<ClientToServerWrapperMessage> client_messages(4);
  ServerToClientWrapperMessage expected_server_message;
  for (int i = 0; i < 4; ++i) {
    PairOfPublicKeys* public_keys =
        expected_server_message.mutable_share_keys_request()
            ->add_pairs_of_public_keys();
    client_messages[i]
        .mutable_advertise_keys()
        ->mutable_pair_of_public_keys()
        ->set_enc_pk(ecdh_keys.GetPublicKeyString(i));
    client_messages[i]
        .mutable_advertise_keys()
        ->mutable_pair_of_public_keys()
        ->set_noise_pk(ecdh_keys.GetPublicKeyString(i + 4));
    public_keys->set_enc_pk(ecdh_keys.GetPublicKeyString(i));
    public_keys->set_noise_pk(ecdh_keys.GetPublicKeyString(i + 4));
  }

  expected_server_message.mutable_share_keys_request()->set_session_id(
      ComputeSessionId(expected_server_message.share_keys_request()).data);

  EXPECT_CALL(*sender, Send(_, _)).Times(0);
  for (int i = 0; i < 4; ++i) {
    EXPECT_CALL(*sender, Send(i, EqualsProto(expected_server_message)))
        .Times(1);
  }
  EXPECT_CALL(*metrics, RoundTimes(Eq(SecAggServerStateKind::R0_ADVERTISE_KEYS),
                                   Eq(true), Ge(0)));
  EXPECT_CALL(*metrics,
              RoundSurvivingClients(
                  Eq(SecAggServerStateKind::R0_ADVERTISE_KEYS), Eq(4)));
  EXPECT_CALL(
      *metrics,
      ClientResponseTimes(
          Eq(ClientToServerWrapperMessage::MessageContentCase::kAdvertiseKeys),
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
      ASSERT_THAT(state.HandleMessage(i, client_messages[i]), IsOk());
      EXPECT_THAT(state.ReadyForNextRound(), Eq(i >= 2));
    }
  }

  auto next_state = state.ProceedToNextRound();
  ASSERT_THAT(next_state, IsOk());
  EXPECT_THAT(next_state.value()->State(),
              Eq(SecAggServerStateKind::R1_SHARE_KEYS));
  EXPECT_THAT(
      tracing_recorder.FindAllEvents<StateCompletion>(),
      ElementsAre(IsEvent<StateCompletion>(
          Eq(SecAggServerTraceState_R0AdvertiseKeys), Eq(true), Ge(0), Eq(4))));
}

}  // namespace
}  // namespace secagg
}  // namespace fcp
