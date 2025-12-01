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

#include "fcp/secagg/shared/map_of_masks.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/node_hash_map.h"
#include "absl/numeric/bits.h"
#include "absl/strings/str_cat.h"
#include "fcp/secagg/shared/aes_ctr_prng_factory.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/math.h"
#include "fcp/secagg/shared/secagg_vector.h"

namespace fcp {
namespace secagg {
namespace {

using ::testing::Eq;
using ::testing::Lt;
using ::testing::Ne;

const std::array<uint64_t, 20> kArbitraryModuli{5,
                                                39,
                                                485,
                                                2400,
                                                14901,
                                                51813,
                                                532021,
                                                13916946,
                                                39549497,
                                                548811945,
                                                590549014,
                                                48296031686,
                                                156712951284,
                                                2636861836189,
                                                14673852658160,
                                                92971495438615,
                                                304436005557271,
                                                14046234330484262,
                                                38067457113486645,
                                                175631339105057682};

TEST(AddMapsTest, AddMapsGetsRightSum_PowerOfTwo) {
  std::vector<uint64_t> vec_a{25, 50, 75, 100, 150};
  std::vector<uint64_t> vec_b{50, 100, 150, 200, 250};
  SecAggVectorMap map_a;
  map_a.emplace("test", SecAggVector(vec_a, 256));
  SecAggVectorMap map_b;
  map_b.emplace("test", SecAggVector(vec_b, 256));

  auto map_sum = AddMaps(map_a, map_b);
  std::vector<uint64_t> vec_sum = map_sum->at("test").GetAsUint64Vector();
  for (int i = 0; i < vec_a.size(); ++i) {
    EXPECT_THAT(vec_sum[i], Eq((vec_a[i] + vec_b[i]) % 256));
  }
}

TEST(AddMapsTest, AddMapsGetsRightSum_AribraryModuli) {
  std::vector<uint64_t> vec_a{25, 50, 75, 100, 150};
  std::vector<uint64_t> vec_b{50, 100, 150, 200, 250};
  SecAggVectorMap map_a;
  map_a.emplace("test", SecAggVector(vec_a, 255));
  SecAggVectorMap map_b;
  map_b.emplace("test", SecAggVector(vec_b, 255));

  auto map_sum = AddMaps(map_a, map_b);
  std::vector<uint64_t> vec_sum = map_sum->at("test").GetAsUint64Vector();
  for (int i = 0; i < vec_a.size(); ++i) {
    EXPECT_THAT(vec_sum[i], Eq((vec_a[i] + vec_b[i]) % 255));
  }
}

TEST(AddMapsTest, AddMapsExhaustiveTest_PowerOfTwo) {
  // Make SecurePrng instance to be used as a consistent pseudo-random number
  // generator.
  uint8_t seed_data[32];
  memset(seed_data, '1', 32);
  AesKey seed(seed_data);
  AesCtrPrngFactory prng_factory;
  std::unique_ptr<SecurePrng> prng = prng_factory.MakePrng(seed);

  // Iterate through all possible bitwidths, add two random vectors, and
  // verify the results.
  for (int number_of_bits = 1;
       number_of_bits <= absl::bit_width(SecAggVector::kMaxModulus - 1);
       ++number_of_bits) {
    uint64_t modulus = 1ULL << number_of_bits;
    constexpr size_t kSize = 1000;
    std::vector<uint64_t> vec_a(kSize);
    std::vector<uint64_t> vec_b(kSize);
    for (size_t i = 0; i < kSize; i++) {
      vec_a[i] = prng->Rand64() % modulus;
      vec_b[i] = prng->Rand64() % modulus;
    }

    SecAggVectorMap map_a;
    map_a.emplace("test", SecAggVector(vec_a, modulus));
    SecAggVectorMap map_b;
    map_b.emplace("test", SecAggVector(vec_b, modulus));

    auto map_sum = AddMaps(map_a, map_b);
    std::vector<uint64_t> vec_sum = map_sum->at("test").GetAsUint64Vector();
    for (size_t i = 0; i < kSize; i++) {
      EXPECT_THAT(vec_sum[i], Eq((vec_a[i] + vec_b[i]) % modulus));
    }
  }
}

TEST(AddMapsTest, AddMapsExhaustiveTest_ArbitraryModuli) {
  // Make SecurePrng instance to be used as a consistent pseudo-random number
  // generator.
  uint8_t seed_data[32];
  memset(seed_data, '1', 32);
  AesKey seed(seed_data);
  AesCtrPrngFactory prng_factory;
  std::unique_ptr<SecurePrng> prng = prng_factory.MakePrng(seed);

  // Iterate through all possible bitwidths, add two random vectors, and
  // verify the results.
  for (uint64_t modulus : kArbitraryModuli) {
    constexpr size_t kSize = 1000;
    std::vector<uint64_t> vec_a(kSize);
    std::vector<uint64_t> vec_b(kSize);
    for (size_t i = 0; i < kSize; i++) {
      vec_a[i] = prng->Rand64() % modulus;
      vec_b[i] = prng->Rand64() % modulus;
    }

    SecAggVectorMap map_a;
    map_a.emplace("test", SecAggVector(vec_a, modulus));
    SecAggVectorMap map_b;
    map_b.emplace("test", SecAggVector(vec_b, modulus));

    auto map_sum = AddMaps(map_a, map_b);
    std::vector<uint64_t> vec_sum = map_sum->at("test").GetAsUint64Vector();
    for (size_t i = 0; i < kSize; i++) {
      EXPECT_THAT(vec_sum[i], Eq((vec_a[i] + vec_b[i]) % modulus));
    }
  }
}

enum MapOfMasksVersion { CURRENT, V3, UNPACKED };

class MapOfMasksTest : public ::testing::TestWithParam<MapOfMasksVersion> {
 public:
  using Uint64VectorMap =
      absl::node_hash_map<std::string, std::vector<uint64_t>>;

