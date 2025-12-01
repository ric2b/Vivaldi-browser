/*
 * Copyright 2024 Google LLC
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

#include "fcp/client/attestation/oak_rust_attestation_verifier.h"

#include <cstdint>
#include <string>
#include <utility>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "fcp/protos/confidentialcompute/signed_endorsements.pb.h"

#ifdef __ANDROID__
#include <android/log.h>
#endif

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/escaping.h"
#include "absl/strings/substitute.h"
#include "absl/time/clock.h"
#include "fcp/base/digest.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/rust/oak_attestation_verification_ffi.h"
#include "fcp/confidentialcompute/cose.h"
#include "fcp/confidentialcompute/crypto.h"
#include "fcp/protos/confidentialcompute/access_policy.pb.h"
#include "fcp/protos/confidentialcompute/access_policy_endorsement_options.pb.h"
#include "fcp/protos/confidentialcompute/verification_record.pb.h"
#include "fcp/protos/federatedcompute/confidential_aggregations.pb.h"
#include "proto/attestation/endorsement.pb.h"
#include "proto/attestation/evidence.pb.h"
#include "proto/attestation/reference_value.pb.h"
#include "proto/attestation/verification.pb.h"

namespace fcp::client::attestation {
using ::fcp::confidential_compute::EcdsaP256R1SignatureVerifier;
using ::fcp::confidential_compute::OkpCwt;
using ::fcp::confidentialcompute::DataAccessPolicy;
using ::google::internal::federatedcompute::v1::ConfidentialEncryptionConfig;
using ::oak::attestation::v1::AttestationResults;
using ::oak::attestation::v1::EndorsementDetails;

// See https://www.iana.org/assignments/cose/cose.xhtml.
constexpr int64_t kAlgorithmES256 = -7;

namespace {
absl::StatusOr<AttestationResults> VerifyPublicKeyAttestation(
    const ConfidentialEncryptionConfig& encryption_config,
    const oak::attestation::v1::ReferenceValues& public_key_reference_values) {
  // Validate the attestation evidence provided in the encryption config, using
  // the `public_key_reference_values_` provided to us at construction time.
  // TODO: b/432726860 - enable use_policy_api.
  FCP_ASSIGN_OR_RETURN(
      AttestationResults attestation_results,
      fcp::client::rust::oak_attestation_verification_ffi::VerifyAttestation(
          absl::Now(), encryption_config.attestation_evidence(),
          encryption_config.attestation_endorsements(),
          public_key_reference_values, /*use_policy_api=*/false));

  if (attestation_results.status() != AttestationResults::STATUS_SUCCESS) {
    return absl::FailedPreconditionError(absl::Substitute(
        "(status: $0, reason: $1)", attestation_results.status(),
        attestation_results.reason()));
  }
  return attestation_results;
}
}  // namespace

