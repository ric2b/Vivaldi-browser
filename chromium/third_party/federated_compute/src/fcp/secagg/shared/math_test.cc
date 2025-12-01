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

#include "fcp/secagg/shared/math.h"

#include <cstdint>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace fcp {
namespace secagg {
namespace {

using ::testing::Eq;

TEST(MathTest, DivideRoundUpIsAccurate) {
  EXPECT_THAT(DivideRoundUp(0, 8), Eq(0));
  EXPECT_THAT(DivideRoundUp(1, 8), Eq(1));
  EXPECT_THAT(DivideRoundUp(8, 8), Eq(1));
  EXPECT_THAT(DivideRoundUp(12, 8), Eq(2));
  EXPECT_THAT(DivideRoundUp(31, 8), Eq(4));
  EXPECT_THAT(DivideRoundUp(32, 8), Eq(4));
  EXPECT_THAT(DivideRoundUp(33, 8), Eq(5));
}

TEST(MathTest, AddModIsAccurate) {
  // power-of-2 moduli
  EXPECT_THAT(AddMod(2, 5, 8), Eq(7));
  EXPECT_THAT(AddMod(4, 5, 8), Eq(1));
  EXPECT_THAT(AddMod(0, 5, 8), Eq(5));
  EXPECT_THAT(AddMod(5, 0, 8), Eq(5));
  EXPECT_THAT(AddMod(7, 7, 8), Eq(6));
  EXPECT_THAT(AddMod(9223372036854775806ULL, 9223372036854775807ULL,
                     9223372036854775808ULL),
              Eq(9223372036854775805ULL));

  // non-power-of-2 moduli
  EXPECT_THAT(AddMod(2, 5, 7), Eq(0));
  EXPECT_THAT(AddMod(4, 5, 7), Eq(2));
  EXPECT_THAT(AddMod(0, 5, 7), Eq(5));
  EXPECT_THAT(AddMod(5, 0, 7), Eq(5));
  EXPECT_THAT(AddMod(7, 7, 7), Eq(0));
  EXPECT_THAT(AddMod(9223372036854775805ULL, 9223372036854775806ULL,
                     9223372036854775807ULL),
              Eq(9223372036854775804ULL));
}

TEST(MathTest, AddModOptIsAccurate) {
  // power-of-2 moduli
  EXPECT_THAT(AddModOpt(2, 5, 8), Eq(7));
  EXPECT_THAT(AddModOpt(4, 5, 8), Eq(1));
  EXPECT_THAT(AddModOpt(0, 5, 8), Eq(5));
  EXPECT_THAT(AddModOpt(5, 0, 8), Eq(5));
  EXPECT_THAT(AddModOpt(7, 7, 8), Eq(6));
  EXPECT_THAT(AddModOpt(9223372036854775806ULL, 9223372036854775807ULL,
                        9223372036854775808ULL),
              Eq(9223372036854775805ULL));

  // non-power-of-2 moduli
  EXPECT_THAT(AddModOpt(2, 5, 7), Eq(0));
  EXPECT_THAT(AddModOpt(4, 5, 7), Eq(2));
  EXPECT_THAT(AddModOpt(0, 5, 7), Eq(5));
  EXPECT_THAT(AddModOpt(5, 0, 7), Eq(5));
  EXPECT_THAT(AddModOpt(6, 6, 7), Eq(5));
  EXPECT_THAT(AddModOpt(9223372036854775805ULL, 9223372036854775806ULL,
                        9223372036854775807ULL),
              Eq(9223372036854775804ULL));
}

TEST(MathTest, SubtractModWorksAndHandlesUnderflow) {
  EXPECT_THAT(SubtractMod(3, 4, 10), Eq(9));
  EXPECT_THAT(SubtractMod(2, 9, 10), Eq(3));
  EXPECT_THAT(SubtractMod(0, 6, 10), Eq(4));
  EXPECT_THAT(SubtractMod(0, 5, 10), Eq(5));
  EXPECT_THAT(SubtractMod(7, 3, 10), Eq(4));
  EXPECT_THAT(SubtractMod(9, 0, 10), Eq(9));
  EXPECT_THAT(SubtractMod(0, 0, 10), Eq(0));
  EXPECT_THAT(SubtractMod(7, 7, 10), Eq(0));
  EXPECT_THAT(SubtractMod(9223372036854775807ULL, 0, 9223372036854775808ULL),
              Eq(9223372036854775807ULL));
  EXPECT_THAT(SubtractMod(0, 9223372036854775807ULL, 9223372036854775808ULL),
              Eq(1));
  EXPECT_THAT(SubtractMod(9223372036854775805ULL, 9223372036854775807ULL,
                          9223372036854775808ULL),
              Eq(9223372036854775806ULL));

  EXPECT_THAT(SubtractMod(9223372036854775806ULL, 0, 9223372036854775807ULL),
              Eq(9223372036854775806ULL));
  EXPECT_THAT(SubtractMod(0, 9223372036854775806ULL, 9223372036854775807ULL),
              Eq(1));
  EXPECT_THAT(SubtractMod(9223372036854775805ULL, 9223372036854775806ULL,
                          9223372036854775807ULL),
              Eq(9223372036854775806ULL));
}

TEST(MathTest, SubtractModOptWorksAndHandlesUnderflow) {
  EXPECT_THAT(SubtractModOpt(3, 4, 10), Eq(9));
  EXPECT_THAT(SubtractModOpt(2, 9, 10), Eq(3));
  EXPECT_THAT(SubtractModOpt(0, 6, 10), Eq(4));
  EXPECT_THAT(SubtractModOpt(0, 5, 10), Eq(5));
  EXPECT_THAT(SubtractModOpt(7, 3, 10), Eq(4));
  EXPECT_THAT(SubtractModOpt(9, 0, 10), Eq(9));
  EXPECT_THAT(SubtractModOpt(0, 0, 10), Eq(0));
  EXPECT_THAT(SubtractModOpt(7, 7, 10), Eq(0));
  EXPECT_THAT(SubtractModOpt(9223372036854775807ULL, 0, 9223372036854775808ULL),
              Eq(9223372036854775807ULL));
  EXPECT_THAT(SubtractModOpt(0, 9223372036854775807ULL, 9223372036854775808ULL),
              Eq(1));
  EXPECT_THAT(SubtractModOpt(9223372036854775805ULL, 9223372036854775807ULL,
                             9223372036854775808ULL),
              Eq(9223372036854775806ULL));

  EXPECT_THAT(SubtractModOpt(9223372036854775806ULL, 0, 9223372036854775807ULL),
              Eq(9223372036854775806ULL));
  EXPECT_THAT(SubtractModOpt(0, 9223372036854775806ULL, 9223372036854775807ULL),
              Eq(1));
  EXPECT_THAT(SubtractModOpt(9223372036854775805ULL, 9223372036854775806ULL,
                             9223372036854775807ULL),
              Eq(9223372036854775806ULL));
}

TEST(MathTest, MultiplyModAvoidsOverflow) {
  uint64_t p = 2147483659ULL;  // 2 ^ 31 + 11; a prime number
  uint32_t a = 2147483646;     // 2 ^ 31 - 2; -13 mod p
  uint32_t b = 2147483640;     // 2 ^ 31 - 8; -19 mod p
  uint32_t res1 = 169;         // -13 * -13
  uint32_t res2 = 247;         // -13 * -19
  uint32_t res3 = 361;         // -19 * -19
  EXPECT_THAT(MultiplyMod(a, a, p), Eq(res1));
  EXPECT_THAT(MultiplyMod(a, b, p), Eq(res2));
  EXPECT_THAT(MultiplyMod(b, a, p), Eq(res2));
  EXPECT_THAT(MultiplyMod(b, b, p), Eq(res3));
}

TEST(MathTest, MultiplyMod64AvoidsOverflow) {
  {
    uint64_t p = 2147483659ULL;  // 2 ^ 31 + 11; a prime number
    uint32_t a = 2147483646;     // 2 ^ 31 - 2; -13 mod p
    uint32_t b = 2147483640;     // 2 ^ 31 - 8; -19 mod p
    uint32_t res1 = 169;         // -13 * -13
    uint32_t res2 = 247;         // -13 * -19
    uint32_t res3 = 361;         // -19 * -19
    EXPECT_THAT(MultiplyMod64(a, a, p), Eq(res1));
    EXPECT_THAT(MultiplyMod64(a, b, p), Eq(res2));
    EXPECT_THAT(MultiplyMod64(b, a, p), Eq(res2));
    EXPECT_THAT(MultiplyMod64(b, b, p), Eq(res3));
  }

  {
    uint64_t p = 4503599627371499ULL;     // 2 ^ 52 + 1003; a prime number
    uint64_t a = 1099511627776ULL;        // 2 ^ 40
    uint64_t b = 36028797018963971ULL;    // 2 ^ 55 + 3
    uint64_t res1 = 4503330386609131ULL;  // a * a
    uint64_t res2 = 188016488351702ULL;   // a * b
    uint64_t res3 = 64336441ULL;          // b * b
    EXPECT_THAT(MultiplyMod64(a, a, p), Eq(res1));
    EXPECT_THAT(MultiplyMod64(a, b, p), Eq(res2));
    EXPECT_THAT(MultiplyMod64(b, a, p), Eq(res2));
    EXPECT_THAT(MultiplyMod64(b, b, p), Eq(res3));
  }
}
TEST(MathTest, InverseModPrimeIsAccurate) {
  // All mods assumed to be prime
  EXPECT_THAT(InverseModPrime(12, 31), Eq(13));
  EXPECT_THAT(InverseModPrime(13, 31), Eq(12));
  EXPECT_THAT(InverseModPrime(13, 2147483659ULL), Eq(1651910507));
  EXPECT_THAT(InverseModPrime(2147483646, 2147483659ULL), Eq(495573152));
}

TEST(MathTest, IntToByteStringProvidesBigEndianString) {
  uint32_t big_low_bits = 0x01234567;
  uint32_t big_high_bits = 0xFEDCBA98;
  uint32_t max_val = 0xFFFFFFFF;
  uint32_t min_val = 0x00000000;
  uint8_t expected0[4] = {0x1, 0x23, 0x45, 0x67};
  EXPECT_THAT(IntToByteString(big_low_bits),
              Eq(std::string(reinterpret_cast<char*>(expected0), 4)));
  uint8_t expected1[4] = {0xFE, 0xDC, 0xBA, 0x98};
  EXPECT_THAT(IntToByteString(big_high_bits),
              Eq(std::string(reinterpret_cast<char*>(expected1), 4)));
  uint8_t expected2[4] = {0xFF, 0xFF, 0xFF, 0xFF};
  EXPECT_THAT(IntToByteString(max_val),
              Eq(std::string(reinterpret_cast<char*>(expected2), 4)));
  uint8_t expected3[4] = {0x0, 0x0, 0x0, 0x0};
  EXPECT_THAT(IntToByteString(min_val),
              Eq(std::string(reinterpret_cast<char*>(expected3), 4)));
}

}  // namespace
}  // namespace secagg
}  // namespace fcp
