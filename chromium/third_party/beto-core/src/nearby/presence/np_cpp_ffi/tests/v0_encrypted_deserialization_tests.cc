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
#include <span>
#include <string>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "nearby_protocol.h"
#include "np_cpp_ffi_types.h"
#include "np_cpp_test.h"
#include "shared_test_util.h"

// NOLINTBEGIN(readability-magic-numbers)
TEST_F(NpCppTest, V0PrivateIdentityDeserializationSimpleCase) {
  auto slab_result = nearby_protocol::CredentialSlab::TryCreate();
  ASSERT_TRUE(slab_result.ok());

  const std::span<uint8_t> metadata_span(V0AdvEncryptedMetadata);
  const nearby_protocol::MatchedCredentialData match_data(123, metadata_span);

  std::array<uint8_t, 32> key_seed = {};
  std::fill_n(key_seed.begin(), 32, 0x11);

  const nearby_protocol::V0MatchableCredential v0_cred(
      key_seed, V0AdvLegacyMetadataKeyHmac, match_data);

  auto add_result = slab_result->AddV0Credential(v0_cred);
  ASSERT_EQ(add_result, absl::OkStatus());

  auto book_result =
      nearby_protocol::CredentialBook::TryCreateFromSlab(*slab_result);
  ASSERT_TRUE(book_result.ok());

  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(
          V0AdvEncryptedPayload, *book_result);
  ASSERT_EQ(deserialize_result.GetKind(),
            nearby_protocol::DeserializeAdvertisementResultKind::V0);

  auto v0_adv = deserialize_result.IntoV0();
  auto kind = v0_adv.GetKind();
  ASSERT_EQ(kind, nearby_protocol::DeserializedV0AdvertisementKind::Legible);

  auto legible_adv = v0_adv.IntoLegible();
  auto identity_kind = legible_adv.GetIdentityKind();
  ASSERT_EQ(identity_kind,
            nearby_protocol::DeserializedV0IdentityKind::Decrypted);
  ASSERT_EQ(legible_adv.GetNumberOfDataElements(), 1);

  auto payload = legible_adv.IntoPayload();
  auto de = payload.TryGetDataElement(0);
  ASSERT_TRUE(de.ok());

  auto metadata = payload.DecryptMetadata();
  ASSERT_TRUE(metadata.ok());
  ASSERT_EQ(ExpectedV0DecryptedMetadata,
            std::string(metadata->begin(), metadata->end()));

  auto identity_details = payload.GetIdentityDetails();
  ASSERT_TRUE(identity_details.ok());
  ASSERT_EQ(identity_details->cred_id, 123);
  ASSERT_EQ(identity_details->identity_type,
            nearby_protocol::EncryptedIdentityType::Private);

  auto de_type = de->GetKind();
  ASSERT_EQ(de_type, nearby_protocol::V0DataElementKind::TxPower);

  auto tx_power_de = de->AsTxPower();
  ASSERT_EQ(tx_power_de.GetAsI8(), 3);
}

nearby_protocol::CredentialBook CreateEmptyCredBook() {
  auto slab = nearby_protocol::CredentialSlab::TryCreate().value();
  auto book = nearby_protocol::CredentialBook::TryCreateFromSlab(slab).value();
  return book;
}

TEST_F(NpCppTest, V0PrivateIdentityEmptyBook) {
  auto book = CreateEmptyCredBook();
  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(
          V0AdvEncryptedPayload, book);
  ASSERT_EQ(deserialize_result.GetKind(),
            nearby_protocol::DeserializeAdvertisementResultKind::V0);

  auto v0_adv = deserialize_result.IntoV0();
  ASSERT_EQ(
      v0_adv.GetKind(),
      nearby_protocol::DeserializedV0AdvertisementKind::NoMatchingCredentials);

  // Should not be able to actually access contents
  ASSERT_DEATH([[maybe_unused]] auto failure = v0_adv.IntoLegible(), "");
}

