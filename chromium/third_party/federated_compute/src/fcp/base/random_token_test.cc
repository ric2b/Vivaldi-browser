/*
 * Copyright 2019 Google LLC
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

#include "fcp/base/random_token.h"

#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash_testing.h"

namespace fcp {

using ::testing::Eq;

TEST(RandomTokenTest, Equality) {
  RandomToken a1 = RandomToken::Generate();
  RandomToken a2 = a1;
  RandomToken b = RandomToken::Generate();

  EXPECT_TRUE(a1 == a2);
  EXPECT_FALSE(a1 != a2);

  EXPECT_TRUE(b != a1);
  EXPECT_FALSE(b == a1);
}

TEST(RandomTokenTest, Hashing) {
  std::vector<RandomToken> distinct;
  for (int i = 0; i < 128; i++) {
    distinct.push_back(RandomToken::Generate());
  }

  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(distinct));
}

TEST(RandomTokenTest, Collisions) {
  // If this test ever fails, then we've tragically over-estimated the quality
  // of our random source.
  absl::flat_hash_set<RandomToken> tokens;
  for (int i = 0; i < 1024; i++) {
    RandomToken t = RandomToken::Generate();
    bool inserted = tokens.insert(t).second;
    EXPECT_TRUE(inserted);
  }
}

TEST(RandomTokenTest, Serialization) {
  RandomToken original = RandomToken::Generate();
  auto bytes = original.ToBytes();
  RandomToken deserialized = RandomToken::FromBytes(bytes);
  EXPECT_THAT(deserialized, Eq(original));
}

TEST(RandomTokenTest, SerializationToString) {
  RandomToken original = RandomToken::Generate();
  std::string str = original.ToString();
  RandomToken deserialized = RandomToken::FromBytes(str);
  EXPECT_THAT(deserialized, Eq(original));
}

TEST(RandomTokenTest, ToPrintableString) {
  constexpr char const* kHex = "000102030405060708090a0b0c0d0e0f";
  std::array<char, kRandomTokenSizeInBytes> kBytes{
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  EXPECT_THAT(RandomToken::FromBytes(kBytes).ToPrintableString(), Eq(kHex));
}

}  // namespace fcp
