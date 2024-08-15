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

#include <cstdint>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "nearby_protocol.h"
#include "np_cpp_ffi_types.h"
#include "np_cpp_test.h"

// NOLINTBEGIN(readability-magic-numbers)

TEST_F(NpCppTest, TxPowerMustBeInRange) {
  auto out_of_range_result = nearby_protocol::TxPower::TryBuildFromI8(100);
  ASSERT_FALSE(out_of_range_result.ok());
}

TEST_F(NpCppTest, ContextSyncSeqNumMustBeInRange) {
  // Check that out-of-range fails
  auto out_of_range_result =
      nearby_protocol::ContextSyncSeqNum::TryBuildFromU8(17);
  ASSERT_FALSE(out_of_range_result.ok());

  // Check that if it's in range, we can get a context sync seq num and set it
  // appropriately within an actions field.
  auto actions = nearby_protocol::V0Actions::BuildNewZeroed(
      nearby_protocol::AdvertisementBuilderKind::Public);
  auto seq_num = nearby_protocol::ContextSyncSeqNum::TryBuildFromU8(15).value();
  actions.SetContextSyncSequenceNumber(seq_num);
  ASSERT_EQ(actions.GetContextSyncSequenceNumber().GetAsU8(),
            seq_num.GetAsU8());
}

TEST_F(NpCppTest, V0UnencryptedActionFlavorMustMatch) {
  auto actions = nearby_protocol::V0Actions::BuildNewZeroed(
      nearby_protocol::AdvertisementBuilderKind::Public);

  // Try to set an encrypted-only action.
  auto mismatch_result = actions.TrySetAction(
      nearby_protocol::BooleanActionType::InstantTethering, true);
  ASSERT_FALSE(mismatch_result.ok());
  // Verify that nothing changed about the actions.
  ASSERT_EQ(actions.GetAsU32(), 0);

  // Try again, but with a plaintext-compatible action.
  auto success_result = actions.TrySetAction(
      nearby_protocol::BooleanActionType::NearbyShare, true);
  ASSERT_TRUE(success_result.ok());
  ASSERT_TRUE(
      actions.HasAction(nearby_protocol::BooleanActionType::NearbyShare));
}

// Corresponds to V0DeserializeSingleDataElementTxPower
TEST_F(NpCppTest, V0SerializeSingleDataElementTxPower) {
  auto adv_builder =
      nearby_protocol::V0AdvertisementBuilder::TryCreatePublic().value();

  auto tx_power = nearby_protocol::TxPower::TryBuildFromI8(3).value();
  auto de = nearby_protocol::V0DataElement(tx_power);

  auto add_de_result = adv_builder.TryAddDE(de);
  ASSERT_TRUE(add_de_result.ok());

  auto serialized_bytes = adv_builder.TrySerialize().value();
  auto actual = serialized_bytes.ToVector();

  const std::vector<uint8_t> expected{
      0x00,       // Adv Header
      0x03,       // Public DE header
      0x15, 0x03  // Length 1 Tx Power DE with value 3
  };
  ASSERT_EQ(actual, expected);
}

// Corresponds to V0DeserializeLengthOneActionsDataElement
TEST_F(NpCppTest, V0SerializeLengthOneActionsDataElement) {
  auto adv_builder =
      nearby_protocol::V0AdvertisementBuilder::TryCreatePublic().value();
  auto actions = nearby_protocol::V0Actions::BuildNewZeroed(
      nearby_protocol::AdvertisementBuilderKind::Public);
  auto de = nearby_protocol::V0DataElement(actions);

  auto add_de_result = adv_builder.TryAddDE(de);
  ASSERT_TRUE(add_de_result.ok());

  auto serialized_bytes = adv_builder.TrySerialize().value();
  auto actual = serialized_bytes.ToVector();

  const std::vector<uint8_t> expected{
      0x00,       // Adv Header
      0x03,       // Public DE header
      0x16, 0x00  // Length 1 Actions DE
  };

  ASSERT_EQ(actual, expected);
}