  std::unique_ptr<Uint64VectorMap> MapOfMasks(
      const std::vector<AesKey>& prng_keys_to_add,
      const std::vector<AesKey>& prng_keys_to_subtract,
      const std::vector<InputVectorSpecification>& input_vector_specs,
      const SessionId& session_id, const AesPrngFactory& prng_factory) {
    if (GetParam() == MapOfMasksVersion::UNPACKED) {
      return ToUint64VectorMap(fcp::secagg::UnpackedMapOfMasks(
          prng_keys_to_add, prng_keys_to_subtract, input_vector_specs,
          session_id, prng_factory));
    } else if (GetParam() == MapOfMasksVersion::V3) {
      return ToUint64VectorMap(fcp::secagg::MapOfMasksV3(
          prng_keys_to_add, prng_keys_to_subtract, input_vector_specs,
          session_id, prng_factory));
    } else {
      return ToUint64VectorMap(fcp::secagg::MapOfMasks(
          prng_keys_to_add, prng_keys_to_subtract, input_vector_specs,
          session_id, prng_factory));
    }
  }

 private:
  std::unique_ptr<Uint64VectorMap> ToUint64VectorMap(
      std::unique_ptr<SecAggVectorMap> map) {
    auto result = std::make_unique<Uint64VectorMap>();
    for (auto& [name, vec] : *map) {
      result->emplace(name, vec.GetAsUint64Vector());
    }
    return result;
  }

