// Copyright 2023 Google LLC
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

#include <algorithm>
#include <array>
#include <cstdint>

#include "gtest/gtest.h"
#include "nearby_protocol.h"
#include "np_cpp_ffi_types.h"
#include "np_cpp_test.h"
#include "shared_test_util.h"

// NOLINTBEGIN(readability-magic-numbers)

// Corresponds to V0PrivateIdentityDeserializationSimpleCase,
// which in turn corresponds to np_adv's examples_v0
TEST_F(NpCppTest, V0PrivateIdentitySerializationSimpleCase) {
  std::array<uint8_t, 32> key_seed = {};
  std::fill_n(key_seed.begin(), 32, 0x11);

  std::array<uint8_t, 14> metadata_key = {};
  std::fill_n(metadata_key.begin(), 14, 0x33);

  auto broadcast_cred =
      nearby_protocol::V0BroadcastCredential(key_seed, metadata_key);

  std::array<uint8_t, 2> salt = {};
  std::fill_n(salt.begin(), 2, 0x22);

  auto adv_builder = nearby_protocol::V0AdvertisementBuilder::CreateEncrypted(
      broadcast_cred, salt);

  auto tx_power = nearby_protocol::TxPower::TryBuildFromI8(3).value();
  auto de = nearby_protocol::V0DataElement(tx_power);

  ASSERT_TRUE(adv_builder.TryAddDE(de).ok());

  auto serialized_bytes = adv_builder.TrySerialize().value();
  auto actual = serialized_bytes.ToVector();

  auto expected = V0AdvEncryptedBuffer.ToVector();

  ASSERT_EQ(actual, expected);
}

// NOLINTEND(readability-magic-numbers)
