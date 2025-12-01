/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "fcp/client/rust/oak_attestation_verification_ffi.h"

#include <cstdint>
#include <filesystem>  // NOLINT(build/c++17)
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "absl/time/time.h"
#include "fcp/client/test_helpers.h"
#include "fcp/testing/testing.h"
#include "proto/attestation/endorsement.pb.h"
#include "proto/attestation/evidence.pb.h"
#include "proto/attestation/reference_value.pb.h"
#include "proto/attestation/verification.pb.h"
#include "proto/digest.pb.h"
#include "proto/validity.pb.h"
#include "util/time/protoutil.h"

namespace fcp::client::rust::oak_attestation_verification_ffi {
namespace {

using ::oak::attestation::v1::Endorsement;
using ::oak::attestation::v1::EndorsementDetails;
using ::oak::attestation::v1::EndorsementReferenceValue;
using ::oak::attestation::v1::KeyType;
using ::oak::attestation::v1::Signature;
using ::oak::attestation::v1::SignedEndorsement;
using ::oak::attestation::v1::VerifyingKey;
using ::oak::attestation::v1::VerifyingKeyReferenceValue;
using ::oak::attestation::v1::VerifyingKeySet;

constexpr absl::string_view kPemHeader = "-----BEGIN PUBLIC KEY-----";
constexpr absl::string_view kPemFooter = "-----END PUBLIC KEY-----";
constexpr absl::string_view kTestDataDir =
    "fcp/client/rust/testdata";

// Pretend the test runs at 1 Jan 2025, 00:00 UTC.
constexpr int64_t kNowUtcMillis = 1735686000000;

constexpr uint32_t kFakeKeyId = 1776;

std::string GetFileContents(absl::string_view filename) {
  std::filesystem::path full_path =
      std::filesystem::path(testing::SrcDir()) / kTestDataDir / filename;
  std::string contents;
  CHECK(LoadFileAsString(full_path, &contents));
  return contents;
}

std::string ConvertPemToRaw(absl::string_view public_key_pem) {
  absl::string_view stripped = absl::StripAsciiWhitespace(public_key_pem);
  stripped = absl::StripPrefix(stripped, kPemHeader);
  stripped = absl::StripSuffix(stripped, kPemFooter);

  std::string decoded;
  CHECK(absl::Base64Unescape(stripped, &decoded));
  return decoded;
}

TEST(OakAttestationVerificationFfiTest, VerifyEndorsementFailure) {
  // Error message is: "no endorsement in signed endorsement", as expected.
  EXPECT_THAT(VerifyEndorsement(absl::UniversalEpoch(), {}, {}),
              IsCode(absl::StatusCode::kFailedPrecondition));
}

TEST(OakAttestationVerificationFfiTest, VerifyEndorsementSuccess) {
  Endorsement endorsement;
  endorsement.set_format(Endorsement::ENDORSEMENT_FORMAT_JSON_INTOTO);
  endorsement.set_serialized(GetFileContents("endorsement.json"));

  Signature signature;
  signature.set_key_id(kFakeKeyId);
  signature.set_raw(GetFileContents("endorsement.json.sig"));

  SignedEndorsement signed_endorsement;
  *signed_endorsement.mutable_endorsement() = endorsement;
  *signed_endorsement.mutable_signature() = signature;
  signed_endorsement.set_rekor_log_entry(GetFileContents("logentry.json"));

  VerifyingKey endorser_key;
  endorser_key.set_type(KeyType::KEY_TYPE_ECDSA_P256_SHA256);
  endorser_key.set_key_id(kFakeKeyId);
  endorser_key.set_raw(
      ConvertPemToRaw(GetFileContents("endorser_public_key.pem")));

  VerifyingKeySet endorser_key_set;
  *endorser_key_set.add_keys() = endorser_key;

  VerifyingKey rekor_key;
  rekor_key.set_type(KeyType::KEY_TYPE_ECDSA_P256_SHA256);
  rekor_key.set_raw(ConvertPemToRaw(GetFileContents("rekor_public_key.pem")));

  VerifyingKeySet rekor_key_set;
  *rekor_key_set.add_keys() = rekor_key;

  VerifyingKeyReferenceValue rekor_reference_value;
  *rekor_reference_value.mutable_verify() = rekor_key_set;

  EndorsementReferenceValue ref_value;
  *ref_value.mutable_endorser() = endorser_key_set;
  // Needs review: Required claims must be confirmed as empty by touching it.
  // Leaving them unset one will not be able to get past the verification.
  ref_value.mutable_required_claims();
  *ref_value.mutable_rekor() = rekor_reference_value;

  auto s = VerifyEndorsement(absl::FromUnixMillis(kNowUtcMillis),
                             signed_endorsement, ref_value);
  EXPECT_OK(s);

  EndorsementDetails details = s.value();
  EXPECT_EQ(absl::BytesToHexString(details.subject_digest().sha2_256()),
            "09ab204446287049060482d9f60fb684ff43a5b53acf317a32ac4ebfa6a5ef95");
  ASSERT_OK_AND_ASSIGN(
      absl::Time not_before,
      util_time::DecodeGoogleApiProto(details.valid().not_before()));
  ASSERT_OK_AND_ASSIGN(absl::Time not_after, util_time::DecodeGoogleApiProto(
                                                 details.valid().not_after()));
  EXPECT_EQ(absl::ToUnixMillis(not_before), 1728410765246);
  EXPECT_EQ(absl::ToUnixMillis(not_after), 1759946765246);
}

}  // namespace
}  // namespace fcp::client::rust::oak_attestation_verification_ffi
