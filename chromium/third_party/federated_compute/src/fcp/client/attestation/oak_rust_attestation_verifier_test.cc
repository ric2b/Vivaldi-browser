#include "fcp/client/attestation/oak_rust_attestation_verifier.h"

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "fcp/base/digest.h"
#include "fcp/client/attestation/log_attestation_records.h"
#include "fcp/client/attestation/test_values.h"
#include "fcp/client/rust/oak_attestation_verification_ffi.h"
#include "fcp/confidentialcompute/cose.h"
#include "fcp/confidentialcompute/crypto.h"
#include "fcp/protos/confidentialcompute/access_policy.pb.h"
#include "fcp/protos/confidentialcompute/access_policy_endorsement_options.pb.h"
#include "fcp/protos/confidentialcompute/access_policy_endorsement_options.pb.h"
#include "fcp/protos/confidentialcompute/verification_record.pb.h"
#include "fcp/protos/federatedcompute/confidential_aggregations.pb.h"
#include "fcp/testing/testing.h"
#include "proto/attestation/endorsement.pb.h"
#include "proto/attestation/evidence.pb.h"
#include "proto/attestation/reference_value.pb.h"
#include "proto/attestation/verification.pb.h"
#include "proto/digest.pb.h"

namespace fcp::client::attestation {
namespace {
using ::fcp::client::attestation::test_values::GetKnownValidEncryptionConfig;
using ::fcp::client::attestation::test_values::GetKnownValidReferenceValues;
using ::fcp::client::attestation::test_values::GetSkipAllReferenceValues;
using ::fcp::confidential_compute::OkpCwt;
using ::google::internal::federatedcompute::v1::ConfidentialEncryptionConfig;
using ::oak::attestation::v1::ReferenceValues;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;

confidentialcompute::SignedEndorsements GetFakeSignedEndorsements(
    int number_of_endorsements) {
  confidentialcompute::SignedEndorsements signed_endorsements;
  for (int i = 1; i < number_of_endorsements + 1; ++i) {
    auto signed_endorsement = signed_endorsements.add_signed_endorsement();
    signed_endorsement->mutable_endorsement()->set_serialized(
        "{\"payload\":{\"subject\":{\"name\":\"signed endorsement\"}}}");
    signed_endorsement->mutable_signature()->set_key_id(i);
    signed_endorsement->mutable_signature()->set_raw("public key");
  }
  return signed_endorsements;
}

confidentialcompute::AccessPolicyEndorsementOptions
GetFakeAccessPolicyEndorsementOptions(int number_of_rvs) {
  confidentialcompute::AccessPolicyEndorsementOptions endorsement_options;
  for (int i = 1; i < number_of_rvs + 1; ++i) {
    auto endorsement_reference_value =
        endorsement_options.add_endorsement_reference_values();
    *endorsement_reference_value->mutable_required_claims()->add_claim_types() =
        "fake claim type";
  }
  return endorsement_options;
}

// Tests the case where default values are given to the verifier. Verification
// should not succeed in this case, since the Rust Oak Attestation Verification
// library will complain that required values are missing.
//
// This validates that we at least correctly call into the Rust Oak Attestation
// Verification library and get a result back, but it doesn't actually test
// the actual verification logic.
TEST(OakRustAttestationTest, DefaultValuesDoNotVerifySuccessfully) {
  // Generate a new public key, which we'll pass to the client in the
  // ConfidentialEncryptionConfig. We'll use the decryptor from which the public
  // key was generated to validate the encrypted payload at the end of the test.
  fcp::confidential_compute::MessageDecryptor decryptor;
  auto encoded_public_key =
      decryptor
          .GetPublicKey(
              [](absl::string_view payload) { return "fakesignature"; }, 0)
          .value();
  absl::StatusOr<OkpCwt> parsed_public_key = OkpCwt::Decode(encoded_public_key);
  ASSERT_OK(parsed_public_key);
  ASSERT_TRUE(parsed_public_key->public_key.has_value());

  // Note: we don't specify any attestation evidence nor attestation
  // endorsements in the encryption config, since we can't generate valid
  // attestations in a test anyway.
  ConfidentialEncryptionConfig encryption_config;
  encryption_config.set_public_key(encoded_public_key);
  // Populate an empty Evidence proto.
  encryption_config.mutable_attestation_evidence();

  // Use an empty ReferenceValues input.
  OakRustAttestationVerifier verifier(
      ReferenceValues(), ReferenceValues(), {},
      confidentialcompute::AccessPolicyEndorsementOptions(),
      LogPrettyPrintedVerificationRecord);

  // The verification should fail, since neither reference values nor the
  // evidence are valid.
  auto result =
      verifier.Verify(absl::Cord(""), confidentialcompute::SignedEndorsements(),
                      encryption_config);
  EXPECT_THAT(result.status(), IsCode(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(result.status().message(),
              HasSubstr("Attestation verification failed"));
}

TEST(OakRustAttestationTest,
     KnownValidEncryptionConfigAndValidPolicyInAllowlist) {
  ConfidentialEncryptionConfig encryption_config =
      GetKnownValidEncryptionConfig();
  ReferenceValues reference_values = GetKnownValidReferenceValues();

  // Create a valid access policy proto with some non-default content.
  confidentialcompute::DataAccessPolicy access_policy = PARSE_TEXT_PROTO(R"pb(
    transforms {
      src: 0
      application { tag: "foo" }
    }
  )pb");
  auto access_policy_bytes = access_policy.SerializeAsString();
  // This verifier will only accept attestation evidence matching the reference
  // values defined above, and will only accept the given access policy.
  OakRustAttestationVerifier verifier(
      reference_values, ReferenceValues(),
      {absl::BytesToHexString(ComputeSHA256(access_policy_bytes))},
      confidentialcompute::AccessPolicyEndorsementOptions(),
      LogPrettyPrintedVerificationRecord);

  // Ensure that the verification succeeds.
  auto result = verifier.Verify(absl::Cord(access_policy_bytes),
                                confidentialcompute::SignedEndorsements(),
                                encryption_config);
  ASSERT_OK(result);
  EXPECT_EQ(result->serialized_public_key, encryption_config.public_key());
  EXPECT_THAT(result->key_id, Not(IsEmpty()));
  EXPECT_EQ(result->access_policy_sha256, ComputeSHA256(access_policy_bytes));
}

TEST(OakRustAttestationTest, KnownValidEncryptionConfigAndMismatchingPolicy) {
  ConfidentialEncryptionConfig encryption_config =
      GetKnownValidEncryptionConfig();
  ReferenceValues reference_values = GetKnownValidReferenceValues();

  // Create a valid access policy proto with some non-default content.
  confidentialcompute::DataAccessPolicy disallowed_access_policy =
      PARSE_TEXT_PROTO(R"pb(
        transforms {
          src: 0
          application { tag: "bar" }
        }
      )pb");
  auto disallowed_access_policy_bytes =
      disallowed_access_policy.SerializeAsString();
  // This verifier will not accept any inputs, since the policy allowlist
  // doesn't match the actual policy.
  OakRustAttestationVerifier verifier(
      reference_values, ReferenceValues(), {"mismatching policy hash"},
      confidentialcompute::AccessPolicyEndorsementOptions(),
      LogPrettyPrintedVerificationRecord);

  // Ensure that the verification *does not* succeed.
  auto result = verifier.Verify(absl::Cord(disallowed_access_policy_bytes),
                                confidentialcompute::SignedEndorsements(),
                                encryption_config);
  EXPECT_THAT(result.status(), IsCode(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(result.status().message(),
              HasSubstr("Data access policy not in allowlist"));
}

TEST(OakRustAttestationTest,
     KnownValidEncryptionConfigAndAndValidPolicyWithEmptyPolicyAllowlist) {
  ConfidentialEncryptionConfig encryption_config =
      GetKnownValidEncryptionConfig();
  ReferenceValues reference_values = GetKnownValidReferenceValues();

  // Create a valid access policy proto with some non-default content.
  confidentialcompute::DataAccessPolicy access_policy = PARSE_TEXT_PROTO(R"pb(
    transforms {
      src: 0
      application { tag: "foo" }
    }
  )pb");
  auto access_policy_bytes = access_policy.SerializeAsString();
  // This verifier will not accept any inputs, since the policy allowlist is
  // empty.
  OakRustAttestationVerifier verifier(
      reference_values, ReferenceValues(), {},
      confidentialcompute::AccessPolicyEndorsementOptions(),
      LogPrettyPrintedVerificationRecord);

  // Ensure that the verification *does not* succeed.
  auto result = verifier.Verify(absl::Cord(access_policy_bytes),
                                confidentialcompute::SignedEndorsements(),
                                encryption_config);
  EXPECT_THAT(result.status(), IsCode(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(result.status().message(),
              HasSubstr("Data access policy not in allowlist"));
}

TEST(OakRustAttestationTest,
     KnownValidEncryptionConfigAndAndEmptyPolicyWithEmptyPolicyAllowlist) {
  ConfidentialEncryptionConfig encryption_config =
      GetKnownValidEncryptionConfig();
  ReferenceValues reference_values = GetKnownValidReferenceValues();
  // This verifier will not accept any inputs, since the policy allowlist is
  // empty.
  OakRustAttestationVerifier verifier(
      reference_values, ReferenceValues(), {},
      confidentialcompute::AccessPolicyEndorsementOptions(),
      LogPrettyPrintedVerificationRecord);

  // Ensure that the verification *does not* succeed, since an empty access
  // policy string still has to match an allowlist entry.
  auto result =
      verifier.Verify(absl::Cord(""), confidentialcompute::SignedEndorsements(),
                      encryption_config);
  EXPECT_THAT(result.status(), IsCode(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(result.status().message(),
              HasSubstr("Data access policy not in allowlist"));
}

// Ensures that mismatching primary reference values and empty secondary
// reference values (which should also be considered 'non-matching') fails
// verification.
TEST(OakRustAttestationTest,
     KnownEncryptionConfigAndMismatchingPrimaryRvsAndEmptySecondaryRvs) {
  ConfidentialEncryptionConfig encryption_config =
      GetKnownValidEncryptionConfig();
  ReferenceValues mismatching_reference_values = GetKnownValidReferenceValues();
  // Mess with the application layer digest value to ensure it won't match the
  // values in the ConfidentialEncryptionConfig.
  (*mismatching_reference_values.mutable_oak_restricted_kernel()
        ->mutable_application_layer()
        ->mutable_binary()
        ->mutable_digests()
        ->mutable_digests(0)
        ->mutable_sha2_256())[0] += 1;

  // Create a valid access policy proto with some non-default content.
  confidentialcompute::DataAccessPolicy access_policy = PARSE_TEXT_PROTO(R"pb(
    transforms {
      src: 0
      application { tag: "bar" }
    }
  )pb");
  auto access_policy_bytes = access_policy.SerializeAsString();
  // This verifier will not accept the encryption config provided, due to the
  // mismatching digest (and the secondary reference values being empty).
  OakRustAttestationVerifier verifier(
      mismatching_reference_values, ReferenceValues(),
      {absl::BytesToHexString(ComputeSHA256(access_policy_bytes))},
      confidentialcompute::AccessPolicyEndorsementOptions(),
      LogPrettyPrintedVerificationRecord);

  // Ensure that the verification *does not* succeed.
  auto result = verifier.Verify(absl::Cord(access_policy_bytes),
                                confidentialcompute::SignedEndorsements(),
                                encryption_config);
  EXPECT_THAT(result.status(), IsCode(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(result.status().message(),
              HasSubstr("Attestation verification failed for both primary and "
                        "secondary reference values"));
}

// Ensures that mismatching primary reference values and empty secondary
// reference values (which should also be considered 'non-matching') fails
// verification.
TEST(OakRustAttestationTest,
     KnownEncryptionConfigAndEmptyPrimaryRvsAndMismatchingSecondaryRvs) {
  ConfidentialEncryptionConfig encryption_config =
      GetKnownValidEncryptionConfig();
  ReferenceValues mismatching_reference_values = GetKnownValidReferenceValues();
  // Mess with the application layer digest value to ensure it won't match the
  // values in the ConfidentialEncryptionConfig.
  (*mismatching_reference_values.mutable_oak_restricted_kernel()
        ->mutable_application_layer()
        ->mutable_binary()
        ->mutable_digests()
        ->mutable_digests(0)
        ->mutable_sha2_256())[0] += 1;

  // Create a valid access policy proto with some non-default content.
  confidentialcompute::DataAccessPolicy access_policy = PARSE_TEXT_PROTO(R"pb(
    transforms {
      src: 0
      application { tag: "bar" }
    }
  )pb");
  auto access_policy_bytes = access_policy.SerializeAsString();
  // This verifier will not accept the encryption config provided, due to the
  // mismatching digest (and the secondary reference values being empty).
  OakRustAttestationVerifier verifier(
      ReferenceValues(), mismatching_reference_values,
      {absl::BytesToHexString(ComputeSHA256(access_policy_bytes))},
      confidentialcompute::AccessPolicyEndorsementOptions(),
      LogPrettyPrintedVerificationRecord);

  // Ensure that the verification *does not* succeed.
  auto result = verifier.Verify(absl::Cord(access_policy_bytes),
                                confidentialcompute::SignedEndorsements(),
                                encryption_config);
  EXPECT_THAT(result.status(), IsCode(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(result.status().message(),
              HasSubstr("Attestation verification failed for both primary and "
                        "secondary reference values"));
}

// Ensures that an empty primary reference values (which should be considered
// 'non-matching') properly falls back to the matching secondary reference
// values.
TEST(OakRustAttestationTest,
     KnownValidEncryptionConfigEmptyPrimaryRvsAndMatchingSecondaryRvs) {
  ConfidentialEncryptionConfig encryption_config =
      GetKnownValidEncryptionConfig();
  ReferenceValues reference_values = GetKnownValidReferenceValues();

  // Create a valid access policy proto with some non-default content.
  confidentialcompute::DataAccessPolicy access_policy = PARSE_TEXT_PROTO(R"pb(
    transforms {
      src: 0
      application { tag: "foo" }
    }
  )pb");
  auto access_policy_bytes = access_policy.SerializeAsString();
  // This verifier will only accept attestation evidence matching the reference
  // values defined above, and will only accept the given access policy.
  OakRustAttestationVerifier verifier(
      ReferenceValues(), reference_values,
      {absl::BytesToHexString(ComputeSHA256(access_policy_bytes))},
      confidentialcompute::AccessPolicyEndorsementOptions(),
      LogPrettyPrintedVerificationRecord);

  // Ensure that the verification succeeds.
  auto result = verifier.Verify(absl::Cord(access_policy_bytes),
                                confidentialcompute::SignedEndorsements(),
                                encryption_config);
  ASSERT_OK(result);
}

// Ensures that a non-empty but mismatching primary reference values (which
// should be considered 'non-matching') properly falls back to the matching
// secondary reference values.
TEST(OakRustAttestationTest,
     KnownEncryptionConfigAndMismatchingPrimaryRvsButMatchingSecondaryRvs) {
  ConfidentialEncryptionConfig encryption_config =
      GetKnownValidEncryptionConfig();
  ReferenceValues mismatching_reference_values = GetKnownValidReferenceValues();
  // Mess with the application layer digest value to ensure it won't match the
  // values in the ConfidentialEncryptionConfig.
  (*mismatching_reference_values.mutable_oak_restricted_kernel()
        ->mutable_application_layer()
        ->mutable_binary()
        ->mutable_digests()
        ->mutable_digests(0)
        ->mutable_sha2_256())[0] += 1;

  // Create a valid access policy proto with some non-default content.
  confidentialcompute::DataAccessPolicy access_policy = PARSE_TEXT_PROTO(R"pb(
    transforms {
      src: 0
      application { tag: "bar" }
    }
  )pb");
  auto access_policy_bytes = access_policy.SerializeAsString();
  // This verifier will not accept the encryption config provided, due to the
  // mismatching digest.
  OakRustAttestationVerifier verifier(
      mismatching_reference_values, GetKnownValidReferenceValues(),
      {absl::BytesToHexString(ComputeSHA256(access_policy_bytes))},
      confidentialcompute::AccessPolicyEndorsementOptions(),
      LogPrettyPrintedVerificationRecord);

  // Ensure that the verification *does not* succeed.
  auto result = verifier.Verify(absl::Cord(access_policy_bytes),
                                confidentialcompute::SignedEndorsements(),
                                encryption_config);
  ASSERT_OK(result);
}

// Ensures that empty primary and secondary reference values (which should be
// considered 'non-matching') fails verification.
TEST(OakRustAttestationTest, KnownEncryptionConfigAndEmptyReferencevalues) {
  ConfidentialEncryptionConfig encryption_config =
      GetKnownValidEncryptionConfig();

  // Create a valid access policy proto with some non-default content.
  confidentialcompute::DataAccessPolicy access_policy = PARSE_TEXT_PROTO(R"pb(
    transforms {
      src: 0
      application { tag: "bar" }
    }
  )pb");
  auto access_policy_bytes = access_policy.SerializeAsString();
  // This verifier will not accept the encryption config provided, due to the
  // reference values being invalid (an empty, uninitialized proto).
  OakRustAttestationVerifier verifier(
      ReferenceValues(), ReferenceValues(),
      {absl::BytesToHexString(ComputeSHA256(access_policy_bytes))},
      confidentialcompute::AccessPolicyEndorsementOptions(),
      LogPrettyPrintedVerificationRecord);

  // Ensure that the verification *does not* succeed.
  auto result = verifier.Verify(absl::Cord(access_policy_bytes),
                                confidentialcompute::SignedEndorsements(),
                                encryption_config);
  EXPECT_THAT(result.status(), IsCode(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(result.status().message(),
              HasSubstr("Attestation verification failed"));
}

TEST(OakRustAttestationTest,
     HasOneSignedEndorsementButNoAccessPolicyEndorsementReferenceValues) {
  ConfidentialEncryptionConfig encryption_config =
      GetKnownValidEncryptionConfig();
  ReferenceValues reference_values = GetKnownValidReferenceValues();

  // Create a valid access policy proto with some non-default content.
  confidentialcompute::DataAccessPolicy access_policy = PARSE_TEXT_PROTO(R"pb(
    transforms {
      src: 0
      application { tag: "foo" }
    }
  )pb");
  auto access_policy_bytes = access_policy.SerializeAsString();
  // This verifier will not accept any inputs when using SignedEndorsements,
  // since there are no EndorsementReferenceValues for the access policy.
  OakRustAttestationVerifier verifier(
      reference_values, ReferenceValues(), {},
      confidentialcompute::AccessPolicyEndorsementOptions(),
      LogPrettyPrintedVerificationRecord);

  confidentialcompute::SignedEndorsements signed_endorsements =
      GetFakeSignedEndorsements(1);

  // Ensure that the verification *does not* succeed.
  auto result = verifier.Verify(absl::Cord(access_policy_bytes),
                                signed_endorsements, encryption_config);
  EXPECT_THAT(result.status(), IsCode(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(result.status().message(),
              HasSubstr("Only a single EndorsementReferenceValue is supported, "
                        "but 0 were provided."));
}
TEST(OakRustAttestationTest, MultipleSignedEndorsementsFails) {
  ConfidentialEncryptionConfig encryption_config =
      GetKnownValidEncryptionConfig();
  ReferenceValues reference_values = GetKnownValidReferenceValues();

  // Create a valid access policy proto with some non-default content.
  confidentialcompute::DataAccessPolicy access_policy = PARSE_TEXT_PROTO(R"pb(
    transforms {
      src: 0
      application { tag: "foo" }
    }
  )pb");
  auto access_policy_bytes = access_policy.SerializeAsString();
  // This verifier will not accept any inputs when using SignedEndorsements,
  // since there are no EndorsementReferenceValues for the access policy.
  OakRustAttestationVerifier verifier(
      reference_values, ReferenceValues(), {},
      confidentialcompute::AccessPolicyEndorsementOptions(),
      LogPrettyPrintedVerificationRecord);

  confidentialcompute::SignedEndorsements signed_endorsements =
      GetFakeSignedEndorsements(2);

  // Ensure that the verification *does not* succeed.
  auto result = verifier.Verify(absl::Cord(access_policy_bytes),
                                signed_endorsements, encryption_config);
  EXPECT_THAT(result.status(), IsCode(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(result.status().message(),
              HasSubstr("Only a single SignedEndorsement is supported, but 2 "
                        "were provided."));
}

TEST(OakRustAttestationTest,
     MultipleAccessPolicyEndorsementReferenceValuesFails) {
  ConfidentialEncryptionConfig encryption_config =
      GetKnownValidEncryptionConfig();
  ReferenceValues reference_values = GetKnownValidReferenceValues();

  // Create a valid access policy proto with some non-default content.
  confidentialcompute::DataAccessPolicy access_policy = PARSE_TEXT_PROTO(R"pb(
    transforms {
      src: 0
      application { tag: "foo" }
    }
  )pb");
  auto access_policy_bytes = access_policy.SerializeAsString();
  confidentialcompute::AccessPolicyEndorsementOptions endorsement_options =
      GetFakeAccessPolicyEndorsementOptions(2);
  // This verifier will not accept any inputs when using SignedEndorsements,
  // since there are no EndorsementReferenceValues for the access policy.
  OakRustAttestationVerifier verifier(reference_values, ReferenceValues(), {},
                                      endorsement_options,
                                      LogPrettyPrintedVerificationRecord);

  confidentialcompute::SignedEndorsements signed_endorsements =
      GetFakeSignedEndorsements(1);

  // Ensure that the verification *does not* succeed.
  auto result = verifier.Verify(absl::Cord(access_policy_bytes),
                                signed_endorsements, encryption_config);
  EXPECT_THAT(result.status(), IsCode(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(result.status().message(),
              HasSubstr("Only a single EndorsementReferenceValue is supported, "
                        "but 2 were provided."));
}

TEST(OakRustAttestationTest, HasSignedEndorsementButNoEndorsementInside) {
  ConfidentialEncryptionConfig encryption_config =
      GetKnownValidEncryptionConfig();
  ReferenceValues reference_values = GetKnownValidReferenceValues();

  // Create a valid access policy proto with some non-default content.
  confidentialcompute::DataAccessPolicy access_policy = PARSE_TEXT_PROTO(R"pb(
    transforms {
      src: 0
      application { tag: "foo" }
    }
  )pb");
  auto access_policy_bytes = access_policy.SerializeAsString();
  // This verifier will not accept any inputs when using SignedEndorsements,
  // since there are no EndorsementReferenceValues for the access policy.
  OakRustAttestationVerifier verifier(
      reference_values, ReferenceValues(), {},
      confidentialcompute::AccessPolicyEndorsementOptions(),
      LogPrettyPrintedVerificationRecord);

  confidentialcompute::SignedEndorsements signed_endorsements;
  auto signed_endorsement = signed_endorsements.add_signed_endorsement();
  // Just a signature but no endorsement.
  signed_endorsement->mutable_signature()->set_key_id(1234);
  signed_endorsement->mutable_signature()->set_raw("public key");

  // Ensure that the verification *does not* succeed.
  auto result = verifier.Verify(absl::Cord(access_policy_bytes),
                                signed_endorsements, encryption_config);
  EXPECT_THAT(result.status(), IsCode(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(result.status().message(),
              HasSubstr("SignedEndorsement does not contain an endorsement."));
}

// Ensures that verification fails if the public key contains an
// access_policy_sha256 claim that doesn't match the access policy.
TEST(OakRustAttestationTest,
     KnownEncryptionConfigAndMismatchingAccessPolicyHashClaim) {
  ConfidentialEncryptionConfig encryption_config =
      GetKnownValidEncryptionConfig();
  absl::StatusOr<OkpCwt> parsed_key =
      OkpCwt::Decode(encryption_config.public_key());
  ASSERT_OK(parsed_key);
  parsed_key->access_policy_sha256 = "mismatching_access_policy_hash";
  encryption_config.set_public_key(parsed_key->Encode().value());

  // Create a valid access policy proto with some non-default content.
  confidentialcompute::DataAccessPolicy access_policy = PARSE_TEXT_PROTO(R"pb(
    transforms {
      src: 0
      application { tag: "bar" }
    }
  )pb");
  auto access_policy_bytes = access_policy.SerializeAsString();
  OakRustAttestationVerifier verifier(
      GetKnownValidReferenceValues(), ReferenceValues(),
      {absl::BytesToHexString(ComputeSHA256(access_policy_bytes))},
      confidentialcompute::AccessPolicyEndorsementOptions(),
      LogPrettyPrintedVerificationRecord);

  auto result = verifier.Verify(absl::Cord(access_policy_bytes),
                                confidentialcompute::SignedEndorsements(),
                                encryption_config);
  EXPECT_THAT(result.status(), IsCode(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(
      result.status().message(),
      HasSubstr(
          "access_policy_sha256 claim does not match the access policy."));
}

// Ensures that access_policy_sha256 claim verification succeeds if does match
// the access policy.
TEST(OakRustAttestationTest,
     KnownEncryptionConfigAndMatchingAccessPolicyHashClaim) {
  ConfidentialEncryptionConfig encryption_config =
      GetKnownValidEncryptionConfig();

  // Create a valid access policy proto with some non-default content.
  confidentialcompute::DataAccessPolicy access_policy = PARSE_TEXT_PROTO(R"pb(
    transforms {
      src: 0
      application { tag: "bar" }
    }
  )pb");
  auto access_policy_bytes = access_policy.SerializeAsString();

  // Add an access_policy_sha256 claim that matches the access policy.
  absl::StatusOr<OkpCwt> parsed_key =
      OkpCwt::Decode(encryption_config.public_key());
  ASSERT_OK(parsed_key);
  parsed_key->access_policy_sha256 = ComputeSHA256(access_policy_bytes);
  encryption_config.set_public_key(parsed_key->Encode().value());

  OakRustAttestationVerifier verifier(
      GetKnownValidReferenceValues(), ReferenceValues(),
      {absl::BytesToHexString(ComputeSHA256(access_policy_bytes))},
      confidentialcompute::AccessPolicyEndorsementOptions(),
      LogPrettyPrintedVerificationRecord);

  auto result = verifier.Verify(absl::Cord(access_policy_bytes),
                                confidentialcompute::SignedEndorsements(),
                                encryption_config);
  // The access_policy_sha256 step should succeed, but since we modified the
  // public key, the signature verification will fail.
  // TODO: b/418269101 - Ensure this test succeeds once we have test data that
  // includes an access_policy_sha256 claim.
  EXPECT_THAT(result.status(), IsCode(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(result.status().message(),
              HasSubstr("Signature verification failed"));
}

// Tests whether the AttestationVerificationRecord emitted by the
// OakRustAttestationVerifier contains sufficient information to allow someone
// to re-do the verification (e.g. on their computer, by calling into the Oak
// Attestation Verification library themselves).
TEST(OakRustAttestationTest,
     AttestationVerificationRecordContainsEnoughInfoToReplayVerification) {
  // First, perform a normal verification pass using known-good values.
  ConfidentialEncryptionConfig encryption_config =
      GetKnownValidEncryptionConfig();
  ReferenceValues reference_values = GetKnownValidReferenceValues();

  // Create a valid access policy proto with some non-default content.
  confidentialcompute::DataAccessPolicy access_policy = PARSE_TEXT_PROTO(R"pb(
    transforms {
      src: 0
      application { tag: "foo" }
    }
  )pb");
  auto access_policy_bytes = access_policy.SerializeAsString();

  confidentialcompute::AttestationVerificationRecord verification_record;
  // This verifier will only accept attestation evidence matching the reference
  // values defined above, and will only accept the given access policy.
  OakRustAttestationVerifier verifier(
      reference_values, ReferenceValues(),
      {absl::BytesToHexString(ComputeSHA256(access_policy_bytes))},
      confidentialcompute::AccessPolicyEndorsementOptions(),
      [&verification_record](
          confidentialcompute::AttestationVerificationRecord record) {
        verification_record = record;
      });

  // Ensure that the verification succeeds.
  auto result = verifier.Verify(absl::Cord(access_policy_bytes),
                                confidentialcompute::SignedEndorsements(),
                                encryption_config);
  ASSERT_OK(result);

  // Ensure that the verification record logger was called and provided the
  // relevant information.
  EXPECT_THAT(verification_record.attestation_evidence(),
              EqualsProto(encryption_config.attestation_evidence()));
  EXPECT_THAT(verification_record.attestation_endorsements(),
              EqualsProto(encryption_config.attestation_endorsements()));
  EXPECT_THAT(verification_record.data_access_policy(),
              EqualsProto(access_policy));

  // Now, let's act like we're re-verifying the information in the
  // AttestationVerificationRecord in an offline fashion, by calling directly
  // into the Rust-based Oak Attestation Verification library.

  // First, we'll pass the attestation evidence to the verification library
  // using a ReferenceValues proto that skips all actual checks. This allows us
  // to access the information embedded within the attestation evidence more
  // easily.
  absl::StatusOr<oak::attestation::v1::AttestationResults>
      raw_attestation_results = fcp::client::rust::
          oak_attestation_verification_ffi::VerifyAttestation(
              absl::Now(), verification_record.attestation_evidence(),
              verification_record.attestation_endorsements(),
              GetSkipAllReferenceValues());
  ASSERT_OK(raw_attestation_results);
  ASSERT_EQ(raw_attestation_results->status(),
            oak::attestation::v1::AttestationResults::STATUS_SUCCESS)
      << raw_attestation_results->reason();

  // Then, let's create a ReferenceValues proto that requires the attestation
  // evidence to be rooted in the AMD SEV-SNP hardware root of trust, and which
  // requires each layer of the attestation evidence to match the exact binary
  // digests that were earlier reported in the `AttestationResults`.
  ReferenceValues reference_values_from_extracted_evidence;
  // Populate root layer values.
  reference_values_from_extracted_evidence.mutable_oak_restricted_kernel()
      ->mutable_root_layer()
      ->mutable_amd_sev()
      ->mutable_min_tcb_version();
  *reference_values_from_extracted_evidence.mutable_oak_restricted_kernel()
       ->mutable_root_layer()
       ->mutable_amd_sev()
       ->mutable_stage0()
       ->mutable_digests()
       ->add_digests()
       ->mutable_sha2_384() = raw_attestation_results->extracted_evidence()
                                  .oak_restricted_kernel()
                                  .root_layer()
                                  .sev_snp()
                                  .initial_measurement();
  // Populate kernel layer values.
  *reference_values_from_extracted_evidence.mutable_oak_restricted_kernel()
       ->mutable_kernel_layer()
       ->mutable_kernel()
       ->mutable_digests()
       ->mutable_image()
       ->add_digests() = raw_attestation_results->extracted_evidence()
                             .oak_restricted_kernel()
                             .kernel_layer()
                             .kernel_image();
  *reference_values_from_extracted_evidence.mutable_oak_restricted_kernel()
       ->mutable_kernel_layer()
       ->mutable_kernel()
       ->mutable_digests()
       ->mutable_setup_data()
       ->add_digests() = raw_attestation_results->extracted_evidence()
                             .oak_restricted_kernel()
                             .kernel_layer()
                             .kernel_setup_data();
  *reference_values_from_extracted_evidence.mutable_oak_restricted_kernel()
       ->mutable_kernel_layer()
       ->mutable_kernel_cmd_line_text()
       ->mutable_string_literals()
       ->add_value() = raw_attestation_results->extracted_evidence()
                           .oak_restricted_kernel()
                           .kernel_layer()
                           .kernel_raw_cmd_line();
  *reference_values_from_extracted_evidence.mutable_oak_restricted_kernel()
       ->mutable_kernel_layer()
       ->mutable_init_ram_fs()
       ->mutable_digests()
       ->add_digests() = raw_attestation_results->extracted_evidence()
                             .oak_restricted_kernel()
                             .kernel_layer()
                             .init_ram_fs();
  *reference_values_from_extracted_evidence.mutable_oak_restricted_kernel()
       ->mutable_kernel_layer()
       ->mutable_memory_map()
       ->mutable_digests()
       ->add_digests() = raw_attestation_results->extracted_evidence()
                             .oak_restricted_kernel()
                             .kernel_layer()
                             .memory_map();
  *reference_values_from_extracted_evidence.mutable_oak_restricted_kernel()
       ->mutable_kernel_layer()
       ->mutable_acpi()
       ->mutable_digests()
       ->add_digests() = raw_attestation_results->extracted_evidence()
                             .oak_restricted_kernel()
                             .kernel_layer()
                             .acpi();
  // Populate application layer values.
  *reference_values_from_extracted_evidence.mutable_oak_restricted_kernel()
       ->mutable_application_layer()
       ->mutable_binary()
       ->mutable_digests()
       ->add_digests() = raw_attestation_results->extracted_evidence()
                             .oak_restricted_kernel()
                             .application_layer()
                             .binary();
  // Add a digest for the application layer config, if the extracted evidence
  // indicates there was an application layer config, otherwise skip the
  // application layer config check since the application doesn't have a config.
  if (raw_attestation_results->extracted_evidence()
          .oak_restricted_kernel()
          .application_layer()
          .config()
          .ByteSizeLong() > 0) {
    *reference_values_from_extracted_evidence.mutable_oak_restricted_kernel()
         ->mutable_application_layer()
         ->mutable_configuration()
         ->mutable_digests()
         ->add_digests() = raw_attestation_results->extracted_evidence()
                               .oak_restricted_kernel()
                               .application_layer()
                               .config();
  } else {
    reference_values_from_extracted_evidence.mutable_oak_restricted_kernel()
        ->mutable_application_layer()
        ->mutable_configuration()
        ->mutable_skip();
  }

  // Lastly, let's verify that verifying the attestation evidence reported in
  // the AttestationVerificationRecord using the now fully-specified
  // ReferenceValues still results in a successful verification.
  //
  // This shows that the data in the AttestationVerificationRecord was indeed
  // valid (as long as we can assume that the Oak Attestation Verification
  // library is implemented correctly).
  raw_attestation_results =
      fcp::client::rust::oak_attestation_verification_ffi::VerifyAttestation(
          absl::Now(), verification_record.attestation_evidence(),
          verification_record.attestation_endorsements(),
          reference_values_from_extracted_evidence);
  ASSERT_OK(raw_attestation_results);
  EXPECT_EQ(raw_attestation_results->status(),
            oak::attestation::v1::AttestationResults::STATUS_SUCCESS)
      << raw_attestation_results->reason();

  // The attestation verification passed, as expected. We can also show that the
  // ReferenceValues proto we constructed from the extracted attestation
  // evidence is effectively the same as the "known good ReferenceValues" we use
  // in the other tests.
  EXPECT_THAT(reference_values_from_extracted_evidence,
              EqualsProto(GetKnownValidReferenceValues()));

  // At this point someone performing an offline re-verification could start
  // looking at the specific binaries that the attestation evidence attested to,
  // by looking them up using the binary digests in the ReferenceValues we
  // constructed above.
}

}  // namespace

}  // namespace fcp::client::attestation