absl::StatusOr<OakRustAttestationVerifier::VerificationResult>
OakRustAttestationVerifier::Verify(
    const absl::Cord& access_policy,
    const confidentialcompute::SignedEndorsements& signed_endorsements,
    const ConfidentialEncryptionConfig& encryption_config) {
  absl::StatusOr<AttestationResults> attestation_results =
      VerifyPublicKeyAttestation(encryption_config,
                                 public_key_reference_values_);
  if (!attestation_results.ok()) {
    absl::StatusOr<AttestationResults> attestation_results_secondary =
        public_key_reference_values_secondary_.type_case() ==
                oak::attestation::v1::ReferenceValues::TYPE_NOT_SET
            ? absl::NotFoundError("No secondary reference values provided.")
            : VerifyPublicKeyAttestation(
                  encryption_config, public_key_reference_values_secondary_);
    if (!attestation_results_secondary.ok()) {
      return absl::FailedPreconditionError(absl::Substitute(
          "Attestation verification failed for both primary and secondary "
          "reference values: $0, $1",
          attestation_results.status(),
          attestation_results_secondary.status()));
    }
    attestation_results = attestation_results_secondary;
  }

  // Ensure that the provided data access policy parses correctly.
  DataAccessPolicy parsed_access_policy;
  if (!parsed_access_policy.ParseFromString(access_policy)) {
    return absl::FailedPreconditionError("DataAccessPolicy failed to parse.");
  }

  // Next, validate whether the provided access policy is allowlisted.
  //
  // Note that we must calculate the hash over the unparsed absl::Cord holding
  // the serialized access policy bytes, rather than calculating it over
  // `parsed_access_policy.SerializeAsString()`, because the re-serialized
  // representation may differ slightly from the original serialization (e.g.
  // due non-deterministic map serialization).
  std::string raw_access_policy_hash = ComputeSHA256(access_policy);
  std::string access_policy_hash =
      absl::BytesToHexString(raw_access_policy_hash);

  if (signed_endorsements.signed_endorsement().empty()) {
    // If we have an empty SignedEndorsements proto, we'll use the legacy
    // allowlisted access policy hashes to validate the access policy.
    if (!allowlisted_access_policy_hashes_.contains(access_policy_hash)) {
      return absl::FailedPreconditionError(absl::Substitute(
          "Data access policy not in allowlist ($0).", access_policy_hash));
    }
  } else {
    // Currently, we only support a single SignedEndorsement and a single
    // EndorsementReferenceValue. If we have more than one of either, fail.
    if (signed_endorsements.signed_endorsement().size() > 1) {
      return absl::FailedPreconditionError(absl::Substitute(
          "Only a single SignedEndorsement is supported, but $0 "
          "were provided.",
          signed_endorsements.signed_endorsement().size()));
    }

    const auto& signed_endorsement = signed_endorsements.signed_endorsement(0);

    if (!signed_endorsement.has_endorsement()) {
      return absl::FailedPreconditionError(
          "SignedEndorsement does not contain an endorsement.");
    }
    if (access_policy_endorsement_options_.endorsement_reference_values()
            .size() != 1) {
      return absl::FailedPreconditionError(absl::Substitute(
          "Only a single EndorsementReferenceValue is supported, but $0 "
          "were provided.",
          access_policy_endorsement_options_.endorsement_reference_values()
              .size()));
    }

    const auto& endorsement_reference_value =
        access_policy_endorsement_options_.endorsement_reference_values(0);

    // verify_endorsement returns the hash of the endorsed access policy.
    FCP_ASSIGN_OR_RETURN(
        EndorsementDetails endorsement_details,
        fcp::client::rust::oak_attestation_verification_ffi::VerifyEndorsement(
            absl::Now(), signed_endorsement, endorsement_reference_value));
    // We must hex-escape the hash, because the hash we're comparing against is
    // also hex-escaped.
    auto endorsed_access_policy_hash =
        absl::BytesToHexString(endorsement_details.subject_digest().sha2_256());
    if (access_policy_hash != endorsed_access_policy_hash) {
      return absl::FailedPreconditionError(absl::Substitute(
          "The digest of the endorsed access policy ($0) does not match the "
          "digest of the provided access policy ($1).",
          endorsed_access_policy_hash, access_policy_hash));
    }
    // If we get here, the endorsement is valid and correctly endorses the
    // access policy.
  }

  // Next, let's validate the CWT-encoded public key.
  //
  // First, we'll parse it and check that is signed by the expected ECDSA256
  // signing algorithm. Then validate that it CWT is correctly signed by the
  // signing key from the AttestationResults, and then we'll return the key to
  // the caller.
  absl::StatusOr<OkpCwt> cwt = OkpCwt::Decode(encryption_config.public_key());
  if (!cwt.ok()) {
    return absl::Status(
        cwt.status().code(),
        absl::Substitute(
            "Parsing confidential aggregation public key failed ($0).",
            cwt.status().message()));
  }

  // Ensure that the CWT is signed using the expected ECDSA256 signing
  // algorithm.
  if (!cwt->algorithm.has_value() || *cwt->algorithm != kAlgorithmES256) {
    return absl::FailedPreconditionError(absl::Substitute(
        "Unsupported COSE signing algorithm ($0).", *cwt->algorithm));
  }

  // If the CWT includes a claim for the access policy hash (KMS only), ensure
  // that it matches the access policy we received.
  if (!cwt->access_policy_sha256.empty() &&
      cwt->access_policy_sha256 != raw_access_policy_hash) {
    return absl::FailedPreconditionError(
        "access_policy_sha256 claim does not match the access policy.");
  }

  // Extract the protected parts of the CWT (the COSE Sig_structure), which are
  // the parts of the CWT that are covered by the signature.
  absl::StatusOr<std::string> cwt_sig_structure =
      OkpCwt::GetSigStructureForVerifying(
          /*cwt=*/encryption_config.public_key(), /*aad=*/"");
  if (!cwt_sig_structure.ok()) {
    return absl::Status(
        cwt.status().code(),
        absl::Substitute("Failed to extract CWT Sig_structure ($0).",
                         cwt_sig_structure.status().message()));
  }

  // Now verify the CWT signature.
  auto signature_verifier = EcdsaP256R1SignatureVerifier::Create(
      attestation_results->extracted_evidence().signing_public_key());
  if (!signature_verifier.ok()) {
    return absl::Status(
        signature_verifier.status().code(),
        absl::Substitute("Failed to create signature verifier ($0).",
                         signature_verifier.status().message()));
  }
  auto sig_verify_result =
      signature_verifier->Verify(*cwt_sig_structure, cwt->signature);
  if (!sig_verify_result.ok()) {
    return absl::FailedPreconditionError(absl::Substitute(
        "Signature verification failed (code: $0, msg: $1).",
        sig_verify_result.code(), sig_verify_result.message()));
  }

  // Verification of the attestation evidence, the access policy, and the public
  // encryption key's signature succeeded!

  // We now log the key information we used to perform the verification to the
  // provided logger. This allows someone observing these logs to replay the
  // same verification, as well as look up the binaries that will process the
  // encrypted data. See the
  // AttestationVerificationRecordContainsEnoughInfoToReplayVerification test in
  // oak_rust_attestation_verifier_test.cc for an example of how this
  // information can be used for that purpose.
  confidentialcompute::AttestationVerificationRecord verification_record;
  *verification_record.mutable_attestation_evidence() =
      encryption_config.attestation_evidence();
  *verification_record.mutable_attestation_endorsements() =
      encryption_config.attestation_endorsements();
  *verification_record.mutable_data_access_policy() =
      std::move(parsed_access_policy);
  record_logger_(verification_record);

  // Return the information with which the caller can now safely encrypt data
  // to be uploaded. Only the attested server binary will have access to the
  // decryption key, and the it will only allow the decryption key to be
  // used by binaries/applications allowed by the data access policy.
  return VerificationResult{
      .serialized_public_key = encryption_config.public_key(),
      .key_id = std::move(cwt->public_key->key_id),
      .access_policy_sha256 = std::move(raw_access_policy_hash),
  };
}

}  // namespace fcp::client::attestation
