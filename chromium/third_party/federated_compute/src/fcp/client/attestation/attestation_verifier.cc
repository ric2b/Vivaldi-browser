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

#include "fcp/client/attestation/attestation_verifier.h"

#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "fcp/base/digest.h"
#include "fcp/confidentialcompute/cose.h"
#include "fcp/protos/federatedcompute/confidential_aggregations.pb.h"

namespace fcp::client::attestation {
using ::fcp::confidential_compute::OkpCwt;
using ::google::internal::federatedcompute::v1::ConfidentialEncryptionConfig;

absl::StatusOr<AttestationVerifier::VerificationResult>
AlwaysFailingAttestationVerifier::Verify(
    const absl::Cord& access_policy,
    const confidentialcompute::SignedEndorsements& signed_endorsements,
    const ConfidentialEncryptionConfig& encryption_config) {
  return absl::FailedPreconditionError(
      "Attestation verification failed unconditionally.");
}

absl::StatusOr<AttestationVerifier::VerificationResult>
AlwaysPassingAttestationVerifier::Verify(
    const absl::Cord& access_policy,
    const confidentialcompute::SignedEndorsements& signed_endorsements,
    const ConfidentialEncryptionConfig& encryption_config) {
  absl::StatusOr<OkpCwt> cwt = OkpCwt::Decode(encryption_config.public_key());
  if (!cwt.ok()) {
    std::string error_msg =
        "Parsing confidential aggregation public key failed.";
    return absl::Status(cwt.status().code(), error_msg);
  }
  return VerificationResult{
      .serialized_public_key = encryption_config.public_key(),
      .key_id = std::move(cwt->public_key->key_id),
      .access_policy_sha256 = ComputeSHA256(access_policy),
  };
}

}  // namespace fcp::client::attestation