TEST_F(NpCppTest, V0PrivateIdentityNoMatchingCreds) {
  auto slab_result = nearby_protocol::CredentialSlab::TryCreate();
  ASSERT_TRUE(slab_result.ok());

  uint8_t metadata[] = {0};
  const std::span<uint8_t> metadata_span(metadata);
  const nearby_protocol::MatchedCredentialData match_data(123, metadata_span);

  // A randomly picked key seed, does NOT match what was used for the canned adv
  std::array<uint8_t, 32> key_seed = {};
  std::fill_n(key_seed.begin(), 31, 0x11);

  const nearby_protocol::V0MatchableCredential v0_cred(
      key_seed, V0AdvLegacyMetadataKeyHmac, match_data);

  auto add_result = slab_result->AddV0Credential(v0_cred);
  ASSERT_EQ(add_result, absl::OkStatus());

  auto book_result =
      nearby_protocol::CredentialBook::TryCreateFromSlab(*slab_result);
  ASSERT_TRUE(book_result.ok());

  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(
          V0AdvEncryptedPayload, *book_result);
  ASSERT_EQ(deserialize_result.GetKind(),
            nearby_protocol::DeserializeAdvertisementResultKind::V0);

  auto v0_adv = deserialize_result.IntoV0();
  ASSERT_EQ(
      v0_adv.GetKind(),
      nearby_protocol::DeserializedV0AdvertisementKind::NoMatchingCredentials);

  // Should not be able to actually access contents
  ASSERT_DEATH([[maybe_unused]] auto failure = v0_adv.IntoLegible(), "");
}

// Make sure the correct credential is matched out of multiple provided
TEST_F(NpCppTest, V0PrivateIdentityMultipleCredentials) {
  auto slab = nearby_protocol::CredentialSlab::TryCreate().value();
  const std::span<uint8_t> metadata_span(V0AdvEncryptedMetadata);
  std::array<uint8_t, 32> key_seed = {};
  // Non matching credential
  const nearby_protocol::MatchedCredentialData match_data(123, metadata_span);
  std::fill_n(key_seed.begin(), 32, 0x12);
  const nearby_protocol::V0MatchableCredential v0_cred(
      key_seed, V0AdvLegacyMetadataKeyHmac, match_data);
  ASSERT_TRUE(slab.AddV0Credential(v0_cred).ok());

  // Matching credential
  const nearby_protocol::MatchedCredentialData match_data2(456, metadata_span);
  std::fill_n(key_seed.begin(), 32, 0x11);
  const nearby_protocol::V0MatchableCredential v0_cred2(
      key_seed, V0AdvLegacyMetadataKeyHmac, match_data2);
  ASSERT_TRUE(slab.AddV0Credential(v0_cred2).ok());

  // Non matching credential
  const nearby_protocol::MatchedCredentialData match_data3(789, metadata_span);
  std::fill_n(key_seed.begin(), 32, 0x13);
  const nearby_protocol::V0MatchableCredential v0_cred3(
      key_seed, V0AdvLegacyMetadataKeyHmac, match_data3);
  ASSERT_TRUE(slab.AddV0Credential(v0_cred3).ok());

  auto book = nearby_protocol::CredentialBook::TryCreateFromSlab(slab).value();
  auto legible_adv = nearby_protocol::Deserializer::DeserializeAdvertisement(
                         V0AdvEncryptedPayload, book)
                         .IntoV0()
                         .IntoLegible();
  ASSERT_EQ(legible_adv.GetIdentityKind(),
            nearby_protocol::DeserializedV0IdentityKind::Decrypted);
  ASSERT_EQ(legible_adv.GetNumberOfDataElements(), 1);

  auto payload = legible_adv.IntoPayload();
  ASSERT_TRUE(payload.TryGetDataElement(0).ok());

  // Make sure the correct credential matches
  auto identity_details = payload.GetIdentityDetails();
  ASSERT_TRUE(identity_details.ok());
  ASSERT_EQ(identity_details->cred_id, 456);
  ASSERT_EQ(identity_details->identity_type,
            nearby_protocol::EncryptedIdentityType::Private);
}
// NOLINTEND(readability-magic-numbers)
