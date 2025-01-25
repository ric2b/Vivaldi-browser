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

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "nearby_protocol.h"
#include "np_cpp_ffi_types.h"
#include "np_cpp_test.h"
#include "shared_test_util.h"

TEST_F(NpCppTest, V1SimpleTestCase) {
  nearby_protocol::CredentialSlab credential_slab;
  nearby_protocol::CredentialBook credential_book(credential_slab);

  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(V1AdvPlaintext,
                                                              credential_book);
  ASSERT_EQ(deserialize_result.GetKind(),
            nearby_protocol::DeserializeAdvertisementResultKind::V1);

  auto v1_adv = deserialize_result.IntoV1();
  ASSERT_EQ(v1_adv.GetNumLegibleSections(), 1);
  ASSERT_EQ(v1_adv.GetNumUndecryptableSections(), 0);

  auto invalid = v1_adv.TryGetSection(1);
  ASSERT_FALSE(invalid.ok());
  ASSERT_TRUE(absl::IsOutOfRange(invalid.status()));

  auto section = v1_adv.TryGetSection(0);
  ASSERT_TRUE(section.ok());
  ASSERT_EQ(section.value().GetIdentityKind(),
            nearby_protocol::DeserializedV1IdentityKind::Plaintext);
  ASSERT_EQ(section.value().NumberOfDataElements(), 1);

  auto invalid_de = section.value().TryGetDataElement(1);
  ASSERT_FALSE(invalid_de.ok());
  ASSERT_TRUE(absl::IsOutOfRange(invalid_de.status()));

  auto de = section.value().TryGetDataElement(0);
  ASSERT_TRUE(de.ok());
  ASSERT_EQ(de.value().GetDataElementTypeCode(), (uint8_t)5);

  auto payload = de.value().GetPayload();
  auto vec = payload.ToVector();
  const std::vector<uint8_t> expected{3};
  ASSERT_EQ(vec, expected);
}

TEST_F(NpCppTest, TestV1AdvMoveConstructor) {
  nearby_protocol::CredentialSlab slab;
  nearby_protocol::CredentialBook book(slab);
  auto result = nearby_protocol::Deserializer::DeserializeAdvertisement(
      V1AdvPlaintext, book);
  ASSERT_EQ(result.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V1);
  auto adv = result.IntoV1();

  // Now move the adv into a new value, and make sure its still valid
  const nearby_protocol::DeserializedV1Advertisement moved_adv(std::move(adv));
  ASSERT_EQ(moved_adv.GetNumLegibleSections(), 1);

  // trying to use the moved object should result in a use after free which
  // triggers an abort
  ASSERT_DEATH(
      [[maybe_unused]] auto failure =
          adv.GetNumLegibleSections(),  // NOLINT(bugprone-use-after-move)
      "");
  ASSERT_DEATH(
      [[maybe_unused]] auto failure = adv.GetNumUndecryptableSections(), "");
  ASSERT_DEATH([[maybe_unused]] auto failure = adv.TryGetSection(0), "");

  // moving again should still preserve the moved state and also lead to an
  // abort
  const nearby_protocol::DeserializedV1Advertisement moved_again(
      std::move(adv));
  ASSERT_DEATH(
      [[maybe_unused]] auto failure = moved_again.GetNumLegibleSections(), "");
  ASSERT_DEATH(
      [[maybe_unused]] auto failure = moved_again.GetNumUndecryptableSections(),
      "");
  ASSERT_DEATH([[maybe_unused]] auto failure = moved_again.TryGetSection(0),
               "");
}

TEST_F(NpCppTest, TestV1AdvMoveAssignment) {
  nearby_protocol::CredentialSlab slab;
  nearby_protocol::CredentialBook book(slab);
  auto result = nearby_protocol::Deserializer::DeserializeAdvertisement(
      V1AdvPlaintext, book);
  ASSERT_EQ(result.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V1);
  auto adv = result.IntoV1();

  // create a second result
  auto another_result = nearby_protocol::Deserializer::DeserializeAdvertisement(
      V1AdvPlaintext, book);
  ASSERT_EQ(another_result.GetKind(),
            np_ffi::internal::DeserializeAdvertisementResultKind::V1);
  auto adv2 = another_result.IntoV1();

  // move adv2 into adv, the original should be deallocated by assignment
  adv2 = std::move(adv);
  ASSERT_EQ(adv2.GetNumLegibleSections(), 1);

  // original result should now be invalid, using it will trigger a use after
  // free abort.
  ASSERT_DEATH(
      [[maybe_unused]] auto failure =
          adv.GetNumLegibleSections(),  // NOLINT(bugprone-use-after-move)
      "");
  ASSERT_DEATH(
      [[maybe_unused]] auto failure = adv.GetNumUndecryptableSections(), "");
  ASSERT_DEATH([[maybe_unused]] auto failure = adv.TryGetSection(0), "");

  // moving again should still lead to an error
  auto moved_again = std::move(adv);
  ASSERT_DEATH(
      [[maybe_unused]] auto failure = moved_again.GetNumLegibleSections(), "");
  ASSERT_DEATH(
      [[maybe_unused]] auto failure = moved_again.GetNumUndecryptableSections(),
      "");
  ASSERT_DEATH([[maybe_unused]] auto failure = moved_again.TryGetSection(0),
               "");
}

