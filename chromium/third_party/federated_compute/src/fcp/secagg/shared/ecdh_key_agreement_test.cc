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

#include "fcp/secagg/shared/ecdh_key_agreement.h"

#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "fcp/secagg/shared/aes_key.h"
#include "fcp/secagg/shared/ecdh_keys.h"
#include "fcp/secagg/testing/ecdh_pregenerated_test_keys.h"

namespace fcp {
namespace secagg {
namespace {

using ::testing::Contains;
using ::testing::Eq;
using ::testing::Not;

TEST(EcdhKeyAgreementTest, CanRecoverFromPrivateKey) {
  EcdhPregeneratedTestKeys keys;
  auto ecdh1 = EcdhKeyAgreement::CreateFromKeypair(keys.GetPrivateKey(0),
                                                   keys.GetPublicKey(0))
                   .value();
  auto ecdh2 =
      EcdhKeyAgreement::CreateFromPrivateKey(ecdh1->PrivateKey()).value();
  EXPECT_THAT(ecdh2->PrivateKey(), Eq(ecdh1->PrivateKey()));
  EXPECT_THAT(ecdh2->PublicKey().size(), Eq(0));
}

TEST(EcdhKeyAgreementTest, CanRecoverKeypairFromPrivateAndPublicKeys) {
  EcdhPregeneratedTestKeys keys;
  auto ecdh1 = EcdhKeyAgreement::CreateFromKeypair(keys.GetPrivateKey(0),
                                                   keys.GetPublicKey(0))
                   .value();
  auto ecdh2 = EcdhKeyAgreement::CreateFromKeypair(ecdh1->PrivateKey(),
                                                   ecdh1->PublicKey())
                   .value();
  EXPECT_THAT(ecdh2->PrivateKey(), Eq(ecdh1->PrivateKey()));
  EXPECT_THAT(ecdh2->PublicKey(), Eq(ecdh1->PublicKey()));
}

TEST(EcdhKeyAgreementTest, PrivateKeyIsExpectedLength) {
  EcdhPregeneratedTestKeys keys;
  auto ecdh = EcdhKeyAgreement::CreateFromKeypair(keys.GetPrivateKey(0),
                                                  keys.GetPublicKey(0))
                  .value();
  EXPECT_THAT(ecdh->PrivateKey().size(), Eq(EcdhPrivateKey::kSize));
}

TEST(EcdhKeyAgreementTest, PublicKeyIsExpectedLength) {
  EcdhPregeneratedTestKeys keys;
  auto ecdh = EcdhKeyAgreement::CreateFromKeypair(keys.GetPrivateKey(0),
                                                  keys.GetPublicKey(0))
                  .value();
  EXPECT_THAT(ecdh->PublicKey().size(), Eq(EcdhPublicKey::kSize));
}

TEST(EcdhKeyAgreementTest, RandomKeypairIsntTheSameEveryTime) {
  std::vector<Key> private_keys;
  std::vector<Key> public_keys;
  private_keys.reserve(16);
  public_keys.reserve(16);
  for (int i = 0; i < 16; ++i) {
    auto ecdh = EcdhKeyAgreement::CreateFromRandomKeys().value();
    EXPECT_THAT(private_keys, Not(Contains(ecdh->PrivateKey())));
    EXPECT_THAT(public_keys, Not(Contains(ecdh->PublicKey())));
    private_keys.push_back(ecdh->PrivateKey());
    public_keys.push_back(ecdh->PublicKey());
  }
}

TEST(EcdhKeyAgreementTest, SharedSecretsHaveCorrectLength) {
  EcdhPregeneratedTestKeys keys;
  auto ecdh = EcdhKeyAgreement::CreateFromKeypair(keys.GetPrivateKey(0),
                                                  keys.GetPublicKey(0))
                  .value();
  auto secret = ecdh->ComputeSharedSecret(keys.GetPublicKey(1));
  ASSERT_TRUE(secret.ok());
  EXPECT_THAT(secret.value().size(), Eq(AesKey::kSize));
}

TEST(EcdhKeyAgreementTest, SharedSecretsAreDeterministic) {
  EcdhPregeneratedTestKeys keys;
  auto ecdh = EcdhKeyAgreement::CreateFromKeypair(keys.GetPrivateKey(0),
                                                  keys.GetPublicKey(0))
                  .value();
  auto secret1 = ecdh->ComputeSharedSecret(keys.GetPublicKey(1));
  auto secret2 = ecdh->ComputeSharedSecret(keys.GetPublicKey(1));
  ASSERT_TRUE(secret1.ok());
  ASSERT_TRUE(secret2.ok());
  EXPECT_THAT(secret1.value(), Eq(secret2.value()));
}

TEST(EcdhKeyAgreementTest, SharedSecretsAreConsistent) {
  EcdhPregeneratedTestKeys keys;
  auto ecdh1 = EcdhKeyAgreement::CreateFromKeypair(keys.GetPrivateKey(0),
                                                   keys.GetPublicKey(0))
                   .value();
  auto ecdh2 = EcdhKeyAgreement::CreateFromKeypair(keys.GetPrivateKey(1),
                                                   keys.GetPublicKey(1))
                   .value();
  auto secret1 = ecdh1->ComputeSharedSecret(ecdh2->PublicKey());
  auto secret2 = ecdh2->ComputeSharedSecret(ecdh1->PublicKey());
  ASSERT_TRUE(secret1.ok());
  ASSERT_TRUE(secret2.ok());
  EXPECT_THAT(secret1.value(), Eq(secret2.value()));
}

TEST(EcdhKeyAgreementTest, SharedSecretsAreConsistentWithoutPublicKey) {
  EcdhPregeneratedTestKeys keys;
  auto ecdh1 =
      EcdhKeyAgreement::CreateFromPrivateKey(keys.GetPrivateKey(0)).value();
  auto ecdh2 =
      EcdhKeyAgreement::CreateFromPrivateKey(keys.GetPrivateKey(1)).value();
  auto secret1 = ecdh1->ComputeSharedSecret(keys.GetPublicKey(1));
  auto secret2 = ecdh2->ComputeSharedSecret(keys.GetPublicKey(0));
  ASSERT_TRUE(secret1.ok());
  ASSERT_TRUE(secret2.ok());
  EXPECT_THAT(secret1.value(), Eq(secret2.value()));
}

TEST(EcdhKeyAgreementTest, CreateFromKeypairErrorsOnInconsistentKeys) {
  EcdhPregeneratedTestKeys keys;
  auto ecdh = EcdhKeyAgreement::CreateFromKeypair(keys.GetPrivateKey(0),
                                                  keys.GetPublicKey(1));
  EXPECT_THAT(ecdh.ok(), Eq(false));
}

TEST(EcdhKeyAgreementTest, ComputeSharedSecretErrorsOnGarbagePublicKey) {
  EcdhPregeneratedTestKeys keys;
  auto ecdh = EcdhKeyAgreement::CreateFromPrivateKey(keys.GetPrivateKey(0));
  ASSERT_TRUE(ecdh.ok());

  // first byte valid at least
  const char bad_key[] =
      "\x2"
      "23456789012345678901234567890123";

  auto secret = ecdh.value()->ComputeSharedSecret(
      EcdhPublicKey(reinterpret_cast<const uint8_t*>(bad_key)));
  EXPECT_THAT(secret.ok(), Eq(false));
}

TEST(EcdhKeyAgreementTest, SharedSecretsWorkWithUncompressedPublicKeys) {
  EcdhPregeneratedTestKeys keys;
  auto ecdh = EcdhKeyAgreement::CreateFromKeypair(keys.GetPrivateKey(0),
                                                  keys.GetPublicKey(0))
                  .value();
  auto secret = ecdh->ComputeSharedSecret(keys.GetUncompressedPublicKey(0));
  ASSERT_TRUE(secret.ok());
  EXPECT_THAT(secret.value().size(), Eq(AesKey::kSize));
}
}  // namespace
}  // namespace secagg
}  // namespace fcp
