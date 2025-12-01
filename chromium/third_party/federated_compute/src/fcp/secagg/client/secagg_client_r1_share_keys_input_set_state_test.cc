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

#include "fcp/secagg/client/secagg_client_r1_share_keys_input_set_state.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "fcp/secagg/client/other_client_state.h"
#include "fcp/secagg/client/secagg_client_aborted_state.h"
#include "fcp/secagg/client/secagg_client_r2_masked_input_coll_input_set_state.h"
#include "fcp/secagg/client/send_to_server_interface.h"
#include "fcp/secagg/client/state_transition_listener_interface.h"
#include "fcp/secagg/shared/aes_ctr_prng_factory.h"
#include "fcp/secagg/shared/aes_gcm_encryption.h"
#include "fcp/secagg/shared/aes_key.h"
#include "fcp/secagg/shared/compute_session_id.h"
#include "fcp/secagg/shared/ecdh_key_agreement.h"
#include "fcp/secagg/shared/ecdh_keys.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/secagg_messages.pb.h"
#include "fcp/secagg/shared/secagg_vector.h"
#include "fcp/secagg/testing/ecdh_pregenerated_test_keys.h"
#include "fcp/secagg/testing/fake_prng.h"
#include "fcp/secagg/testing/mock_send_to_server_interface.h"
#include "fcp/secagg/testing/mock_state_transition_listener.h"
#include "fcp/testing/testing.h"

namespace fcp {
namespace secagg {

using ::testing::Eq;
using ::testing::Pointee;

TEST(SecAggClientR1ShareKeysInputSetStateTest, IsAbortedReturnsFalse) {
  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({2, 4, 6, 8}, 32));
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR1ShareKeysInputSetState r1_state(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      std::move(EcdhKeyAgreement::CreateFromRandomKeys().value()),
      std::move(input_map), std::move(input_vector_specs),
      std::make_unique<FakePrng>(),
      std::move(EcdhKeyAgreement::CreateFromRandomKeys().value()),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  EXPECT_THAT(r1_state.IsAborted(), Eq(false));
}

TEST(SecAggClientR1ShareKeysInputSetStateTest,
     IsCompletedSuccessfullyReturnsFalse) {
  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({2, 4, 6, 8}, 32));
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR1ShareKeysInputSetState r1_state(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      std::move(EcdhKeyAgreement::CreateFromRandomKeys().value()),
      std::move(input_map), std::move(input_vector_specs),
      std::make_unique<FakePrng>(),
      std::move(EcdhKeyAgreement::CreateFromRandomKeys().value()),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  EXPECT_THAT(r1_state.IsCompletedSuccessfully(), Eq(false));
}

TEST(SecAggClientR1ShareKeysInputSetStateTest, StartRaisesErrorStatus) {
  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({2, 4, 6, 8}, 32));
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR1ShareKeysInputSetState r1_state(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      std::move(EcdhKeyAgreement::CreateFromRandomKeys().value()),
      std::move(input_map), std::move(input_vector_specs),
      std::make_unique<FakePrng>(),
      std::move(EcdhKeyAgreement::CreateFromRandomKeys().value()),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  EXPECT_THAT(r1_state.Start().ok(), Eq(false));
}

TEST(SecAggClientR1ShareKeysInputSetStateTest, SetInputRaisesErrorStatus) {
  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({2, 4, 6, 8}, 32));
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR1ShareKeysInputSetState r1_state(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      std::move(EcdhKeyAgreement::CreateFromRandomKeys().value()),
      std::move(input_map), std::move(input_vector_specs),
      std::make_unique<FakePrng>(),
      std::move(EcdhKeyAgreement::CreateFromRandomKeys().value()),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  EXPECT_THAT(r1_state.SetInput(std::make_unique<SecAggVectorMap>()).ok(),
              Eq(false));
}

TEST(SecAggClientR1ShareKeysInputSetStateTest, ErrorMessageRaisesErrorStatus) {
  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({2, 4, 6, 8}, 32));
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR1ShareKeysInputSetState r1_state(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      std::move(EcdhKeyAgreement::CreateFromRandomKeys().value()),
      std::move(input_map), std::move(input_vector_specs),
      std::make_unique<FakePrng>(),
      std::move(EcdhKeyAgreement::CreateFromRandomKeys().value()),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  EXPECT_THAT(r1_state.ErrorMessage().ok(), Eq(false));
}

TEST(SecAggClientR1ShareKeysInputSetStateTest,
     AbortReturnsValidAbortStateAndNotifiesServer) {
  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({2, 4, 6, 8}, 32));
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR1ShareKeysInputSetState r1_state(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      std::move(EcdhKeyAgreement::CreateFromRandomKeys().value()),
      std::move(input_map), std::move(input_vector_specs),
      std::make_unique<FakePrng>(),
      std::move(EcdhKeyAgreement::CreateFromRandomKeys().value()),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  std::string error_string =
      "Abort upon external request for reason <Abort reason>.";
  ClientToServerWrapperMessage expected_message;
  expected_message.mutable_abort()->set_diagnostic_info(error_string);
  EXPECT_CALL(*sender, Send(Pointee(EqualsProto(expected_message))));

  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r1_state.Abort("Abort reason");
  ASSERT_THAT(new_state.ok(), Eq(true));
  EXPECT_THAT(new_state.value()->StateName(), Eq("ABORTED"));
  EXPECT_THAT(new_state.value()->ErrorMessage().value(), Eq(error_string));
}

TEST(SecAggClientR1ShareKeysInputSetStateTest,
     AbortFailureMessageCausesAbortWithoutNotifyingServer) {
  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({2, 4, 6, 8}, 32));
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR1ShareKeysInputSetState r1_state(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      std::move(EcdhKeyAgreement::CreateFromRandomKeys().value()),
      std::move(input_map), std::move(input_vector_specs),
      std::make_unique<FakePrng>(),
      std::move(EcdhKeyAgreement::CreateFromRandomKeys().value()),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  EXPECT_CALL(*sender, Send(::testing::_)).Times(0);
  ServerToClientWrapperMessage abort_message;
  abort_message.mutable_abort()->set_early_success(false);

  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r1_state.HandleMessage(abort_message);
  ASSERT_TRUE(new_state.ok());
  EXPECT_THAT(new_state.value()->StateName(), Eq("ABORTED"));
  EXPECT_THAT(new_state.value()->ErrorMessage().value(),
              Eq("Aborting because of abort message from the server."));
}

TEST(SecAggClientR1ShareKeysInputSetStateTest,
     EarlySuccessMessageCausesTransitionToCompletedState) {
  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({2, 4, 6, 8}, 32));
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();

