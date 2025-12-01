#ifndef FCP_CLIENT_RUST_OAK_ATTESTATION_VERIFICATION_FFI_H_
#define FCP_CLIENT_RUST_OAK_ATTESTATION_VERIFICATION_FFI_H_

#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "proto/attestation/endorsement.pb.h"
#include "proto/attestation/evidence.pb.h"
#include "proto/attestation/reference_value.pb.h"
#include "proto/attestation/verification.pb.h"

namespace fcp::client::rust::oak_attestation_verification_ffi {

// Verifies the given attestation evidence using the provided endorsements and
// reference values, and returns the verification result as an
// `AttestationResults` proto (which indicates whether verification passed or
// not).
//
// Returns an error if the verification failed for an unexpected reason, e.g. if
// the provided evidence was malformed (as opposed to being well-formed but not
// passing the verification criteria specified by the `ReferenceValues`).
//
// This function delegates the actual verification process to the Oak
// Attestation Verification crate
// https://github.com/project-oak/oak/tree/main/oak_attestation_verification.
absl::StatusOr<oak::attestation::v1::AttestationResults> VerifyAttestation(
    absl::Time now, const oak::attestation::v1::Evidence& evidence,
    const oak::attestation::v1::Endorsements& endorsements,
    const oak::attestation::v1::ReferenceValues& reference_values,
    bool use_policy_api = true);

// Verifies the given signed endorsement against a reference value.
//
// Returns OK whenever the endorsement is valid, and FailedPreconditionError
// with a message describing the failure otherwise.
//
// This function delegates the actual verification process to the Oak
// Attestation Verification crate
// https://github.com/project-oak/oak/tree/main/oak_attestation_verification.
absl::StatusOr<oak::attestation::v1::EndorsementDetails> VerifyEndorsement(
    absl::Time now,
    const oak::attestation::v1::SignedEndorsement& signed_endorsement,
    const oak::attestation::v1::EndorsementReferenceValue& reference_value);

}  // namespace fcp::client::rust::oak_attestation_verification_ffi

#endif  // FCP_CLIENT_RUST_OAK_ATTESTATION_VERIFICATION_FFI_H_
