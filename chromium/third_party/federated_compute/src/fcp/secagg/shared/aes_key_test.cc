// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "fcp/secagg/shared/aes_key.h"

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "fcp/secagg/shared/math.h"
#include "fcp/secagg/shared/shamir_secret_sharing.h"

namespace fcp {
namespace secagg {
namespace {

using ::testing::Eq;

// For testing purposes, make an AesKey out of a string.
AesKey AesKeyFromString(const std::string& key) {
  return AesKey(reinterpret_cast<const uint8_t*>(key.c_str()),
                static_cast<int>(key.size()));
}

// Suppose the randomly chosen key can be expressed in bit_length <= 128 bits.
// Java did not express the key in 128 bits, but rather will have used
// (bit_length + 1) bits. The extra bit is the highest-order bit, and is a sign
// bit, guaranteed to be 0. The next highest-order bit is guaranteed to be 1.
std::string JavaStyleKey(int bit_length) {
  EXPECT_TRUE(0 < bit_length && bit_length <= 128);
  std::string key = "16 byte test key";
  int byte_with_sign_bit = (127 - bit_length) / 8;
  int pos_of_sign_bit = (127 - bit_length) % 8;
  if (bit_length == 128) {
    pos_of_sign_bit = 7;
    key = absl::StrCat("\0", key);
  } else {
    key.erase(0, byte_with_sign_bit);
  }
  // Make sure the high-order bit is the sign bit 0, and the next highest-order
  // bit is 1.
  key[0] = static_cast<char>(127 >> pos_of_sign_bit);
  if (pos_of_sign_bit == 7) {
    key[1] = static_cast<char>(255);
  }
  return key;
}

TEST(AesKeyTest, CreateFromSharesHandles32BKeys) {
  AesKey original_key = AesKeyFromString("32 byte AES key for testing only");
  ShamirSecretSharing shamir;
  auto shares = shamir.Share(5, 7, original_key);
  auto key_or_error = AesKey::CreateFromShares(shares, 5);
  EXPECT_THAT(key_or_error.ok(), Eq(true));
  EXPECT_THAT(key_or_error.value(), Eq(original_key));
}

TEST(AesKeyTest, CreateFromSharesHandlesShortKeys) {
  ShamirSecretSharing shamir;
  for (int i = 1; i <= 128; ++i) {
    std::string original_key_string = JavaStyleKey(i);
    AesKey original_key = AesKeyFromString(original_key_string);
    std::string key_string_for_sharing;
    if (original_key_string.size() < 16) {
      key_string_for_sharing =
          absl::StrCat(std::string(16 - original_key_string.size(), '\0'),
                       original_key_string);
    } else if (original_key_string.size() == 17) {
      key_string_for_sharing = original_key_string.substr(1);
    } else {
      key_string_for_sharing = original_key_string;
    }
    auto shares = shamir.Share(5, 7, AesKeyFromString(key_string_for_sharing));
    auto key_or_error = AesKey::CreateFromShares(shares, 5);
    EXPECT_THAT(key_or_error.ok(), Eq(true));
    EXPECT_THAT(key_or_error.value(), Eq(original_key))
        << i << " bit key fails";
  }
}

}  // namespace
}  // namespace secagg
}  // namespace fcp