  SecAggClientR1ShareKeysInputSetState r1_state(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      std::move(EcdhKeyAgreement::CreateFromRandomKeys().value()),
      std::move(input_map), std::move(input_vector_specs),
      std::make_unique<FakePrng>(),
      std::move(EcdhKeyAgreement::CreateFromRandomKeys().value()),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  EXPECT_CALL(*sender, Send(::testing::_)).Times(0);
  ServerToClientWrapperMessage abort_message;
  abort_message.mutable_abort()->set_early_success(true);

  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r1_state.HandleMessage(abort_message);
  ASSERT_TRUE(new_state.ok());
  EXPECT_THAT(new_state.value()->StateName(), Eq("COMPLETED"));
}

// A gMock matcher to see if the message sent by the client contains shares that
// reconstruct to the right private keys.

// pairwise_key is the expected pairwise PRNG key.
// self_key is the expected self PRNG key.
// enc_keys must be a std::vector<AesKey>, the vector of encryption keys.
// Assumes 3-of-4 secret sharing.
MATCHER_P3(ReconstructsCorrectly, pairwise_key, self_key, enc_keys, "") {
  AesGcmEncryption decryptor;
  std::vector<ShamirShare> pairwise_shares;
  std::vector<ShamirShare> self_shares;
  for (int i = 0; i < enc_keys.size(); ++i) {
    // Blank shares must be blank in both places
    if (arg->share_keys_response().encrypted_key_shares(i).empty()) {
      pairwise_shares.push_back({""});
      self_shares.push_back({""});
      continue;
    }
    auto decrypted = decryptor.Decrypt(
        enc_keys[i], arg->share_keys_response().encrypted_key_shares(i));
    if (!decrypted.ok()) {
      return false;
    }
    PairOfKeyShares key_shares;
    if (!key_shares.ParseFromString(decrypted.value())) {
      return false;
    }
    pairwise_shares.push_back({key_shares.noise_sk_share()});
    self_shares.push_back({key_shares.prf_sk_share()});
  }
  // Reconstruct keys to see if they match
  ShamirSecretSharing reconstructor;
  std::string reconstructed_pairwise_key_string =
      reconstructor.Reconstruct(3, pairwise_shares, EcdhPrivateKey::kSize)
          .value();
  std::string reconstructed_self_key_string =
      reconstructor.Reconstruct(3, self_shares, AesKey::kSize).value();
  EcdhPrivateKey reconstructed_pairwise_key(reinterpret_cast<const uint8_t*>(
      reconstructed_pairwise_key_string.c_str()));
  AesKey reconstructed_self_key(
      reinterpret_cast<const uint8_t*>(reconstructed_self_key_string.c_str()));
  return pairwise_key == reconstructed_pairwise_key &&
         self_key == reconstructed_self_key;
}

TEST(SecAggClientR1ShareKeysInputSetStateTest,
     ShareKeysRequestIsHandledCorrectlyWhenNoClientsDie) {
  // In this test, the client under test is id 1, and there are 4 clients, all
  // alive.
  EcdhPregeneratedTestKeys ecdh_keys;
  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({2, 4, 6, 8}, 32));
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR1ShareKeysInputSetState r1_state(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      std::move(EcdhKeyAgreement::CreateFromKeypair(ecdh_keys.GetPrivateKey(2),
                                                    ecdh_keys.GetPublicKey(2))
                    .value()),
      std::move(input_map), std::move(input_vector_specs),
      std::make_unique<FakePrng>(),
      std::move(EcdhKeyAgreement::CreateFromKeypair(ecdh_keys.GetPrivateKey(3),
                                                    ecdh_keys.GetPublicKey(3))
                    .value()),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  // Make a copy of the encryption keys for testing
  std::vector<AesKey> enc_keys;
  auto enc_key_agreement =
      EcdhKeyAgreement::CreateFromPrivateKey(ecdh_keys.GetPrivateKey(2))
          .value();

  ServerToClientWrapperMessage message;
  for (int i = 0; i < 4; ++i) {
    PairOfPublicKeys* keypair =
        message.mutable_share_keys_request()->add_pairs_of_public_keys();
    keypair->set_enc_pk(ecdh_keys.GetPublicKeyString(2 * i));
    keypair->set_noise_pk(ecdh_keys.GetPublicKeyString(2 * i + 1));

    if (i == 1) {
      enc_keys.push_back(AesKey());
    } else {
      enc_keys.push_back(
          enc_key_agreement->ComputeSharedSecret(ecdh_keys.GetPublicKey(2 * i))
              .value());
    }
  }

  // Make a copy of the self PRNG key.
  FakePrng prng;
  uint8_t self_prng_key_buffer[AesKey::kSize];
  for (int i = 0; i < AesKey::kSize; ++i) {
    self_prng_key_buffer[i] = prng.Rand8();
  }
  auto self_prng_key = AesKey(self_prng_key_buffer);

  EXPECT_CALL(*sender, Send(ReconstructsCorrectly(ecdh_keys.GetPrivateKey(3),
                                                  self_prng_key, enc_keys)))
      .Times(1);

  message.mutable_share_keys_request()->set_session_id(
      ComputeSessionId(message.share_keys_request()).data);
  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r1_state.HandleMessage(message);
  ASSERT_TRUE(new_state.ok());
  EXPECT_THAT(new_state.value()->StateName(),
              Eq("R2_MASKED_INPUT_COLL_INPUT_SET"));
}

// A gMock matcher to see if the message sent by the client contains shares that
// reconstruct to the right private keys.
//
// This version also takes into account the client's own shares, to ensure that
// it works even when exactly threshold clients survive.
//
// pairwise_key is the expected pairwise PRNG key, an AesKey.
// self_key is the expected self PRNG key, an AesKey.
// own_pairwise_key_share is client 1's ShamirShare of its own pairwise PRNG
// key.
// own_self_key_share is client 1's ShamirShare of its own pairwise PRNG
// key.
// enc_keys must be a std::vector<AesKey>, the vector of encryption keys.
// Assumes 3-of-4 secret sharing.
MATCHER_P5(ReconstructsCorrectlyWithOwnKeys, pairwise_key, self_key,
           own_pairwise_key_share, own_self_key_share, enc_keys, "") {
  AesGcmEncryption decryptor;
  std::vector<ShamirShare> pairwise_shares;
  std::vector<ShamirShare> self_shares;
  for (int i = 0; i < enc_keys.size(); ++i) {
    // Blank shares must be blank in both places
    if (arg->share_keys_response().encrypted_key_shares(i).empty()) {
      if (i == 1) {
        pairwise_shares.push_back(own_pairwise_key_share);
        self_shares.push_back(own_self_key_share);
      } else {
        pairwise_shares.push_back({""});
        self_shares.push_back({""});
      }
      continue;
    }
    auto decrypted = decryptor.Decrypt(
        enc_keys[i], arg->share_keys_response().encrypted_key_shares(i));
    if (!decrypted.ok()) {
      return false;
    }
    PairOfKeyShares key_shares;
    if (!key_shares.ParseFromString(decrypted.value())) {
      return false;
    }
    pairwise_shares.push_back({key_shares.noise_sk_share()});
    self_shares.push_back({key_shares.prf_sk_share()});
  }
  // Reconstruct keys to see if they match
  ShamirSecretSharing reconstructor;
  std::string reconstructed_pairwise_key_string =
      reconstructor.Reconstruct(3, pairwise_shares, EcdhPrivateKey::kSize)
          .value();
  std::string reconstructed_self_key_string =
      reconstructor.Reconstruct(3, self_shares, AesKey::kSize).value();
  EcdhPrivateKey reconstructed_pairwise_key(reinterpret_cast<const uint8_t*>(
      reconstructed_pairwise_key_string.c_str()));
  AesKey reconstructed_self_key(
      reinterpret_cast<const uint8_t*>(reconstructed_self_key_string.c_str()));
  EXPECT_THAT(reconstructed_pairwise_key_string,
              Eq(std::string(reinterpret_cast<const char*>(pairwise_key.data()),
                             EcdhPrivateKey::kSize)));
  return pairwise_key == reconstructed_pairwise_key;
}

TEST(SecAggClientR1ShareKeysInputSetStateTest,
     ShareKeysRequestIsHandledCorrectlyWithDeadClient) {
  // In this test, the client under test is id 1, and there are 4 clients.
  // Client 3 has died in this round.
  EcdhPregeneratedTestKeys ecdh_keys;
  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({2, 4, 6, 8}, 32));
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR1ShareKeysInputSetState r1_state(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      std::move(EcdhKeyAgreement::CreateFromKeypair(ecdh_keys.GetPrivateKey(2),
                                                    ecdh_keys.GetPublicKey(2))
                    .value()),
      std::move(input_map), std::move(input_vector_specs),
      std::make_unique<FakePrng>(),
      std::move(EcdhKeyAgreement::CreateFromKeypair(ecdh_keys.GetPrivateKey(3),
                                                    ecdh_keys.GetPublicKey(3))
                    .value()),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  // Make a copy of the encryption keys for testing
  std::vector<AesKey> enc_keys;
  auto enc_key_agreement =
      EcdhKeyAgreement::CreateFromPrivateKey(ecdh_keys.GetPrivateKey(2))
          .value();

  ServerToClientWrapperMessage message;
  for (int i = 0; i < 3; ++i) {  // exclude client 3
    PairOfPublicKeys* keypair =
        message.mutable_share_keys_request()->add_pairs_of_public_keys();
    keypair->set_enc_pk(ecdh_keys.GetPublicKeyString(2 * i));
    keypair->set_noise_pk(ecdh_keys.GetPublicKeyString(2 * i + 1));

    if (i == 1) {
      enc_keys.push_back(AesKey());
    } else {
      enc_keys.push_back(
          enc_key_agreement->ComputeSharedSecret(ecdh_keys.GetPublicKey(2 * i))
              .value());
    }
  }
  message.mutable_share_keys_request()->add_pairs_of_public_keys();
  enc_keys.push_back(AesKey());

  // Make a copy of the self PRNG key.
  FakePrng prng;
  uint8_t self_prng_key_buffer[AesKey::kSize];
  for (int i = 0; i < AesKey::kSize; ++i) {
    self_prng_key_buffer[i] = prng.Rand8();
  }
  auto self_prng_key = AesKey(self_prng_key_buffer);

  r1_state.SetUpShares(3, 4, ecdh_keys.GetPrivateKey(3), self_prng_key,
                       &r1_state.self_prng_key_shares_,
                       &r1_state.pairwise_prng_key_shares_);
  ShamirShare own_pairwise_key_share = r1_state.pairwise_prng_key_shares_.at(1);
  ShamirShare own_self_key_share = r1_state.self_prng_key_shares_.at(1);

  EXPECT_CALL(*sender,
              Send(ReconstructsCorrectlyWithOwnKeys(
                  ecdh_keys.GetPrivateKey(3), self_prng_key,
                  own_pairwise_key_share, own_self_key_share, enc_keys)))
      .Times(1);

  message.mutable_share_keys_request()->set_session_id(
      ComputeSessionId(message.share_keys_request()).data);
  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r1_state.HandleMessage(message);
  ASSERT_TRUE(new_state.ok());
  EXPECT_THAT(new_state.value()->StateName(),
              Eq("R2_MASKED_INPUT_COLL_INPUT_SET"));
}

TEST(SecAggClientR1ShareKeysInputSetStateTest,
     ShareKeysRequestCausesAbortIfTooManyDeadClients) {
  // In this test, the client under test is id 1, and there are 4 clients.
  // Clients 2 and 3 died, and we need 3 clients to continue, so we should
  // abort.
  EcdhPregeneratedTestKeys ecdh_keys;
  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({2, 4, 6, 8}, 32));
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();

