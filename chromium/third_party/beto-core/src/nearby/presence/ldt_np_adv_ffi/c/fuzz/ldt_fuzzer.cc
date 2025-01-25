/*
 * Copyright 2022 Google LLC
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

#include <array>
#include <vector>

#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"
#include "np_ldt.h"
#include "openssl/digest.h"
#include "openssl/hkdf.h"
#include "openssl/hmac.h"
#include "openssl/obj_mac.h"

void LdtDecryptBadMac(std::array<uint8_t, 32> key_seed_bytes,
                      std::array<uint8_t, 2> salt_bytes,
                      std::vector<uint8_t> plaintext_bytes,
                      std::array<uint8_t, 32> metadata_key_hmac_tag_bytes) {
  NpLdtKeySeed key_seed;
  memcpy(key_seed.bytes, key_seed_bytes.data(), key_seed_bytes.size());

  NpLdtSalt salt;
  memcpy(salt.bytes, salt_bytes.data(), salt_bytes.size());

  NpMetadataKeyHmac metadata_key_hmac;
  memcpy(metadata_key_hmac.bytes, metadata_key_hmac_tag_bytes.data(),
         metadata_key_hmac_tag_bytes.size());

  auto enc_handle = NpLdtEncryptCreate(key_seed);
  EXPECT_NE(0, enc_handle.handle);

  auto result = NpLdtEncrypt(enc_handle, plaintext_bytes.data(),
                             plaintext_bytes.size(), salt);
  EXPECT_EQ(NP_LDT_RESULT::NP_LDT_SUCCESS, result);

  auto dec_handle = NpLdtDecryptCreate(key_seed, metadata_key_hmac);
  EXPECT_NE(0, dec_handle.handle);
  result = NpLdtDecryptAndVerify(dec_handle, plaintext_bytes.data(),
                                 plaintext_bytes.size(), salt);
  EXPECT_EQ(NP_LDT_RESULT::NP_LDT_ERROR_MAC_MISMATCH, result)
      << "we expect mac mismatch since we're using a random mac";

  result = NpLdtEncryptClose(enc_handle);
  EXPECT_EQ(NP_LDT_RESULT::NP_LDT_SUCCESS, result);

  result = NpLdtDecryptClose(dec_handle);
  EXPECT_EQ(NP_LDT_RESULT::NP_LDT_SUCCESS, result);
}

FUZZ_TEST(LdtFuzzers, LdtDecryptBadMac)
    .WithDomains(fuzztest::Arbitrary<std::array<uint8_t, 32>>(),
                 fuzztest::Arbitrary<std::array<uint8_t, 2>>(),
                 fuzztest::Arbitrary<std::vector<uint8_t>>()
                     .WithMinSize(16)
                     .WithMaxSize(31),
                 fuzztest::Arbitrary<std::array<uint8_t, 32>>());

void LdtDecryptCorrectMac(std::array<uint8_t, 32> key_seed_bytes,
                          std::array<uint8_t, 2> salt_bytes,
                          std::vector<uint8_t> plaintext_bytes) {
  NpLdtKeySeed key_seed;
  memcpy(key_seed.bytes, key_seed_bytes.data(), key_seed_bytes.size());

  NpLdtSalt salt;
  memcpy(salt.bytes, salt_bytes.data(), salt_bytes.size());

  // calculate metadata key hmac key by HKDFing key seed
  // Reference:
  // https://commondatastorage.googleapis.com/chromium-boringssl-docs/hkdf.h.html
  // 32 byte HMAC-SHA256 key
  uint8_t metadata_key_hmac_key[32] = {0};
  auto result = HKDF(metadata_key_hmac_key, sizeof(metadata_key_hmac_key),
                     EVP_sha256(), (const uint8_t *)&key_seed.bytes, (size_t)32,
                     (const uint8_t *)"Google Nearby", (size_t)13,
                     (const uint8_t *)"V0 Identity token verification HMAC key",
                     (size_t)39);
  EXPECT_EQ(1, result);
  // calculate metadata key hmac using hkdf'd hmac key
  NpMetadataKeyHmac metadata_key_hmac = {.bytes = {0}};
  // will be written to by HMAC call, but it will always be
  // 32 because that's what SHA256 outputs
  unsigned int md_len = 32;
  // first 14 bytes of payload are metadata key
  HMAC(EVP_sha256(), metadata_key_hmac_key, 32, plaintext_bytes.data(), 14,
       metadata_key_hmac.bytes, &md_len);

  auto enc_handle = NpLdtEncryptCreate(key_seed);
  EXPECT_NE(0, enc_handle.handle);

  result = NpLdtEncrypt(enc_handle, plaintext_bytes.data(),
                        plaintext_bytes.size(), salt);
  EXPECT_EQ(NP_LDT_RESULT::NP_LDT_SUCCESS, result);

  auto dec_handle = NpLdtDecryptCreate(key_seed, metadata_key_hmac);
  EXPECT_NE(0, dec_handle.handle);
  result = NpLdtDecryptAndVerify(dec_handle, plaintext_bytes.data(),
                                 plaintext_bytes.size(), salt);
  EXPECT_EQ(NP_LDT_RESULT::NP_LDT_SUCCESS, result);

  result = NpLdtEncryptClose(enc_handle);
  EXPECT_EQ(NP_LDT_RESULT::NP_LDT_SUCCESS, result);

  result = NpLdtDecryptClose(dec_handle);
  EXPECT_EQ(NP_LDT_RESULT::NP_LDT_SUCCESS, result);
}

FUZZ_TEST(LdtFuzzers, LdtDecryptCorrectMac)
    .WithDomains(fuzztest::Arbitrary<std::array<uint8_t, 32>>(),
                 fuzztest::Arbitrary<std::array<uint8_t, 2>>(),
                 fuzztest::Arbitrary<std::vector<uint8_t>>()
                     .WithMinSize(16)
                     .WithMaxSize(31));
