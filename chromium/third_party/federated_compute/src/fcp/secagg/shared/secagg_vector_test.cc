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

#include "fcp/secagg/shared/secagg_vector.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "fcp/secagg/shared/math.h"

namespace fcp {
namespace secagg {
namespace {

using ::testing::ElementsAreArray;
using ::testing::Eq;
using SecAggVectorTest = ::testing::TestWithParam<bool>;

static std::array<uint64_t, 20> kArbitraryModuli{5,
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

TEST_P(SecAggVectorTest, GettersReturnAppropriateValuesOnConstructedVector) {
  std::vector<uint64_t> raw_vector = {4, 5};
  uint64_t modulus = 256;
  SecAggVector vector(raw_vector, modulus, GetParam());
  EXPECT_THAT(modulus, Eq(vector.modulus()));
  EXPECT_THAT(8, Eq(vector.bit_width()));
  EXPECT_THAT(raw_vector.size(), Eq(vector.num_elements()));
  EXPECT_THAT(raw_vector, Eq(vector.GetAsUint64Vector()));
}

TEST_P(SecAggVectorTest, ConstructorDoesNotDieOnInputsCloseToModulusBound) {
  std::vector<uint64_t> raw_vector = {0, 3};
  SecAggVector vector(raw_vector, 4, GetParam());
}

TEST_P(SecAggVectorTest, ConstructorDiesOnInputEqualsModulus) {
  std::vector<uint64_t> raw_vector = {4};
  EXPECT_DEATH(SecAggVector vector(raw_vector, 4, GetParam()),
               "The span does not have the appropriate modulus");
}

TEST_P(SecAggVectorTest, ConstructorDiesOnInputBiggerThanMaxModulus) {
  std::vector<uint64_t> raw_vector = {SecAggVector::kMaxModulus};
  EXPECT_DEATH(
      SecAggVector vector(raw_vector, SecAggVector::kMaxModulus, GetParam()),
      "The span does not have the appropriate modulus");
}

TEST_P(SecAggVectorTest, ConstructorDiesOnNegativeModulus) {
  std::vector<uint64_t> raw_vector = {4};
  EXPECT_DEATH(SecAggVector vector(raw_vector, -2, GetParam()),
               "The specified modulus is not valid");
}

TEST_P(SecAggVectorTest, ConstructorDiesOnModulusZero) {
  std::vector<uint64_t> raw_vector = {4};
  EXPECT_DEATH(SecAggVector vector(raw_vector, 0, GetParam()),
               "The specified modulus is not valid");
}

TEST_P(SecAggVectorTest, ConstructorDiesOnModulusOne) {
  std::vector<uint64_t> raw_vector = {4};
  EXPECT_DEATH(SecAggVector vector(raw_vector, 1, GetParam()),
               "The specified modulus is not valid");
}

TEST_P(SecAggVectorTest, ConstructorDiesOnModulusTooLarge) {
  std::vector<uint64_t> raw_vector = {4};
  EXPECT_DEATH(SecAggVector vector(raw_vector, SecAggVector::kMaxModulus + 1,
                                   GetParam()),
               "The specified modulus is not valid");
}

TEST_P(SecAggVectorTest, StringConstructorSucceedsOnValidInputs) {
  std::string packed_bytes(3, '\0');
  SecAggVector vector(packed_bytes, 4, 12, GetParam());

  // empty vector
  std::string packed_bytes2 = "";
  SecAggVector vector2(packed_bytes2, 32, 0, GetParam());

  // lines up with byte boundary
  std::string packed_bytes3(4, '\0');
  SecAggVector vector3(packed_bytes3, 1ULL << 16, 2, GetParam());
}

TEST_P(SecAggVectorTest, StringConstructorDiesOnNegativeModulus) {
  std::string packed_bytes(3, '\0');
  EXPECT_DEATH(SecAggVector vector(packed_bytes, -2, 4, GetParam()),
               "The specified modulus is not valid");
}

TEST_P(SecAggVectorTest, StringConstructorDiesOnModulusZero) {
  std::string packed_bytes(3, '\0');
  EXPECT_DEATH(SecAggVector vector(packed_bytes, 0, 4, GetParam()),
               "The specified modulus is not valid");
}

TEST_P(SecAggVectorTest, StringConstructorDiesOnModulusOne) {
  std::string packed_bytes(3, '\0');
  EXPECT_DEATH(SecAggVector vector(packed_bytes, 1, 4, GetParam()),
               "The specified modulus is not valid");
}

TEST_P(SecAggVectorTest, StringConstructorDiesOnModulusTooLarge) {
  std::string packed_bytes(3, '\0');
  EXPECT_DEATH(SecAggVector vector(packed_bytes, SecAggVector::kMaxModulus + 1,
                                   4, GetParam()),
               "The specified modulus is not valid");
}

TEST_P(SecAggVectorTest, StringConstructorDiesOnTooShortString) {
  int num_elements = 4;
  uint64_t modulus = 16;
  int bit_width = 4;
  int expected_length = DivideRoundUp(num_elements * bit_width, 8);

  std::string packed_bytes(expected_length - 1, '\0');
  EXPECT_DEATH(SecAggVector vector(packed_bytes, modulus, 4, GetParam()),
               "The supplied string is not the right size");
}

TEST_P(SecAggVectorTest, StringConstructorDiesOnTooLongString) {
  int num_elements = 4;
  uint64_t modulus = 16;
  int bit_width = 4;
  int expected_length = DivideRoundUp(num_elements * bit_width, 8);

  std::string packed_bytes(expected_length + 1, '\0');
  EXPECT_DEATH(SecAggVector vector(packed_bytes, modulus, 4, GetParam()),
               "The supplied string is not the right size");
}

TEST_P(SecAggVectorTest, PackedVectorHasCorrectSize) {
  std::vector<uint64_t> raw_vector = {0, 1, 2, 3, 4};
  uint64_t modulus = 32;
  int bit_width = 5;
  SecAggVector vector(raw_vector, modulus, GetParam());
  std::string packed_bytes = vector.GetAsPackedBytes();
  int expected_length = DivideRoundUp(raw_vector.size() * bit_width, 8);
  EXPECT_THAT(expected_length, Eq(packed_bytes.size()));

  // empty vector
  std::vector<uint64_t> empty_raw_vector = {};
  modulus = 32;
  bit_width = 5;
  SecAggVector vector2(empty_raw_vector, modulus, GetParam());
  packed_bytes = vector2.GetAsPackedBytes();
  expected_length = 0;
  EXPECT_THAT(expected_length, Eq(packed_bytes.size()));

  // packed_bytes lines up with byte boundary
  modulus = 1ULL << 16;
  bit_width = 16;
  SecAggVector vector3(raw_vector, modulus, GetParam());
  packed_bytes = vector3.GetAsPackedBytes();
  expected_length = DivideRoundUp(raw_vector.size() * bit_width, 8);
  EXPECT_THAT(expected_length, Eq(packed_bytes.size()));

  // max bit_width
  modulus = 1ULL << 62;
  bit_width = 62;
  SecAggVector vector4(raw_vector, modulus, GetParam());
  packed_bytes = vector4.GetAsPackedBytes();
  expected_length = DivideRoundUp(raw_vector.size() * bit_width, 8);
  EXPECT_THAT(expected_length, Eq(packed_bytes.size()));
}

TEST_P(SecAggVectorTest, PackedVectorUnpacksToSameValues) {
  std::vector<uint64_t> raw_vector = {};
  uint64_t modulus = 32;
  SecAggVector vector(raw_vector, modulus, GetParam());
  std::string packed_bytes = vector.GetAsPackedBytes();
  SecAggVector unpacked_vector(packed_bytes, modulus, raw_vector.size(),
                               GetParam());
  EXPECT_THAT(raw_vector, Eq(unpacked_vector.GetAsUint64Vector()));

  // bit_width 1
  raw_vector = {0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0};
  modulus = 2;
  SecAggVector vector2(raw_vector, modulus, GetParam());
  packed_bytes = vector2.GetAsPackedBytes();
  SecAggVector unpacked_vector2(packed_bytes, modulus, raw_vector.size(),
                                GetParam());
  EXPECT_THAT(raw_vector, Eq(unpacked_vector2.GetAsUint64Vector()));

  // bit_width lines up with byte boundary
  raw_vector = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  modulus = 1ULL << 16;
  SecAggVector vector3(raw_vector, modulus, GetParam());
  packed_bytes = vector3.GetAsPackedBytes();
  SecAggVector unpacked_vector3(packed_bytes, modulus, raw_vector.size(),
                                GetParam());
  EXPECT_THAT(raw_vector, Eq(unpacked_vector3.GetAsUint64Vector()));

  // bit_width one less than with byte boundary
  raw_vector = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  modulus = 1ULL << 15;
  SecAggVector vector4(raw_vector, modulus, GetParam());
  packed_bytes = vector4.GetAsPackedBytes();
  SecAggVector unpacked_vector4(packed_bytes, modulus, raw_vector.size(),
                                GetParam());
  EXPECT_THAT(raw_vector, Eq(unpacked_vector4.GetAsUint64Vector()));

  // bit_width one greater than byte boundary
  raw_vector = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  modulus = 1ULL << 17;
  SecAggVector vector5(raw_vector, modulus, GetParam());
  packed_bytes = vector5.GetAsPackedBytes();
  SecAggVector unpacked_vector5(packed_bytes, modulus, raw_vector.size(),
                                GetParam());
  EXPECT_THAT(raw_vector, Eq(unpacked_vector5.GetAsUint64Vector()));

  // bit_width relatively prime to byte boundary
  raw_vector.clear();
  raw_vector.resize(100, 1L);
  modulus = 1ULL << 19;
  SecAggVector vector6(raw_vector, modulus, GetParam());
  packed_bytes = vector6.GetAsPackedBytes();
  SecAggVector unpacked_vector6(packed_bytes, modulus, raw_vector.size(),
                                GetParam());
  EXPECT_THAT(raw_vector, Eq(unpacked_vector6.GetAsUint64Vector()));

  // max bit_width, where each array entry has its lowest bit set
  modulus = 1ULL << 62;
  SecAggVector vector7(raw_vector, modulus, GetParam());
  packed_bytes = vector7.GetAsPackedBytes();
  SecAggVector unpacked_vector7(packed_bytes, modulus, raw_vector.size(),
                                GetParam());
  EXPECT_THAT(raw_vector, Eq(unpacked_vector7.GetAsUint64Vector()));

  // max bit_width, where each array entry has its highest bit set
  uint64_t val = SecAggVector::kMaxModulus - 1;
  raw_vector.clear();
  raw_vector.resize(100, val);
  modulus = 1ULL << 62;
  SecAggVector vector8(raw_vector, modulus, GetParam());
  packed_bytes = vector8.GetAsPackedBytes();
  SecAggVector unpacked_vector8(packed_bytes, modulus, raw_vector.size(),
                                GetParam());
  EXPECT_THAT(raw_vector, Eq(unpacked_vector8.GetAsUint64Vector()));

  // small non power-of-2 modulus
  raw_vector = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  modulus = 11;
  SecAggVector vector9(raw_vector, modulus, GetParam());
  packed_bytes = vector9.GetAsPackedBytes();
  SecAggVector unpacked_vector9(packed_bytes, modulus, raw_vector.size(),
                                GetParam());
  EXPECT_THAT(raw_vector, Eq(unpacked_vector9.GetAsUint64Vector()));

  // large non power-of-2 modulus
  raw_vector = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 2636861836188};
  modulus = 2636861836189;
  SecAggVector vector10(raw_vector, modulus, GetParam());
  packed_bytes = vector10.GetAsPackedBytes();
  SecAggVector unpacked_vector10(packed_bytes, modulus, raw_vector.size(),
                                 GetParam());
  EXPECT_THAT(raw_vector, Eq(unpacked_vector10.GetAsUint64Vector()));
}

TEST_P(SecAggVectorTest, PackedVectorUnpacksToSameValuesExhaustive_PowerOf2) {
  for (auto i = 1; i < absl::bit_width(SecAggVector::kMaxModulus - 1); ++i) {
    for (auto j = 0; j < 1024; ++j) {
      for (auto val : {1ULL, 1ULL << (i - 1), (1ULL << (i - 1)) - 1,
                       i & ~((1ULL << i) - 1)}) {
        auto bit_width = i;
        uint64_t modulus = 1ULL << bit_width;
        std::vector<uint64_t> raw_vector(j, val);
        SecAggVector vector(raw_vector, modulus, GetParam());
        const auto& packed_bytes = vector.GetAsPackedBytes();
        SecAggVector unpacked_vector(packed_bytes, modulus, raw_vector.size(),
                                     GetParam());
        EXPECT_THAT(raw_vector, Eq(unpacked_vector.GetAsUint64Vector()));
      }
    }
  }
}

TEST_P(SecAggVectorTest, PackedVectorUnpacksToSameValuesExhaustive_Arbitrary) {
  for (auto modulus : kArbitraryModuli) {
    for (auto j = 0; j < 1024; ++j) {
      for (uint64_t val :
           {static_cast<uint64_t>(0UL), static_cast<uint64_t>(1UL),
            static_cast<uint64_t>((modulus >> 1) - 1),
            static_cast<uint64_t>(modulus >> 1),
            static_cast<uint64_t>((modulus >> 1) + 1),
            static_cast<uint64_t>(modulus - 1)}) {
        std::vector<uint64_t> raw_vector(j, val);
        SecAggVector vector(raw_vector, modulus, GetParam());
        const auto& packed_bytes = vector.GetAsPackedBytes();
        SecAggVector unpacked_vector(packed_bytes, modulus, raw_vector.size(),
                                     GetParam());
        EXPECT_THAT(raw_vector, Eq(unpacked_vector.GetAsUint64Vector()));
      }
    }
  }
}

TEST_P(SecAggVectorTest, VerifyPackingExample1) {
  std::vector<uint64_t> correct_unpacked = {1, 3, 7, 15};
  char correct_packed_array[] = {static_cast<char>(0b01100001),
                                 static_cast<char>(0b10011100),
                                 static_cast<char>(0b00000111)};
  std::string correct_packed(correct_packed_array, 3);
  uint64_t modulus = 32;

  SecAggVector from_unpacked_vector(correct_unpacked, modulus, GetParam());
  const std::string& packed_bytes = from_unpacked_vector.GetAsPackedBytes();
  EXPECT_THAT(correct_packed, Eq(packed_bytes));

  SecAggVector from_packed_vector(correct_packed, modulus,
                                  correct_unpacked.size(), GetParam());
  EXPECT_THAT(correct_unpacked, Eq(from_packed_vector.GetAsUint64Vector()));
}

TEST_P(SecAggVectorTest, VerifyPackingExample2) {
  std::vector<uint64_t> correct_unpacked = {13, 17, 19};
  char correct_packed_array[] = {
      static_cast<char>(0b00001101), static_cast<char>(0b00100010),
      static_cast<char>(0b01001100), static_cast<char>(0b00000000)};
  std::string correct_packed(correct_packed_array, 4);
  uint64_t modulus = 512;

  SecAggVector from_unpacked_vector(correct_unpacked, modulus, GetParam());
  const std::string& packed_bytes = from_unpacked_vector.GetAsPackedBytes();
  EXPECT_THAT(correct_packed, Eq(packed_bytes));

  SecAggVector from_packed_vector(correct_packed, modulus,
                                  correct_unpacked.size(), GetParam());
  EXPECT_THAT(correct_unpacked, Eq(from_packed_vector.GetAsUint64Vector()));
}

TEST_P(SecAggVectorTest, MoveConstructor) {
  std::vector<uint64_t> raw_vector = {0, 3};
  SecAggVector vector(raw_vector, 4, GetParam());
  SecAggVector other(std::move(vector));
  EXPECT_THAT(other.GetAsUint64Vector(), Eq(raw_vector));
}

TEST_P(SecAggVectorTest, MoveAssignment) {
  std::vector<uint64_t> raw_vector = {0, 3};
  SecAggVector vector(raw_vector, 4, GetParam());
  SecAggVector other = std::move(vector);
  EXPECT_THAT(other.GetAsUint64Vector(), Eq(raw_vector));
}

TEST_P(SecAggVectorTest, VerifyGetAsPackedBytesDiesAfterMoving) {
  std::vector<uint64_t> raw_vector = {0, 3};
  SecAggVector vector(raw_vector, 4, GetParam());
  SecAggVector other = std::move(vector);

  ASSERT_DEATH(auto i = vector.GetAsPackedBytes(),  // NOLINT
               "SecAggVector has no value");
}

TEST_P(SecAggVectorTest, VerifyGetAsUint64VectorDiesAfterMoving) {
  std::vector<uint64_t> raw_vector = {0, 3};
  SecAggVector vector(raw_vector, 4, GetParam());
  SecAggVector other = std::move(vector);

  ASSERT_DEATH(auto vec = vector.GetAsUint64Vector(),  // NOLINT
               "SecAggVector has no value");
}

TEST(SecAggVectorTest, VerifyTakePackedBytesDiesAfterMoving) {
  std::vector<uint64_t> raw_vector = {0, 3};
  SecAggVector vector(raw_vector, 4);
  SecAggVector other = std::move(vector);

  ASSERT_DEATH(auto i = std::move(vector).TakePackedBytes(),  // NOLINT
               "SecAggVector has no value");
}

INSTANTIATE_TEST_SUITE_P(Branchless, SecAggVectorTest, ::testing::Bool(),
                         ::testing::PrintToStringParamName());

TEST(SecAggUnpackedVectorTest, VerifyBasicOperations) {
  SecAggUnpackedVector vector(100, 32);
  EXPECT_THAT(vector.num_elements(), Eq(100));
  EXPECT_THAT(vector.modulus(), Eq(32));

  SecAggUnpackedVector vector2({1, 2, 3}, 32);
  EXPECT_THAT(vector2.num_elements(), Eq(3));
  EXPECT_THAT(vector2.modulus(), Eq(32));
  EXPECT_THAT(vector2.size(), Eq(3));
  EXPECT_THAT(vector2[1], Eq(2));
}

TEST(SecAggUnpackedVectorTest, VerifyMoveConstructor) {
  SecAggUnpackedVector vector({1, 2, 3}, 32);
  SecAggUnpackedVector vector2(std::move(vector));
  EXPECT_THAT(vector.modulus(), Eq(0));  // NOLINT
  EXPECT_THAT(vector2.num_elements(), Eq(3));
  EXPECT_THAT(vector2.modulus(), Eq(32));
  EXPECT_THAT(vector2[2], Eq(3));
}

TEST(SecAggUnpackedVectorTest, VerifyConstructorFromSecAggVector) {
  std::vector<uint64_t> raw_vector = {1, 2, 3};
  SecAggVector vector(raw_vector, 32);
  SecAggUnpackedVector vector2(vector);
  EXPECT_THAT(vector2.num_elements(), Eq(3));
  EXPECT_THAT(vector2.modulus(), Eq(32));
  EXPECT_THAT(vector2[2], Eq(3));
}

TEST(SecAggUnpackedVectorTest, VerifyMoveAssignment) {
  SecAggUnpackedVector vector({1, 2, 3}, 32);
  SecAggUnpackedVector vector2 = std::move(vector);
  EXPECT_THAT(vector.modulus(), Eq(0));  // NOLINT
  EXPECT_THAT(vector2.num_elements(), Eq(3));
  EXPECT_THAT(vector2.modulus(), Eq(32));
  EXPECT_THAT(vector2[0], Eq(1));
}

TEST(SecAggUnpackedVectorTest, AddSecAggVectorMapInPlace) {
  SecAggUnpackedVectorMap unpacked_map;
  unpacked_map.emplace("foobar", SecAggUnpackedVector({0, 10, 20, 30}, 32));

  SecAggVectorMap packed_map;
  packed_map.emplace("foobar", SecAggVector({5, 5, 5, 5}, 32));

  unpacked_map.Add(packed_map);
  EXPECT_THAT(unpacked_map.size(), Eq(1));
  EXPECT_THAT(unpacked_map.at("foobar"), ElementsAreArray({5, 15, 25, 3}));
}

TEST(SecAggUnpackedVectorTest, AddUnpackedSecAggVectorMaps) {
  SecAggUnpackedVectorMap unpacked_map_1, unpacked_map_2;
  unpacked_map_1.emplace("foobar", SecAggUnpackedVector({0, 10, 20, 30}, 32));
  unpacked_map_2.emplace("foobar", SecAggUnpackedVector({5, 5, 5, 5}, 32));

  auto result =
      SecAggUnpackedVectorMap::AddMaps(unpacked_map_1, unpacked_map_2);
  EXPECT_THAT(result->size(), Eq(1));
  EXPECT_THAT(result->at("foobar"), ElementsAreArray({5, 15, 25, 3}));
}

TEST(SecAggUnpackedVectorTest, AddUnpackedSecAggVectorMapInPlace) {
  SecAggUnpackedVectorMap unpacked_map_1, unpacked_map_2;
  unpacked_map_1.emplace("foobar", SecAggUnpackedVector({0, 10, 20, 30}, 32));
  unpacked_map_2.emplace("foobar", SecAggUnpackedVector({5, 5, 5, 5}, 32));

  unpacked_map_1.Add(unpacked_map_2);
  EXPECT_THAT(unpacked_map_1.size(), Eq(1));
  EXPECT_THAT(unpacked_map_1.at("foobar"), ElementsAreArray({5, 15, 25, 3}));
}

}  // namespace
}  // namespace secagg
}  // namespace fcp
