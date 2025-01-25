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
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "nearby_protocol.h"
#include "np_cpp_ffi_types.h"
#include "np_cpp_test.h"
#include "shared_test_util.h"

TEST_F(NpCppTest, V1PrivateIdentitySimpleCase) {
  nearby_protocol::CredentialSlab slab;
  const std::span<uint8_t> metadata_span(V1AdvEncryptedMetadata);
  const nearby_protocol::MatchedCredentialData match_data(123, metadata_span);
  const nearby_protocol::V1MatchableCredential v1_cred(
      V1AdvKeySeed, V1AdvExpectedMicExtendedSaltIdentityTokenHmac,
      V1AdvExpectedSignatureIdentityTokenHmac, V1AdvPublicKey, match_data);

  auto add_result = slab.AddV1Credential(v1_cred);
  ASSERT_EQ(add_result, absl::OkStatus());

  nearby_protocol::CredentialBook book(slab);
  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(V1AdvEncrypted,
                                                              book);
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

  auto metadata = section->TryDecryptMetadata();
  ASSERT_TRUE(metadata.ok());
  ASSERT_EQ(ExpectedV1DecryptedMetadata,
            std::string(metadata->begin(), metadata->end()));

  auto identity_details = section->GetIdentityDetails();
  ASSERT_TRUE(identity_details.ok());
  ASSERT_EQ(identity_details->cred_id, 123);
  ASSERT_EQ(identity_details->verification_mode,
            nearby_protocol::V1VerificationMode::Signature);

  auto de = section->TryGetDataElement(0);
  ASSERT_TRUE(de.ok());
  ASSERT_EQ(de->GetDataElementTypeCode(), 5U);
  ASSERT_EQ(de->GetPayload().ToVector(), std::vector<uint8_t>{7});

  auto offset = de->GetOffset();
  auto derived_salt = section->DeriveSaltForOffset(offset);
  ASSERT_TRUE(derived_salt.ok());
  const std::array<uint8_t, 16> expected = {0xD5, 0x63, 0x47, 0x39, 0x77, 0x84,
                                            0x38, 0xF2, 0x91, 0xBC, 0x24, 0x21,
                                            0xAD, 0x80, 0x88, 0x16};
  ASSERT_EQ(*derived_salt, expected);
}