  SecAggClientR1ShareKeysInputSetState r1_state(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      std::move(EcdhKeyAgreement::CreateFromKeypair(ecdh_keys.GetPrivateKey(2),
                                                    ecdh_keys.GetPublicKey(2))
                    .value()),
      std::move(input_map), std::move(input_vector_specs),
      std::make_unique<FakePrng>(),
      std::move(EcdhKeyAgreement::CreateFromKeypair(ecdh_keys.GetPrivateKey(3),
                                                    ecdh_keys.GetPublicKey(3))
                    .value()),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  ServerToClientWrapperMessage message;
  for (int i = 0; i < 2; ++i) {  // exclude clients 2 and 3.
    PairOfPublicKeys* keypair =
        message.mutable_share_keys_request()->add_pairs_of_public_keys();
    keypair->set_enc_pk(ecdh_keys.GetPublicKeyString(2 * i));
    keypair->set_noise_pk(ecdh_keys.GetPublicKeyString(2 * i + 1));
  }
  message.mutable_share_keys_request()->add_pairs_of_public_keys();
  message.mutable_share_keys_request()->add_pairs_of_public_keys();

  std::string error_string =
      "There are not enough clients to complete this protocol session. "
      "Aborting.";
  ClientToServerWrapperMessage expected_message;
  expected_message.mutable_abort()->set_diagnostic_info(error_string);
  EXPECT_CALL(*sender, Send(Pointee(EqualsProto(expected_message))));

  message.mutable_share_keys_request()->set_session_id(
      ComputeSessionId(message.share_keys_request()).data);
  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r1_state.HandleMessage(message);
  ASSERT_THAT(new_state.ok(), Eq(true));
  EXPECT_THAT(new_state.value()->StateName(), Eq("ABORTED"));
  EXPECT_THAT(new_state.value()->ErrorMessage().value(), Eq(error_string));
}

TEST(SecAggClientR1ShareKeysInputSetStateTest,
     ShareKeysRequestCausesAbortIfServerSendsWrongSizeKey) {
  // In this test, the client under test is id 1, and there are 4 clients.
  // One of client 3's keys is a string of the wrong length, so this should
  // cause an abort.
  EcdhPregeneratedTestKeys ecdh_keys;
  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({2, 4, 6, 8}, 32));
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR1ShareKeysInputSetState r1_state(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      std::move(EcdhKeyAgreement::CreateFromKeypair(ecdh_keys.GetPrivateKey(2),
                                                    ecdh_keys.GetPublicKey(2))
                    .value()),
      std::move(input_map), std::move(input_vector_specs),
      std::make_unique<FakePrng>(),
      std::move(EcdhKeyAgreement::CreateFromKeypair(ecdh_keys.GetPrivateKey(3),
                                                    ecdh_keys.GetPublicKey(3))
                    .value()),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  ServerToClientWrapperMessage message;
  for (int i = 0; i < 3; ++i) {  // handle client 3 separately
    PairOfPublicKeys* keypair =
        message.mutable_share_keys_request()->add_pairs_of_public_keys();
    keypair->set_enc_pk(ecdh_keys.GetPublicKeyString(2 * i));
    keypair->set_noise_pk(ecdh_keys.GetPublicKeyString(2 * i + 1));
  }
  PairOfPublicKeys* bad_keypair =
      message.mutable_share_keys_request()->add_pairs_of_public_keys();
  bad_keypair->set_enc_pk(ecdh_keys.GetPublicKeyString(6));
  bad_keypair->set_noise_pk("there's no way this is a valid key");

  std::string error_string = "Invalid public key in request from server.";
  ClientToServerWrapperMessage expected_message;
  expected_message.mutable_abort()->set_diagnostic_info(error_string);
  EXPECT_CALL(*sender, Send(Pointee(EqualsProto(expected_message))));

  message.mutable_share_keys_request()->set_session_id(
      ComputeSessionId(message.share_keys_request()).data);
  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r1_state.HandleMessage(message);
  ASSERT_THAT(new_state.ok(), Eq(true));
  EXPECT_THAT(new_state.value()->StateName(), Eq("ABORTED"));
  EXPECT_THAT(new_state.value()->ErrorMessage().value(), Eq(error_string));
}

TEST(SecAggClientR1ShareKeysInputSetStateTest,
     ShareKeysRequestCausesAbortIfServerSendsInvalidKey) {
  // In this test, the client under test is id 1, and there are 4 clients.
  // One of client 3's keys is a not a valid ECDH key, so this should cause an
  // abort.
  EcdhPregeneratedTestKeys ecdh_keys;
  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({2, 4, 6, 8}, 32));
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR1ShareKeysInputSetState r1_state(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      std::move(EcdhKeyAgreement::CreateFromKeypair(ecdh_keys.GetPrivateKey(2),
                                                    ecdh_keys.GetPublicKey(2))
                    .value()),
      std::move(input_map), std::move(input_vector_specs),
      std::make_unique<FakePrng>(),
      std::move(EcdhKeyAgreement::CreateFromKeypair(ecdh_keys.GetPrivateKey(3),
                                                    ecdh_keys.GetPublicKey(3))
                    .value()),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  ServerToClientWrapperMessage message;
  for (int i = 0; i < 3; ++i) {  // handle client 3 separately
    PairOfPublicKeys* keypair =
        message.mutable_share_keys_request()->add_pairs_of_public_keys();
    keypair->set_enc_pk(ecdh_keys.GetPublicKeyString(2 * i));
    keypair->set_noise_pk(ecdh_keys.GetPublicKeyString(2 * i + 1));
  }
  PairOfPublicKeys* bad_keypair =
      message.mutable_share_keys_request()->add_pairs_of_public_keys();
  bad_keypair->set_enc_pk(ecdh_keys.GetPublicKeyString(6));
  bad_keypair->set_noise_pk("Right size, but not an ECDH point");

  std::string error_string = "Invalid public key in request from server.";
  ClientToServerWrapperMessage expected_message;
  expected_message.mutable_abort()->set_diagnostic_info(error_string);
  EXPECT_CALL(*sender, Send(Pointee(EqualsProto(expected_message))));

  message.mutable_share_keys_request()->set_session_id(
      ComputeSessionId(message.share_keys_request()).data);
  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r1_state.HandleMessage(message);
  ASSERT_THAT(new_state.ok(), Eq(true));
  EXPECT_THAT(new_state.value()->StateName(), Eq("ABORTED"));
  EXPECT_THAT(new_state.value()->ErrorMessage().value(), Eq(error_string));
}

TEST(SecAggClientR1ShareKeysInputSetStateTest,
     ShareKeysRequestCausesAbortIfServerSendsTooManyKeys) {
  // In this test, the client under test is id 1, and it expects there to be no
  // more than 3 clients. However, the server sends 4 keypairs. This should
  // cause an abort.
  EcdhPregeneratedTestKeys ecdh_keys;
  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({2, 4, 6, 8}, 32));
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR1ShareKeysInputSetState r1_state(
      3,  // max_neighbors_expected
      2,  // minimum_surviving_neighbors_for_reconstruction
      std::move(EcdhKeyAgreement::CreateFromKeypair(ecdh_keys.GetPrivateKey(2),
                                                    ecdh_keys.GetPublicKey(2))
                    .value()),
      std::move(input_map), std::move(input_vector_specs),
      std::make_unique<FakePrng>(),
      std::move(EcdhKeyAgreement::CreateFromKeypair(ecdh_keys.GetPrivateKey(3),
                                                    ecdh_keys.GetPublicKey(3))
                    .value()),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  ServerToClientWrapperMessage message;
  for (int i = 0; i < 4; ++i) {
    PairOfPublicKeys* keypair =
        message.mutable_share_keys_request()->add_pairs_of_public_keys();
    keypair->set_enc_pk(ecdh_keys.GetPublicKeyString(2 * i));
    keypair->set_noise_pk(ecdh_keys.GetPublicKeyString(2 * i + 1));
  }

  std::string error_string =
      "The ShareKeysRequest received contains too many participants.";
  ClientToServerWrapperMessage expected_message;
  expected_message.mutable_abort()->set_diagnostic_info(error_string);
  EXPECT_CALL(*sender, Send(Pointee(EqualsProto(expected_message))));

  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r1_state.HandleMessage(message);
  ASSERT_THAT(new_state.ok(), Eq(true));
  EXPECT_THAT(new_state.value()->StateName(), Eq("ABORTED"));
  EXPECT_THAT(new_state.value()->ErrorMessage().value(), Eq(error_string));
}

TEST(SecAggClientR1ShareKeysInputSetStateTest,
     ShareKeysRequestCausesAbortIfServerSendsTooFewKeys) {
  // In this test, the client under test is id 1, and the threshold is 3
  // clients. However, the server sends only 2 keys. This should cause an abort.
  EcdhPregeneratedTestKeys ecdh_keys;
  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({2, 4, 6, 8}, 32));
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR1ShareKeysInputSetState r1_state(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      std::move(EcdhKeyAgreement::CreateFromKeypair(ecdh_keys.GetPrivateKey(2),
                                                    ecdh_keys.GetPublicKey(2))
                    .value()),
      std::move(input_map), std::move(input_vector_specs),
      std::make_unique<FakePrng>(),
      std::move(EcdhKeyAgreement::CreateFromKeypair(ecdh_keys.GetPrivateKey(3),
                                                    ecdh_keys.GetPublicKey(3))
                    .value()),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  ServerToClientWrapperMessage message;
  for (int i = 0; i < 2; ++i) {  // exclude clients 2 and 3
    PairOfPublicKeys* keypair =
        message.mutable_share_keys_request()->add_pairs_of_public_keys();
    keypair->set_enc_pk(ecdh_keys.GetPublicKeyString(2 * i));
    keypair->set_noise_pk(ecdh_keys.GetPublicKeyString(2 * i + 1));
  }

  std::string error_string =
      "The ShareKeysRequest received does not contain enough participants.";
  ClientToServerWrapperMessage expected_message;
  expected_message.mutable_abort()->set_diagnostic_info(error_string);
  EXPECT_CALL(*sender, Send(Pointee(EqualsProto(expected_message))));

  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r1_state.HandleMessage(message);
  ASSERT_THAT(new_state.ok(), Eq(true));
  EXPECT_THAT(new_state.value()->StateName(), Eq("ABORTED"));
  EXPECT_THAT(new_state.value()->ErrorMessage().value(), Eq(error_string));
}

TEST(SecAggClientR1ShareKeysInputSetStateTest,
     ShareKeysRequestCausesAbortIfServerOmitsClientsKey) {
  // In this test, the client under test is not represented at all in the
  // server's message. This should cause an abort.
  EcdhPregeneratedTestKeys ecdh_keys;
  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({2, 4, 6, 8}, 32));
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR1ShareKeysInputSetState r1_state(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      std::move(EcdhKeyAgreement::CreateFromKeypair(ecdh_keys.GetPrivateKey(2),
                                                    ecdh_keys.GetPublicKey(2))
                    .value()),
      std::move(input_map), std::move(input_vector_specs),
      std::make_unique<FakePrng>(),
      std::move(EcdhKeyAgreement::CreateFromKeypair(ecdh_keys.GetPrivateKey(3),
                                                    ecdh_keys.GetPublicKey(3))
                    .value()),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  ServerToClientWrapperMessage message;
  for (int i = 0; i < 4; ++i) {
    if (i == 1) {
      continue;  // skipping this client
    }
    PairOfPublicKeys* keypair =
        message.mutable_share_keys_request()->add_pairs_of_public_keys();
    keypair->set_enc_pk(ecdh_keys.GetPublicKeyString(2 * i));
    keypair->set_noise_pk(ecdh_keys.GetPublicKeyString(2 * i + 1));
  }

  std::string error_string =
      "The ShareKeysRequest sent by the server doesn't contain this client's "
      "public keys.";
  ClientToServerWrapperMessage expected_message;
  expected_message.mutable_abort()->set_diagnostic_info(error_string);
  EXPECT_CALL(*sender, Send(Pointee(EqualsProto(expected_message))));

  message.mutable_share_keys_request()->set_session_id(
      ComputeSessionId(message.share_keys_request()).data);
  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r1_state.HandleMessage(message);
  ASSERT_THAT(new_state.ok(), Eq(true));
  EXPECT_THAT(new_state.value()->StateName(), Eq("ABORTED"));
  EXPECT_THAT(new_state.value()->ErrorMessage().value(), Eq(error_string));
}

TEST(SecAggClientR1ShareKeysInputSetStateTest,
     ShareKeysRequestCausesAbortIfServerDuplicatesClientsKey) {
  // In this test, the client under test is included twice in the server's
  // message. This should cause an abort.
  EcdhPregeneratedTestKeys ecdh_keys;
  auto input_map = std::make_unique<SecAggVectorMap>();
  input_map->emplace("test", SecAggVector({2, 4, 6, 8}, 32));
  auto input_vector_specs =
      std::make_unique<std::vector<InputVectorSpecification> >();
  input_vector_specs->push_back(InputVectorSpecification("test", 4, 32));
  MockSendToServerInterface* sender = new MockSendToServerInterface();
  MockStateTransitionListener* transition_listener =
      new MockStateTransitionListener();
  SecAggClientR1ShareKeysInputSetState r1_state(
      4,  // max_neighbors_expected
      3,  // minimum_surviving_neighbors_for_reconstruction
      std::move(EcdhKeyAgreement::CreateFromKeypair(ecdh_keys.GetPrivateKey(2),
                                                    ecdh_keys.GetPublicKey(2))
                    .value()),
      std::move(input_map), std::move(input_vector_specs),
      std::make_unique<FakePrng>(),
      std::move(EcdhKeyAgreement::CreateFromKeypair(ecdh_keys.GetPrivateKey(3),
                                                    ecdh_keys.GetPublicKey(3))
                    .value()),
      std::unique_ptr<SendToServerInterface>(sender),
      std::unique_ptr<StateTransitionListenerInterface>(transition_listener),
      std::make_unique<AesCtrPrngFactory>());

  ServerToClientWrapperMessage message;
  for (int i = 0; i < 3; ++i) {  // handle client 3 separately
    PairOfPublicKeys* keypair =
        message.mutable_share_keys_request()->add_pairs_of_public_keys();
    keypair->set_enc_pk(ecdh_keys.GetPublicKeyString(2 * i));
    keypair->set_noise_pk(ecdh_keys.GetPublicKeyString(2 * i + 1));
  }
  PairOfPublicKeys* bad_keypair =
      message.mutable_share_keys_request()->add_pairs_of_public_keys();
  bad_keypair->set_enc_pk(ecdh_keys.GetPublicKeyString(2));
  bad_keypair->set_noise_pk(ecdh_keys.GetPublicKeyString(3));

  std::string error_string =
      "Found this client's keys in the ShareKeysRequest twice somehow.";
  ClientToServerWrapperMessage expected_message;
  expected_message.mutable_abort()->set_diagnostic_info(error_string);
  EXPECT_CALL(*sender, Send(Pointee(EqualsProto(expected_message))));

  message.mutable_share_keys_request()->set_session_id(
      ComputeSessionId(message.share_keys_request()).data);
  StatusOr<std::unique_ptr<SecAggClientState> > new_state =
      r1_state.HandleMessage(message);
  ASSERT_THAT(new_state.ok(), Eq(true));
  EXPECT_THAT(new_state.value()->StateName(), Eq("ABORTED"));
  EXPECT_THAT(new_state.value()->ErrorMessage().value(), Eq(error_string));
}

}  // namespace secagg
}  // namespace fcp