  std::unique_ptr<Uint64VectorMap> ToUint64VectorMap(
      std::unique_ptr<SecAggUnpackedVectorMap> map) {
    auto result = std::make_unique<Uint64VectorMap>();
    for (auto& [name, vec] : *map) {
      result->emplace(name, std::move(vec));
    }
    return result;
  }
};

// AES MapOfMasks: Power-of-two Moduli

TEST_P(MapOfMasksTest, ReturnsZeroIfNoKeysSpecified_PowerOfTwo) {
  std::vector<AesKey> prng_keys_to_add;
  std::vector<AesKey> prng_keys_to_subtract;
  SessionId session_id = {std::string(32, 'Z')};
  std::vector<InputVectorSpecification> vector_specs;
  vector_specs.push_back(InputVectorSpecification("test", 10, 1ULL << 20));

  auto masks = MapOfMasks(prng_keys_to_add, prng_keys_to_subtract, vector_specs,
                          session_id, AesCtrPrngFactory());

  EXPECT_THAT(masks->size(), Eq(1));
  std::vector<uint64_t> zeroes(10, 0);
  EXPECT_THAT(masks->at("test"), Eq(std::vector<uint64_t>(10, 0)));
}

TEST_P(MapOfMasksTest, ReturnsNonZeroIfOneKeySpecified_PowerOfTwo) {
  std::vector<AesKey> prng_keys_to_add;
  uint8_t key[AesKey::kSize];
  memset(key, 'A', AesKey::kSize);
  prng_keys_to_add.push_back(AesKey(key));
  std::vector<AesKey> prng_keys_to_subtract;
  SessionId session_id = {std::string(32, 'Z')};
  std::vector<InputVectorSpecification> vector_specs;
  vector_specs.push_back(InputVectorSpecification("test", 10, 1ULL << 20));

  auto masks = MapOfMasks(prng_keys_to_add, prng_keys_to_subtract, vector_specs,
                          session_id, AesCtrPrngFactory());

  EXPECT_THAT(masks->size(), Eq(1));
  EXPECT_THAT(masks->at("test"), Ne(std::vector<uint64_t>(10, 0)));
}

TEST_P(MapOfMasksTest, MapWithOneKeyDiffersFromMapWithTwoKeys_PowerOfTwo) {
  std::vector<AesKey> prng_keys_to_add;
  uint8_t
      key[AesKey::kSize];  // This key is reusable because AesKey makes a copy
  memset(key, 'A', AesKey::kSize);
  prng_keys_to_add.push_back(AesKey(key));
  std::vector<AesKey> prng_keys_to_subtract;
  SessionId session_id = {std::string(32, 'Z')};
  std::vector<InputVectorSpecification> vector_specs;
  vector_specs.push_back(InputVectorSpecification("test", 10, 1ULL << 20));

  auto masks1 = MapOfMasks(prng_keys_to_add, prng_keys_to_subtract,
                           vector_specs, session_id, AesCtrPrngFactory());

  memset(key, 'B', AesKey::kSize);
  prng_keys_to_add.push_back(AesKey(key));
  auto masks2 = MapOfMasks(prng_keys_to_add, prng_keys_to_subtract,
                           vector_specs, session_id, AesCtrPrngFactory());

  EXPECT_THAT(masks1->size(), Eq(1));
  EXPECT_THAT(masks2->size(), Eq(1));
  EXPECT_THAT(masks2->at("test"), Ne(masks1->at("test")));
}

TEST_P(MapOfMasksTest, MapsWithOppositeMasksCancel_PowerOfTwo) {
  std::vector<AesKey> prng_keys_to_add;
  uint8_t key[AesKey::kSize];
  memset(key, 'A', AesKey::kSize);
  prng_keys_to_add.push_back(AesKey(key));
  memset(key, 'B', AesKey::kSize);
  prng_keys_to_add.push_back(AesKey(key));
  std::vector<AesKey> prng_keys_to_subtract;
  SessionId session_id = {std::string(32, 'Z')};
  std::vector<InputVectorSpecification> vector_specs;
  vector_specs.push_back(InputVectorSpecification("test", 10, 1ULL << 20));

  auto masks1 = MapOfMasks(prng_keys_to_add, prng_keys_to_subtract,
                           vector_specs, session_id, AesCtrPrngFactory());

  auto masks2 = MapOfMasks(prng_keys_to_subtract, prng_keys_to_add,
                           vector_specs, session_id, AesCtrPrngFactory());

  EXPECT_THAT(masks1->size(), Eq(1));
  EXPECT_THAT(masks2->size(), Eq(1));
  std::vector<uint64_t> mask_vector1 = masks1->at("test");
  std::vector<uint64_t> mask_vector2 = masks2->at("test");
  for (int i = 0; i < 10; ++i) {
    EXPECT_THAT(AddMod(mask_vector1[i], mask_vector2[i], 1ULL << 20), Eq(0));
  }
}

TEST_P(MapOfMasksTest, MapsWithMixedOppositeMasksCancel_PowerOfTwo) {
  std::vector<AesKey> prng_keys_to_add;
  uint8_t key[AesKey::kSize];
  memset(key, 'A', AesKey::kSize);
  prng_keys_to_add.push_back(AesKey(key));
  memset(key, 'B', AesKey::kSize);
  std::vector<AesKey> prng_keys_to_subtract;
  prng_keys_to_subtract.push_back(AesKey(key));
  SessionId session_id = {std::string(32, 'Z')};
  std::vector<InputVectorSpecification> vector_specs;
  vector_specs.push_back(InputVectorSpecification("test", 10, 1ULL << 20));

  auto masks1 = MapOfMasks(prng_keys_to_add, prng_keys_to_subtract,
                           vector_specs, session_id, AesCtrPrngFactory());

  auto masks2 = MapOfMasks(prng_keys_to_subtract, prng_keys_to_add,
                           vector_specs, session_id, AesCtrPrngFactory());

  EXPECT_THAT(masks1->size(), Eq(1));
  EXPECT_THAT(masks2->size(), Eq(1));
  std::vector<uint64_t> mask_vector1 = masks1->at("test");
  std::vector<uint64_t> mask_vector2 = masks2->at("test");
  for (int i = 0; i < 10; ++i) {
    EXPECT_THAT(AddMod(mask_vector1[i], mask_vector2[i], 1ULL << 20), Eq(0));
  }
}

TEST_P(MapOfMasksTest, PrngMaskGeneratesCorrectBitwidthMasks_PowerOfTwo) {
  std::vector<AesKey> prng_keys_to_add;
  uint8_t key[AesKey::kSize];
  memset(key, 'A', AesKey::kSize);
  prng_keys_to_add.push_back(AesKey(key));
  std::vector<AesKey> prng_keys_to_subtract;
  SessionId session_id = {std::string(32, 'Z')};
  std::vector<InputVectorSpecification> vector_specs;

  // Check a variety of bit_widths
  std::vector<uint64_t> moduli{1ULL << 1, 1ULL << 4, 1ULL << 20, 1ULL << 24,
                               SecAggVector::kMaxModulus};
  for (uint64_t i : moduli) {
    vector_specs.push_back(
        InputVectorSpecification(absl::StrCat("test", i), 50, i));
  }

  auto masks = MapOfMasks(prng_keys_to_add, prng_keys_to_subtract, vector_specs,
                          session_id, AesCtrPrngFactory());

  // Make sure all elements are less than the bound, and also at least one of
  // them has the highest-allowed bit set.
  for (uint64_t modulus : moduli) {
    auto vec = masks->at(absl::StrCat("test", modulus));
    bool high_order_bit_set = false;
    for (uint64_t mask : vec) {
      EXPECT_THAT(mask, Lt(modulus));
      if (mask >= (modulus >> 1)) {
        high_order_bit_set = true;
      }
    }
    EXPECT_THAT(high_order_bit_set, Eq(true));
  }
}

// AES MapOfMasks: Arbitrary Moduli

TEST_P(MapOfMasksTest, ReturnsZeroIfNoKeysSpecified_ArbitraryModuli) {
  uint64_t modulus = 2636861836189;
  std::vector<AesKey> prng_keys_to_add;
  std::vector<AesKey> prng_keys_to_subtract;
  SessionId session_id = {std::string(32, 'Z')};
  std::vector<InputVectorSpecification> vector_specs;
  vector_specs.push_back(InputVectorSpecification("test", 10, modulus));

  auto masks = MapOfMasks(prng_keys_to_add, prng_keys_to_subtract, vector_specs,
                          session_id, AesCtrPrngFactory());

  EXPECT_THAT(masks->size(), Eq(1));
  std::vector<uint64_t> zeroes(10, 0);
  EXPECT_THAT(masks->at("test"), Eq(std::vector<uint64_t>(10, 0)));
}

TEST_P(MapOfMasksTest, ReturnsNonZeroIfOneKeySpecified_ArbitraryModuli) {
  uint64_t modulus = 2636861836189;
  std::vector<AesKey> prng_keys_to_add;
  uint8_t key[AesKey::kSize];
  memset(key, 'A', AesKey::kSize);
  prng_keys_to_add.push_back(AesKey(key));
  std::vector<AesKey> prng_keys_to_subtract;
  SessionId session_id = {std::string(32, 'Z')};
  std::vector<InputVectorSpecification> vector_specs;
  vector_specs.push_back(InputVectorSpecification("test", 10, modulus));

  auto masks = MapOfMasks(prng_keys_to_add, prng_keys_to_subtract, vector_specs,
                          session_id, AesCtrPrngFactory());

  EXPECT_THAT(masks->size(), Eq(1));
  EXPECT_THAT(masks->at("test"), Ne(std::vector<uint64_t>(10, 0)));
}

TEST_P(MapOfMasksTest, MapWithOneKeyDiffersFromMapWithTwoKeys_ArbitraryModuli) {
  uint64_t modulus = 2636861836189;
  std::vector<AesKey> prng_keys_to_add;
  uint8_t
      key[AesKey::kSize];  // This key is reusable because AesKey makes a copy
  memset(key, 'A', AesKey::kSize);
  prng_keys_to_add.push_back(AesKey(key));
  std::vector<AesKey> prng_keys_to_subtract;
  SessionId session_id = {std::string(32, 'Z')};
  std::vector<InputVectorSpecification> vector_specs;
  vector_specs.push_back(InputVectorSpecification("test", 10, modulus));

  auto masks1 = MapOfMasks(prng_keys_to_add, prng_keys_to_subtract,
                           vector_specs, session_id, AesCtrPrngFactory());

  memset(key, 'B', AesKey::kSize);
  prng_keys_to_add.push_back(AesKey(key));
  auto masks2 = MapOfMasks(prng_keys_to_add, prng_keys_to_subtract,
                           vector_specs, session_id, AesCtrPrngFactory());

  EXPECT_THAT(masks1->size(), Eq(1));
  EXPECT_THAT(masks2->size(), Eq(1));
  EXPECT_THAT(masks2->at("test"), Ne(masks1->at("test")));
}

TEST_P(MapOfMasksTest, MapsAreDeterministic_KeysToAdd_ArbitraryModuli) {
  uint64_t modulus = 2636861836189;
  uint8_t key[AesKey::kSize];
  // prng_keys_to_add includes A
  std::vector<AesKey> prng_keys_to_add;
  memset(key, 'A', AesKey::kSize);
  prng_keys_to_add.push_back(AesKey(key));

  // prng_keys_to_subtract includes B
  std::vector<AesKey> prng_keys_to_subtract;
  memset(key, 'B', AesKey::kSize);
  prng_keys_to_subtract.push_back(AesKey(key));

  SessionId session_id = {std::string(32, 'Z')};
  std::vector<InputVectorSpecification> vector_specs;
  vector_specs.push_back(InputVectorSpecification("test", 10, modulus));

  auto masks1 = MapOfMasks(prng_keys_to_add, prng_keys_to_subtract,
                           vector_specs, session_id, AesCtrPrngFactory());

  auto masks2 = MapOfMasks(prng_keys_to_add, prng_keys_to_subtract,
                           vector_specs, session_id, AesCtrPrngFactory());

  EXPECT_THAT(masks1->size(), Eq(1));
  EXPECT_THAT(masks2->size(), Eq(1));
  std::vector<uint64_t> mask_vector1 = masks1->at("test");
  std::vector<uint64_t> mask_vector2 = masks2->at("test");
  for (int i = 0; i < 10; ++i) {
    EXPECT_THAT(mask_vector1[i], Eq(mask_vector2[i]));
  }
}

TEST_P(MapOfMasksTest, MapsWithOppositeMasksCancel_ArbitraryModuli) {
  uint64_t modulus = 2636861836189;
  uint8_t key[AesKey::kSize];
  // prng_keys_to_add includes A & B
  std::vector<AesKey> prng_keys_to_add;
  memset(key, 'A', AesKey::kSize);
  prng_keys_to_add.push_back(AesKey(key));
  memset(key, 'B', AesKey::kSize);
  prng_keys_to_add.push_back(AesKey(key));
  // prng_keys_to_subtract is empty
  std::vector<AesKey> prng_keys_to_subtract;

  SessionId session_id = {std::string(32, 'Z')};
  std::vector<InputVectorSpecification> vector_specs;
  vector_specs.push_back(InputVectorSpecification("test", 10, modulus));

  auto masks1 = MapOfMasks(prng_keys_to_add, prng_keys_to_subtract,
                           vector_specs, session_id, AesCtrPrngFactory());

  auto masks2 = MapOfMasks(prng_keys_to_subtract, prng_keys_to_add,
                           vector_specs, session_id, AesCtrPrngFactory());

  EXPECT_THAT(masks1->size(), Eq(1));
  EXPECT_THAT(masks2->size(), Eq(1));
  std::vector<uint64_t> mask_vector1 = masks1->at("test");
  std::vector<uint64_t> mask_vector2 = masks2->at("test");
  for (int i = 0; i < 10; ++i) {
    EXPECT_THAT(AddMod(mask_vector1[i], mask_vector2[i], modulus), Eq(0));
  }
}

TEST_P(MapOfMasksTest, MapsWithMixedOppositeMasksCancel_ArbitraryModuli) {
  uint64_t modulus = 2636861836189;
  uint8_t key[AesKey::kSize];
  // prng_keys_to_add includes A
  std::vector<AesKey> prng_keys_to_add;
  memset(key, 'A', AesKey::kSize);
  prng_keys_to_add.push_back(AesKey(key));
  // prng_keys_to_subtract includes B
  std::vector<AesKey> prng_keys_to_subtract;
  memset(key, 'B', AesKey::kSize);
  prng_keys_to_subtract.push_back(AesKey(key));

  SessionId session_id = {std::string(32, 'Z')};
  std::vector<InputVectorSpecification> vector_specs;
  vector_specs.push_back(InputVectorSpecification("test", 10, modulus));

  auto masks1 = MapOfMasks(prng_keys_to_add, prng_keys_to_subtract,
                           vector_specs, session_id, AesCtrPrngFactory());

  auto masks2 = MapOfMasks(prng_keys_to_subtract, prng_keys_to_add,
                           vector_specs, session_id, AesCtrPrngFactory());

  EXPECT_THAT(masks1->size(), Eq(1));
  EXPECT_THAT(masks2->size(), Eq(1));
  std::vector<uint64_t> mask_vector1 = masks1->at("test");
  std::vector<uint64_t> mask_vector2 = masks2->at("test");
  for (int i = 0; i < 10; ++i) {
    EXPECT_THAT(AddMod(mask_vector1[i], mask_vector2[i], modulus), Eq(0));
  }
}

TEST_P(MapOfMasksTest, PrngMaskGeneratesCorrectBitwidthMasks_ArbitraryModuli) {
  std::vector<AesKey> prng_keys_to_add;
  uint8_t key[AesKey::kSize];
  memset(key, 'A', AesKey::kSize);
  prng_keys_to_add.push_back(AesKey(key));
  std::vector<AesKey> prng_keys_to_subtract;
  SessionId session_id = {std::string(32, 'Z')};
  std::vector<InputVectorSpecification> vector_specs;

  // Check a variety of bit_widths
  for (uint64_t i : kArbitraryModuli) {
    vector_specs.push_back(
        InputVectorSpecification(absl::StrCat("test", i), 50, i));
  }

  auto masks = MapOfMasks(prng_keys_to_add, prng_keys_to_subtract, vector_specs,
                          session_id, AesCtrPrngFactory());

  // Make sure all elements are less than the bound, and also at least one of
  // them has the highest-allowed bit set.
  for (uint64_t modulus : kArbitraryModuli) {
    auto vec = masks->at(absl::StrCat("test", modulus));
    bool high_order_bit_set = false;
    for (uint64_t mask : vec) {
      EXPECT_THAT(mask, Lt(modulus));
      if (mask >= (modulus >> 1)) {
        high_order_bit_set = true;
      }
    }
    EXPECT_THAT(high_order_bit_set, Eq(true));
  }
}

INSTANTIATE_TEST_SUITE_P(MapOfMasksTest, MapOfMasksTest,
                         ::testing::Values<MapOfMasksVersion>(CURRENT, V3,
                                                              UNPACKED));

}  // namespace
}  // namespace secagg
}  // namespace fcp
