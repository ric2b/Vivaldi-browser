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

#ifndef FCP_CLIENT_ATTESTATION_OAK_RUST_ATTESTATION_VERIFIER_H_
#define FCP_CLIENT_ATTESTATION_OAK_RUST_ATTESTATION_VERIFIER_H_

#include <string>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "fcp/client/attestation/attestation_verifier.h"
#include "fcp/protos/confidentialcompute/access_policy_endorsement_options.pb.h"
#include "fcp/protos/confidentialcompute/signed_endorsements.pb.h"
#include "fcp/protos/confidentialcompute/verification_record.pb.h"
#include "fcp/protos/federatedcompute/confidential_aggregations.pb.h"
#include "proto/attestation/endorsement.pb.h"
#include "proto/attestation/evidence.pb.h"
#include "proto/attestation/reference_value.pb.h"
#include "proto/attestation/verification.pb.h"

namespace fcp::client::attestation {

// An AttestationVerifier implementation that is backed by the Oak Attestation
// Verification library written in Rust.
class OakRustAttestationVerifier : public AttestationVerifier {
 public:
  // Creates a new verifier that will use either of the given `ReferenceValues`
  // to validate the attestation evidence, and which will validate the given
  // access policies.
  //
  // As long as one of the `ReferenceValues` matches the attestation evidence,
  // the verification will succeed. This makes it possible to transition from
  // one set of reference values to another.
  //
  // The `record_logger` parameter will be called with a record of every
  // successful attestation verification.
  OakRustAttestationVerifier(
      oak::attestation::v1::ReferenceValues public_key_reference_values,
      oak::attestation::v1::ReferenceValues
          public_key_reference_values_secondary,
      absl::flat_hash_set<std::string> allowlisted_access_policy_hashes,
      confidentialcompute::AccessPolicyEndorsementOptions
          access_policy_endorsement_options,
      absl::AnyInvocable<
          void(const fcp::confidentialcompute::AttestationVerificationRecord&)>
          record_logger)
      : public_key_reference_values_(std::move(public_key_reference_values)),
        public_key_reference_values_secondary_(
            std::move(public_key_reference_values_secondary)),
        allowlisted_access_policy_hashes_(
            std::move(allowlisted_access_policy_hashes)),
        access_policy_endorsement_options_(
            std::move(access_policy_endorsement_options)),
        record_logger_(std::move(record_logger)) {}

  absl::StatusOr<VerificationResult> Verify(
      const absl::Cord& access_policy,
      const fcp::confidentialcompute::SignedEndorsements& signed_endorsements,
      const google::internal::federatedcompute::v1::
          ConfidentialEncryptionConfig& encryption_config) override;

 private:
  oak::attestation::v1::ReferenceValues public_key_reference_values_;
  oak::attestation::v1::ReferenceValues public_key_reference_values_secondary_;
  absl::flat_hash_set<std::string> allowlisted_access_policy_hashes_;
  fcp::confidentialcompute::AccessPolicyEndorsementOptions
      access_policy_endorsement_options_;
  absl::AnyInvocable<void(
      const fcp::confidentialcompute::AttestationVerificationRecord&)>
      record_logger_;
};

}  // namespace fcp::client::attestation

#endif  // FCP_CLIENT_ATTESTATION_OAK_RUST_ATTESTATION_VERIFIER_H_
