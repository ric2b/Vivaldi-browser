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

#include "fcp/secagg/shared/aes_ctr_prng.h"

#include <cstdint>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "fcp/secagg/shared/aes_ctr_prng_factory.h"

namespace fcp {
namespace secagg {
namespace {

using ::testing::Contains;
using ::testing::Eq;
using ::testing::Ne;
using ::testing::Not;
using ::testing::Pointwise;

TEST(AesCtrPrngTest, Rand8ReturnsSameValuesGivenSameInputs) {
  uint8_t seed_data[32];
  memset(seed_data, '1', 32);
  AesKey seed(seed_data);

  AesCtrPrngFactory factory;
  std::unique_ptr<SecurePrng> prng0 = factory.MakePrng(seed);
  std::unique_ptr<SecurePrng> prng1 = factory.MakePrng(seed);
  std::vector<uint8_t> output0;
  std::vector<uint8_t> output1;
  for (int i = 0; i < 16; ++i) {
    output0.push_back(prng0->Rand8());
    output1.push_back(prng1->Rand8());
  }
  EXPECT_THAT(output0, Eq(output1));
}

TEST(AesCtrPrngTest, Rand64ReturnsSameValuesGivenSameInputs) {
  uint8_t seed_data[32];
  memset(seed_data, '1', 32);
  AesKey seed(seed_data);

  AesCtrPrngFactory factory;
  std::unique_ptr<SecurePrng> prng0 = factory.MakePrng(seed);
  std::unique_ptr<SecurePrng> prng1 = factory.MakePrng(seed);
  std::vector<uint64_t> output0;
  std::vector<uint64_t> output1;
  for (int i = 0; i < 16; ++i) {
    output0.push_back(prng0->Rand64());
    output1.push_back(prng1->Rand64());
  }
  EXPECT_THAT(output0, Eq(output1));
}

TEST(AesCtrPrngTest, MixedRandCallsReturnSameValuesGivenSameInputs) {
  uint8_t seed_data[32];
  memset(seed_data, '1', 32);
  AesKey seed(seed_data);

  AesCtrPrngFactory factory;
  std::unique_ptr<SecurePrng> prng0 = factory.MakePrng(seed);
  std::unique_ptr<SecurePrng> prng1 = factory.MakePrng(seed);
  std::vector<uint64_t> output0;
  std::vector<uint64_t> output1;
  for (int i = 0; i < 5; ++i) {
    output0.push_back(prng0->Rand8());
    output1.push_back(prng1->Rand8());
  }
  for (int i = 0; i < 5; ++i) {
    output0.push_back(prng0->Rand64());
    output1.push_back(prng1->Rand64());
  }
  for (int i = 0; i < 10; ++i) {
    output0.push_back(prng0->Rand8());
    output1.push_back(prng1->Rand8());
  }
  EXPECT_THAT(output0, Eq(output1));
}

// While for random seeds or IVs there would be a very small chance of
// duplication, these tests are not flaky because this PRNG is deterministic.
TEST(AesCtrPrngTest, DifferentSeedsGenerateDifferentValues) {
  uint8_t seed_data[32];
  memset(seed_data, '1', 32);
  AesKey seed1(seed_data);
  memset(seed_data, '3', 32);
  AesKey seed2(seed_data);

  AesCtrPrngFactory factory;
  std::unique_ptr<SecurePrng> prng0 = factory.MakePrng(seed1);
  std::unique_ptr<SecurePrng> prng1 = factory.MakePrng(seed2);
  std::vector<uint64_t> output0;
  std::vector<uint64_t> output1;
  for (int i = 0; i < 16; ++i) {
    output0.push_back(prng0->Rand64());
    output1.push_back(prng1->Rand64());
  }
  // output0 differs from output1 at every point
  EXPECT_THAT(output0, Pointwise(Ne(), output1));
}

TEST(AesCtrPrngTest, DoesntGenerateRepeatedValues) {
  uint8_t seed_data[32];
  memset(seed_data, '1', 32);
  AesKey seed(seed_data);

  AesCtrPrngFactory factory;
  std::unique_ptr<SecurePrng> prng = factory.MakePrng(seed);
  std::vector<uint64_t> output;
  uint64_t val;
  for (int i = 0; i < 16; ++i) {
    val = prng->Rand64();
    EXPECT_THAT(output, Not(Contains(val)));
    output.push_back(val);
  }
}

TEST(AesCtrPrngTest, GeneratesExpectedValues) {
  uint8_t iv[16];
  memset(iv, 0, sizeof(iv));

  uint8_t seed_data[32];
  memset(seed_data, '1', sizeof(seed_data));
  AesKey seed(seed_data);

  EVP_CIPHER_CTX* ctx;
  ctx = EVP_CIPHER_CTX_new();
  ASSERT_THAT(ctx, Ne(nullptr));

  ASSERT_THAT(
      EVP_EncryptInit_ex(ctx, EVP_aes_256_ctr(), nullptr, seed_data, iv),
      Eq(1));

  const int kBlockSize = 16 * 32;

  static constexpr uint8_t zeroes[kBlockSize] = {0};

  // These are processed separately in the class
  uint8_t expected_uint8_t[kBlockSize];
  uint8_t expected_uint64_t[kBlockSize];

  // Obtain the ciphertext incrementally to verify identical output of versions
  // using a different block size.
  int len;
  for (auto i = 0; i < kBlockSize; i += 16) {
    ASSERT_THAT(EVP_EncryptUpdate(ctx, &expected_uint8_t[i], &len, zeroes, 16),
                Ne(0));
    ASSERT_THAT(len, 16);
  }
  for (auto i = 0; i < kBlockSize; i += 16) {
    ASSERT_THAT(EVP_EncryptUpdate(ctx, &expected_uint64_t[i], &len, zeroes, 16),
                Ne(0));
    ASSERT_THAT(len, 16);
  }

  AesCtrPrngFactory factory;
  std::unique_ptr<SecurePrng> prng = factory.MakePrng(seed);

  for (int i = 0; i < sizeof(expected_uint8_t); i++) {
    EXPECT_THAT(prng->Rand8(), Eq(expected_uint8_t[i]));
  }
  for (int i = 0; i < sizeof(expected_uint64_t) / sizeof(uint64_t); i++) {
    uint64_t value = 0;
    for (int j = 0; j < sizeof(uint64_t); j++) {
      value |=
          static_cast<uint64_t>(expected_uint64_t[i * sizeof(uint64_t) + j])
          << (8 * j);
    }
    EXPECT_THAT(prng->Rand64(), Eq(value));
  }
  EVP_CIPHER_CTX_free(ctx);
}

TEST(AesCtrPrngTest, RandBufferIsConsistentWithRand8) {
  uint8_t seed_data[32];
  memset(seed_data, '1', 32);
  AesKey seed(seed_data);

  AesCtrPrngFactory factory1;
  AesCtrPrngFactory factory2;
  std::unique_ptr<SecurePrng> prng1 = factory1.MakePrng(seed);
  std::unique_ptr<SecurePrng> prng2 = factory2.MakePrng(seed);
  auto batch_prng = static_cast<SecureBatchPrng*>(prng2.get());

  constexpr int kSize = 16000;
  std::vector<uint8_t> output1(kSize);
  std::vector<uint8_t> output2(kSize);

  // Fill output1 using Rand8
  for (int i = 0; i < kSize; ++i) {
    output1[i] = prng1->Rand8();
  }

  // Fill output2 using RandBuffer
  int bytes_received = 0;
  while (bytes_received < kSize) {
    bytes_received += batch_prng->RandBuffer(output2.data() + bytes_received,
                                             kSize - bytes_received);
  }

  // output1 and output2 should be the same.
  EXPECT_THAT(output1, Eq(output2));
}

}  // namespace
}  // namespace secagg
}  // namespace fcp
