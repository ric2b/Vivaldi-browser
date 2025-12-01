
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
#include "fcp/secagg/shared/shamir_secret_sharing.h"

#include <cstdint>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "fcp/secagg/shared/ecdh_keys.h"
#include "fcp/secagg/testing/ecdh_pregenerated_test_keys.h"
#include "fcp/secagg/testing/fake_prng.h"
namespace fcp {
namespace secagg {
namespace {
using ::testing::Eq;
TEST(ShamirSecretSharingTest, ShareReturnsTheAppropriateNumberOfShares) {
  ShamirSecretSharing shamir;
  std::string secret = "abcdefghijklmnopqrstuvwxyz123456";
  std::vector<ShamirShare> shares;
  for (int num_shares = 2; num_shares < 5; ++num_shares) {
    for (int threshold = 2; threshold <= num_shares; ++threshold) {
      shares = shamir.Share(threshold, num_shares, secret);
      EXPECT_THAT(shares.size(), Eq(num_shares));
    }
  }
}
TEST(ShamirSecretSharingTest, ShareFailsWhenTheSecretIsEmpty) {
  ShamirSecretSharing shamir;
  std::string secret = "";
  EXPECT_DEATH(shamir.Share(2, 5, secret), "to_share must not be empty");
}
TEST(ShamirSecretSharingTest, ShareFailsWhenNumberOfSharesIsSmall) {
  ShamirSecretSharing shamir;
  std::string secret = "abcdefghijklmnopqrstuvwxyz123456";
  EXPECT_DEATH(shamir.Share(1, 1, secret), "num_shares must be greater than 1");
}
TEST(ShamirSecretSharingTest, ShareFailsWhenTheThresholdIsOutOfBounds) {
  ShamirSecretSharing shamir;
  std::string secret = "abcdefghijklmnopqrstuvwxyz123456";
  EXPECT_DEATH(shamir.Share(6, 5, secret),
               "threshold must be at least 2 and at most num_shares");
  EXPECT_DEATH(shamir.Share(1, 5, secret),
               "threshold must be at least 2 and at most num_shares");
}
TEST(ShamirSecretSharingTest, ShareAndReconstructIntegrate) {
  ShamirSecretSharing shamir;
  std::string secret = "abcdefghijklmnopqrstuvwxyz123456";
  std::vector<ShamirShare> shares;
  int num_shares = 6;
  int threshold = 4;
  shares = shamir.Share(threshold, num_shares, secret);
  auto reconstructed_or_error =
      shamir.Reconstruct(threshold, shares, secret.size());
  EXPECT_THAT(reconstructed_or_error.ok(), Eq(true));
  EXPECT_THAT(reconstructed_or_error.value(), Eq(secret));
}
TEST(ShamirSecretSharingTest, ShareAndReconstructIntegrateWithMissingShares) {
  ShamirSecretSharing shamir;
  std::string secret = "abcdefghijklmnopqrstuvwxyz123456";
  std::vector<ShamirShare> shares;
  int num_shares = 6;
  int threshold = 4;
  shares = shamir.Share(threshold, num_shares, secret);
  shares[0].data = "";
  shares[2].data = "";
  auto reconstructed_or_error =
      shamir.Reconstruct(threshold, shares, secret.size());
  EXPECT_THAT(reconstructed_or_error.ok(), Eq(true));
  EXPECT_THAT(reconstructed_or_error.value(), Eq(secret));
}
TEST(ShamirSecretSharingTest, ShareAndReconstructIntegrateWithZeroInSecret) {
  ShamirSecretSharing shamir;
  std::string secret = "abcdefghijklmnopqrstuvwxyz123456";
  secret[26] = '\0';
  std::vector<ShamirShare> shares;
  int num_shares = 6;
  int threshold = 4;
  shares = shamir.Share(threshold, num_shares, secret);
  auto reconstructed_or_error =
      shamir.Reconstruct(threshold, shares, secret.size());
  EXPECT_THAT(reconstructed_or_error.ok(), Eq(true));
  EXPECT_THAT(reconstructed_or_error.value(), Eq(secret));
}
TEST(ShamirSecretSharingTest,
     ShareAndReconstructIntegrateWithHighOrderCharactersInSecret) {
  ShamirSecretSharing shamir;
  std::string secret = "abcdefghijklmnopqrstuvwxyz123456";
  secret[10] = static_cast<char>(128);
  secret[20] = static_cast<char>(197);
  secret[30] = static_cast<char>(255);
  std::vector<ShamirShare> shares;
  int num_shares = 6;
  int threshold = 4;
  shares = shamir.Share(threshold, num_shares, secret);
  auto reconstructed_or_error =
      shamir.Reconstruct(threshold, shares, secret.size());
  EXPECT_THAT(reconstructed_or_error.ok(), Eq(true));
  EXPECT_THAT(reconstructed_or_error.value(), Eq(secret));
}
TEST(ShamirSecretSharingTest, ShareAndReconstructIntegrateWithKeys) {
  ShamirSecretSharing shamir;
  EcdhPregeneratedTestKeys keys;
  std::vector<ShamirShare> shares;
  int num_shares = 6;
  int threshold = 4;
  shares = shamir.Share(threshold, num_shares, keys.GetPrivateKeyString(3));
  auto reconstructed_string_or_error =
      shamir.Reconstruct(threshold, shares, EcdhPrivateKey::kSize);
  EXPECT_THAT(reconstructed_string_or_error.ok(), Eq(true));
  EcdhPrivateKey reconstructed(reinterpret_cast<const uint8_t*>(
      reconstructed_string_or_error.value().c_str()));
  EXPECT_THAT(reconstructed, Eq(keys.GetPrivateKey(3)));
  EXPECT_THAT(reconstructed_string_or_error.value(),
              Eq(keys.GetPrivateKeyString(3)));
}
TEST(ShamirSecretSharingTest, ReconstructFailsIfThresholdIsInvalid) {
  ShamirSecretSharing shamir;
  std::vector<ShamirShare> shares(5, {"fake"});
  EXPECT_DEATH(auto secret_or_error = shamir.Reconstruct(1, shares, 16),
               "threshold must be at least 2");
  EXPECT_DEATH(
      auto secret_or_error = shamir.Reconstruct(6, shares, 16),
      "A vector of size 5 was provided, but threshold was specified as 6");
}
TEST(ShamirSecretSharingTest, ReconstructFailsIfSecretLengthSmall) {
  ShamirSecretSharing shamir;
  std::vector<ShamirShare> shares(5, {"fake"});
  EXPECT_DEATH(auto secret_or_error = shamir.Reconstruct(2, shares, 0),
               "secret_length must be positive");
}
TEST(ShamirSecretSharingTest, ReconstructFailsIfSharesAreInvalid) {
  ShamirSecretSharing shamir;
  std::vector<ShamirShare> shares(5, {"fakefakefakefakefake"});
  shares[0].data = "bad";
  EXPECT_DEATH(auto secret_or_error = shamir.Reconstruct(5, shares, 16),
               "Share with index 0 is invalid: a share of size 3 was provided "
               "but a multiple of 4 is expected");
  shares[0].data = "baad";
  EXPECT_DEATH(auto secret_or_error = shamir.Reconstruct(5, shares, 16),
               "Share with index 1 is invalid: all shares must match sizes");
  shares[0].data = "baadbaadbaadbaadbaadbaad";
  EXPECT_DEATH(auto secret_or_error = shamir.Reconstruct(5, shares, 16),
               "Share with index 0 is invalid: the number of subsecrets is 6 "
               "but between 1 and 5 is expected");
  shares[0].data = "";
  auto secret_or_error = shamir.Reconstruct(5, shares, 16);
  EXPECT_THAT(secret_or_error.ok(), Eq(false));
  EXPECT_THAT(secret_or_error.status().message(),
              testing::HasSubstr("Only 4 valid shares were provided, but "
                                 "threshold was specified as 5"));
}
TEST(ShamirSecretSharingTest, ReconstructWorksWithPrecomputedShares) {
  ShamirSecretSharing shamir;
  std::vector<ShamirShare> shares(5);
  int threshold = 3;
  // These shares were generated by the legacy Java code.
  uint8_t shares0[] = {112, 207, 118, 46, 110, 212, 170, 28};
  shares[0].data = std::string(reinterpret_cast<char*>(shares0), 8);
  uint8_t shares1[] = {48, 160, 197, 172, 38, 235, 145, 204};
  shares[1].data = std::string(reinterpret_cast<char*>(shares1), 8);
  uint8_t shares2[] = {63, 115, 238, 144, 40, 68, 183, 71};
  shares[2].data = std::string(reinterpret_cast<char*>(shares2), 8);
  uint8_t shares3[] = {29, 72, 240, 207, 114, 224, 26, 141};
  shares[3].data = std::string(reinterpret_cast<char*>(shares3), 8);
  uint8_t shares4[] = {74, 31, 204, 116, 6, 189, 187, 136};
  shares[4].data = std::string(reinterpret_cast<char*>(shares4), 8);
  ASSERT_THAT(shares[0].data.size(), Eq(8));
  auto reconstructed_or_error = shamir.Reconstruct(threshold, shares, 4);
  EXPECT_THAT(reconstructed_or_error.ok(), Eq(true));
  EXPECT_THAT(reconstructed_or_error.value(), Eq(std::string({0, 0, 0, 33})));
}
}  // namespace
}  // namespace secagg
}  // namespace fcp
