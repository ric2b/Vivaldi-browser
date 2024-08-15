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

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "nearby_protocol.h"
#include "np_cpp_ffi_types.h"
#include "np_cpp_test.h"
#include "shared_test_util.h"

TEST_F(NpCppTest, V1PrivateIdentitySimpleCase) {
  auto slab_result = nearby_protocol::CredentialSlab::TryCreate();
  ASSERT_TRUE(slab_result.ok());

  const std::span<uint8_t> metadata_span(V1AdvEncryptedMetadata);
  const nearby_protocol::MatchedCredentialData match_data(123, metadata_span);

  const nearby_protocol::V1MatchableCredential v1_cred(
      V1AdvKeySeed, V1AdvExpectedUnsignedMetadataKeyHmac,
      V1AdvExpectedSignedMetadataKeyHmac, V1AdvPublicKey, match_data);

  auto add_result = slab_result->AddV1Credential(v1_cred);
  ASSERT_EQ(add_result, absl::OkStatus());

  auto book_result =
      nearby_protocol::CredentialBook::TryCreateFromSlab(*slab_result);
  ASSERT_TRUE(book_result.ok());

  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(V1AdvEncrypted,
                                                              *book_result);
  ASSERT_EQ(deserialize_result.GetKind(),
            nearby_protocol::DeserializeAdvertisementResultKind::V1);

  auto v1_adv = deserialize_result.IntoV1();
  ASSERT_EQ(v1_adv.GetNumUndecryptableSections(), 0);
  ASSERT_EQ(v1_adv.GetNumLegibleSections(), 1);

  auto section = v1_adv.TryGetSection(0);
  ASSERT_TRUE(section.ok());
  ASSERT_EQ(section->GetIdentityKind(),
            nearby_protocol::DeserializedV1IdentityKind::Decrypted);
  ASSERT_EQ(section->NumberOfDataElements(), 1);

  auto metadata = section->DecryptMetadata();
  ASSERT_TRUE(metadata.ok());
  ASSERT_EQ(ExpectedV1DecryptedMetadata,
            std::string(metadata->begin(), metadata->end()));

  auto identity_details = section->GetIdentityDetails();
  ASSERT_TRUE(identity_details.ok());
  ASSERT_EQ(identity_details->cred_id, 123);
  ASSERT_EQ(identity_details->verification_mode,
            nearby_protocol::V1VerificationMode::Signature);
  ASSERT_EQ(identity_details->identity_type,
            nearby_protocol::EncryptedIdentityType::Private);

  auto de = section->TryGetDataElement(0);
  ASSERT_TRUE(de.ok());
  ASSERT_EQ(de->GetDataElementTypeCode(), 5);
  ASSERT_EQ(de->GetPayload().ToVector(), std::vector<uint8_t>{7});

  auto offset = de->GetOffset();
  auto derived_salt = section->DeriveSaltForOffset(offset);
  ASSERT_TRUE(derived_salt.ok());
  const std::array<uint8_t, 16> expected = {
      94, 154, 245, 152, 164, 22, 131, 157, 8, 79, 28, 77, 236, 57, 17, 97};
  ASSERT_EQ(*derived_salt, expected);
}