nearby_protocol::DeserializedV1Section GetSection(
    nearby_protocol::CredentialBook &book) {
  // Create the adv in this scope, so its de-allocated at the end of this call.
  // The section should still be valid
  auto v1_adv = nearby_protocol::Deserializer::DeserializeAdvertisement(
                    V1AdvPlaintext, book)
                    .IntoV1();
  auto section = v1_adv.TryGetSection(0);
  return section.value();
}

bool TryDeserializeNewV1Adv(nearby_protocol::CredentialBook &book) {
  auto adv = nearby_protocol::Deserializer::DeserializeAdvertisement(
      V1AdvPlaintext, book);
  return adv.GetKind() ==
         np_ffi::internal::DeserializeAdvertisementResultKind::V1;
}

TEST_F(NpCppTest, TestSectionOwnership) {
  nearby_protocol::CredentialSlab slab;
  nearby_protocol::CredentialBook book(slab);
  {
    auto section = GetSection(book);
    ASSERT_EQ(section.GetIdentityKind(),
              nearby_protocol::DeserializedV1IdentityKind::Plaintext);
    ASSERT_EQ(section.NumberOfDataElements(), 1);
    ASSERT_TRUE(section.TryGetDataElement(0).ok());

    auto section2 = GetSection(book);
    ASSERT_EQ(section2.GetIdentityKind(),
              nearby_protocol::DeserializedV1IdentityKind::Plaintext);
    ASSERT_EQ(section2.NumberOfDataElements(), 1);
    ASSERT_TRUE(section2.TryGetDataElement(0).ok());

    auto allocations =
        nearby_protocol::GlobalConfig::GetCurrentHandleAllocationCount();
    ASSERT_EQ(allocations.legible_v1_sections, 2U);
  }

  // now that the section has gone out of scope the allocation should be
  // released
  auto allocations =
      nearby_protocol::GlobalConfig::GetCurrentHandleAllocationCount();
  ASSERT_EQ(allocations.legible_v1_sections, 0U);
}

/*
 * Multiple sections are not supported in plaintext advertisements
 * TODO Update the below test to use encrypted sections
TEST(NpCppTest, V1MultipleSections) {
  auto credential_book = nearby_protocol::CredentialBook::Create();
  ASSERT_TRUE(credential_book.ok());

  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(
          V1AdvMultipleSections, credential_book);
  ASSERT_EQ(deserialize_result.GetKind(),
            nearby_protocol::DeserializeAdvertisementResultKind::V1);

  auto v1_adv = deserialize_result.IntoV1();
  ASSERT_EQ(v1_adv.GetNumLegibleSections(), 2);
  ASSERT_EQ(v1_adv.GetNumUndecryptableSections(), 0);

  auto invalid = v1_adv.TryGetSection(2);
  ASSERT_FALSE(invalid.ok());
  ASSERT_TRUE(absl::IsOutOfRange(invalid.status()));

  auto section = v1_adv.TryGetSection(0);
  ASSERT_TRUE(section.ok());
  ASSERT_EQ(section.value().GetIdentityKind(),
            nearby_protocol::DeserializedV1IdentityKind::Plaintext);
  ASSERT_EQ(section.value().NumberOfDataElements(), 1);

  auto invalid_de = section.value().TryGetDataElement(1);
  ASSERT_FALSE(invalid_de.ok());
  ASSERT_TRUE(absl::IsOutOfRange(invalid_de.status()));

  auto de = section.value().TryGetDataElement(0);
  ASSERT_TRUE(de.ok());
  ASSERT_EQ(de.value().GetDataElementTypeCode(), 6);

  auto payload = de.value().GetPayload();
  auto vec = payload.ToVector();
  std::vector<uint8_t> expected{0x00, 0x46};
  ASSERT_EQ(vec, expected);

  auto section2 = v1_adv.TryGetSection(1);
  ASSERT_TRUE(section2.ok());
  ASSERT_EQ(section2.value().GetIdentityKind(),
            nearby_protocol::DeserializedV1IdentityKind::Plaintext);
  ASSERT_EQ(section2.value().NumberOfDataElements(), 1);
}
*/
