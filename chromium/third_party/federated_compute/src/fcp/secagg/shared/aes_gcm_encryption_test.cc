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

#include "fcp/secagg/shared/aes_gcm_encryption.h"

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "fcp/secagg/shared/aes_key.h"

namespace fcp {
namespace secagg {
namespace {

using ::testing::Eq;
using ::testing::Ne;

// For testing purposes, make an AesKey out of a string.
AesKey MakeAesKey(const std::string& key) {
  EXPECT_THAT(key.size(), Eq(AesKey::kSize));
  return AesKey(reinterpret_cast<const uint8_t*>(key.c_str()));
}

TEST(AesGcmEncryptionTest, EncryptionThenDecryptionWorks) {
  AesGcmEncryption aes;
  AesKey key = MakeAesKey("Just some random 32 byte AES key");
  std::string test_str = "This is a test. Should work on arbitrary strings.";

  std::string ciphertext = aes.Encrypt(key, test_str);
  StatusOr<std::string> plaintext = aes.Decrypt(key, ciphertext);
  ASSERT_TRUE(plaintext.ok());
  EXPECT_THAT(plaintext.value(), Eq(test_str));
}

TEST(AesGcmEncryptionTest, MultipleOperationsWithSameObjectWork) {
  AesGcmEncryption aes;
  AesKey key1 = MakeAesKey("Just some random 32 byte AES key");
  AesKey key2 = MakeAesKey("A different 32-byte-long AES key");
  std::string test_str1 = "This is a test. Should work on arbitrary strings.";
  std::string test_str2 = "Another test string.";

  std::string ciphertext1 = aes.Encrypt(key1, test_str1);
  std::string ciphertext2 = aes.Encrypt(key2, test_str2);
  StatusOr<std::string> plaintext1 = aes.Decrypt(key1, ciphertext1);
  StatusOr<std::string> plaintext2 = aes.Decrypt(key2, ciphertext2);
  ASSERT_TRUE(plaintext1.ok());
  EXPECT_THAT(plaintext1.value(), Eq(test_str1));
  ASSERT_TRUE(plaintext2.ok());
  EXPECT_THAT(plaintext2.value(), Eq(test_str2));
}

TEST(AesGcmEncryptionTest, EncryptionsWithDifferentKeysAreDifferent) {
  AesGcmEncryption aes;
  AesKey key1 = MakeAesKey("Just some random 32 byte AES key");
  AesKey key2 = MakeAesKey("A different 32-byte-long AES key");
  std::string test_str = "This is a test. Should work on arbitrary strings.";

  std::string ciphertext1 = aes.Encrypt(key1, test_str);
  std::string ciphertext2 = aes.Encrypt(key2, test_str);
  EXPECT_THAT(ciphertext1, Ne(ciphertext2));
  StatusOr<std::string> plaintext1 = aes.Decrypt(key1, ciphertext1);
  StatusOr<std::string> plaintext2 = aes.Decrypt(key2, ciphertext2);
  ASSERT_TRUE(plaintext1.ok());
  EXPECT_THAT(plaintext1.value(), Eq(test_str));
  ASSERT_TRUE(plaintext2.ok());
  EXPECT_THAT(plaintext2.value(), Eq(test_str));
}

TEST(AesGcmEncryptionTest, VerificationFailsOnBadTag) {
  AesGcmEncryption aes;
  AesKey key = MakeAesKey("Just some random 32 byte AES key");
  std::string test_str = "This is a test. Should work on arbitrary strings.";

  std::string ciphertext = aes.Encrypt(key, test_str);
  ciphertext[ciphertext.size() - 1] = 'X';
  StatusOr<std::string> plaintext = aes.Decrypt(key, ciphertext);
  EXPECT_THAT(plaintext.ok(), Eq(false));
}

TEST(AesGcmEncryptionTest, VerificationFailsOnBadCiphertext) {
  AesGcmEncryption aes;
  AesKey key = MakeAesKey("Just some random 32 byte AES key");
  std::string test_str = "This is a test. Should work on arbitrary strings.";

  std::string ciphertext = aes.Encrypt(key, test_str);
  for (int i = 0; i < ciphertext.size(); i++) {
    // modify every bit of the ciphertext
    for (int j = 0; j < 8; j++) {
      ciphertext[i] ^= (1 << j);

      StatusOr<std::string> plaintext = aes.Decrypt(key, ciphertext);
      EXPECT_THAT(plaintext.ok(), Eq(false));

      // reset the ciphertext
      ciphertext[i] ^= (1 << j);
    }
  }
}

TEST(AesGcmEncryptionTest, VerificationFailsOnWrongKey) {
  AesGcmEncryption aes;
  AesKey key = MakeAesKey("Just some random 32 byte AES key");
  AesKey key2 = MakeAesKey("A different 32-byte-long AES key");
  std::string test_str = "This is a test. Should work on arbitrary strings.";

  std::string ciphertext = aes.Encrypt(key, test_str);
  StatusOr<std::string> plaintext = aes.Decrypt(key2, ciphertext);
  EXPECT_THAT(plaintext.ok(), Eq(false));
}

TEST(AesGcmEncryptionTest, EncryptionDiesOnEmptyKey) {
  AesGcmEncryption aes;
  std::string test_str = "This is a test. Should work on arbitrary strings.";

  EXPECT_DEATH(aes.Encrypt(AesKey(), test_str),
               "Encrypt called with blank key.");
}

TEST(AesGcmEncryptionTest, DecryptionDiesOnEmptyKey) {
  AesGcmEncryption aes;
  AesKey key = MakeAesKey("Just some random 32 byte AES key");
  std::string test_str = "This is a test. Should work on arbitrary strings.";

  std::string ciphertext = aes.Encrypt(key, test_str);
  EXPECT_DEATH(aes.Decrypt(AesKey(), ciphertext).IgnoreError(),
               "Decrypt called with blank key.");
}
TEST(AesGcmEncryptionTest, EncryptionDiesOnShortKey) {
  AesGcmEncryption aes;
  std::string test_str = "This is a test. Should work on arbitrary strings.";

  std::string bad_key_input = "only 16 byte key";
  EXPECT_DEATH(
      aes.Encrypt(
          AesKey(reinterpret_cast<const uint8_t*>(bad_key_input.c_str()), 16),
          test_str),
      "Encrypt called with key of 16 bytes, but 32 bytes are required.");
}

TEST(AesGcmEncryptionTest, DecryptionDiesOnShortKey) {
  AesGcmEncryption aes;
  AesKey key = MakeAesKey("Just some random 32 byte AES key");
  std::string test_str = "This is a test. Should work on arbitrary strings.";

  std::string ciphertext = aes.Encrypt(key, test_str);
  std::string bad_key_input = "short 17 byte key";
  EXPECT_DEATH(
      aes.Decrypt(
             AesKey(reinterpret_cast<const uint8_t*>(bad_key_input.c_str()),
                    17),
             ciphertext)
          .IgnoreError(),
      "Decrypt called with key of 17 bytes, but 32 bytes are required.");
}
}  // namespace
}  // namespace secagg
}  // namespace fcp