// Corresponds to V0DeserializeLengthTwoActionsDataElement
TEST_F(NpCppTest, V0SerializeLengthTwoActionsDataElement) {
  auto adv_builder =
      nearby_protocol::V0AdvertisementBuilder::TryCreatePublic().value();
  auto actions = nearby_protocol::V0Actions::BuildNewZeroed(
      nearby_protocol::AdvertisementBuilderKind::Public);

  ASSERT_TRUE(
      actions
          .TrySetAction(nearby_protocol::BooleanActionType::NearbyShare, true)
          .ok());
  ASSERT_TRUE(
      actions.TrySetAction(nearby_protocol::BooleanActionType::Finder, true)
          .ok());
  ASSERT_TRUE(
      actions
          .TrySetAction(nearby_protocol::BooleanActionType::FastPairSass, true)
          .ok());
  auto seq_num =
      nearby_protocol::ContextSyncSeqNum::TryBuildFromU8(0xD).value();
  actions.SetContextSyncSequenceNumber(seq_num);

  auto de = nearby_protocol::V0DataElement(actions);

  auto add_de_result = adv_builder.TryAddDE(de);
  ASSERT_TRUE(add_de_result.ok());

  auto serialized_bytes = adv_builder.TrySerialize().value();
  auto actual = serialized_bytes.ToVector();

  const std::vector<uint8_t> expected{
      0x00,             // Adv Header
      0x03,             // Public DE header
      0x26, 0xD0, 0x46  // Length 2 Actions DE
  };

  ASSERT_EQ(actual, expected);
}
// Corresponds to V0DeserializeMultipleDataElements
TEST_F(NpCppTest, V0SerializeMultipleDataElements) {
  auto adv_builder =
      nearby_protocol::V0AdvertisementBuilder::TryCreatePublic().value();

  auto tx_power = nearby_protocol::TxPower::TryBuildFromI8(5).value();
  auto tx_power_de = nearby_protocol::V0DataElement(tx_power);
  auto add_tx_power_de_result = adv_builder.TryAddDE(tx_power_de);
  ASSERT_TRUE(add_tx_power_de_result.ok());

  auto actions = nearby_protocol::V0Actions::BuildNewZeroed(
      nearby_protocol::AdvertisementBuilderKind::Public);

  ASSERT_TRUE(
      actions
          .TrySetAction(nearby_protocol::BooleanActionType::NearbyShare, true)
          .ok());
  ASSERT_TRUE(
      actions.TrySetAction(nearby_protocol::BooleanActionType::Finder, true)
          .ok());
  ASSERT_TRUE(
      actions
          .TrySetAction(nearby_protocol::BooleanActionType::FastPairSass, true)
          .ok());

  auto actions_de = nearby_protocol::V0DataElement(actions);

  auto add_actions_de_result = adv_builder.TryAddDE(actions_de);
  ASSERT_TRUE(add_actions_de_result.ok());

  auto serialized_bytes = adv_builder.TrySerialize().value();
  auto actual = serialized_bytes.ToVector();

  const std::vector<uint8_t> expected{
      0x00,              // Adv Header
      0x03,              // Public DE header
      0x15, 0x05,        // Tx Power value 5
      0x26, 0x00, 0x46,  // Length 2 Actions
  };

  ASSERT_EQ(actual, expected);
}

// TODO: Reinstate this test.
// TEST_F(NpCppTest, V0SerializeEmptyPayload) {
//  auto adv_builder =
//  nearby_protocol::V0AdvertisementBuilder::TryCreatePublic().value(); auto
//  serialize_result = adv_builder.TrySerialize();
//  ASSERT_FALSE(serialize_result.ok());
//}

TEST_F(NpCppTest, TestV0AdvBuilderMoveConstructor) {
  auto adv_builder =
      nearby_protocol::V0AdvertisementBuilder::TryCreatePublic().value();
  // Move it, and ensure it's still valid.
  nearby_protocol::V0AdvertisementBuilder moved_adv_builder(
      std::move(adv_builder));

  auto actions = nearby_protocol::V0Actions::BuildNewZeroed(
      nearby_protocol::AdvertisementBuilderKind::Public);
  auto actions_de = nearby_protocol::V0DataElement(actions);
  ASSERT_TRUE(moved_adv_builder.TryAddDE(actions_de).ok());

  // Trying to use the moved-out-of-object should trigger an abort
  ASSERT_DEATH([[maybe_unused]] auto failure =
                   adv_builder.TryAddDE(  // NOLINT(bugprone-use-after-move)
                       actions_de),
               "");

  // Moving again should still preserve the moved state and lead to an abort
  nearby_protocol::V0AdvertisementBuilder moved_again(std::move(adv_builder));
  ASSERT_DEATH([[maybe_unused]] auto failure = moved_again.TryAddDE(
                   actions_de)  // NOLINT(bugprone-use-after-move)
               ,
               "");
}

TEST_F(NpCppTest, TestV0AdvBuilderDestructor) {
  {
    // Get us up to the limit on the number of adv builders
    auto adv_builder_one =
        nearby_protocol::V0AdvertisementBuilder::TryCreatePublic().value();
    auto adv_builder_two =
        nearby_protocol::V0AdvertisementBuilder::TryCreatePublic().value();
    // Assert that we're at the limit
    ASSERT_FALSE(
        nearby_protocol::V0AdvertisementBuilder::TryCreatePublic().ok());
    // Destructors should run.
  }
  // The space from the adv builders should've been reclaimed now.
  ASSERT_TRUE(nearby_protocol::V0AdvertisementBuilder::TryCreatePublic().ok());
}

// NOLINTEND(readability-magic-numbers)
