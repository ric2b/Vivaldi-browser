#include "fcp/client/rust/oak_attestation_verification_ffi.h"

#include <nl_types.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

#include "absl/cleanup/cleanup.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "proto/attestation/endorsement.pb.h"
#include "proto/attestation/evidence.pb.h"
#include "proto/attestation/reference_value.pb.h"
#include "proto/attestation/verification.pb.h"

namespace fcp::client::rust::oak_attestation_verification_ffi {

using oak::attestation::v1::EndorsementDetails;

// See `oak_attestation_verification_ffi_wrapper.rs`.
struct SerializedResult {
  char* data;
  size_t size;
  bool is_err;
};

extern "C" SerializedResult
fcp_rs_oak_attestation_verification_verify_attestation(
    int64_t now_utc_millis, const char* serialized_evidence,
    size_t serialized_evidence_size, const char* serialized_endorsements,
    size_t, const char* serialized_reference_values, size_t, bool);

extern "C" void fcp_rs_oak_attestation_verification_free_result(
    SerializedResult result);

extern "C" SerializedResult
fcp_rs_oak_attestation_verification_verify_endorsement(int64_t now_utc_millis,
                                                       const char*, size_t,
                                                       const char*, size_t);

absl::StatusOr<oak::attestation::v1::AttestationResults> VerifyAttestation(
    absl::Time now, const oak::attestation::v1::Evidence& evidence,
    const oak::attestation::v1::Endorsements& endorsements,
    const oak::attestation::v1::ReferenceValues& reference_values,
    bool use_policy_api) {
  std::string serialized_evidence = evidence.SerializeAsString();
  std::string serialized_endorsements = endorsements.SerializeAsString();
  std::string serialized_reference_values =
      reference_values.SerializeAsString();

  SerializedResult attestation_result =
      fcp_rs_oak_attestation_verification_verify_attestation(
          absl::ToUnixMillis(now), serialized_evidence.data(),
          serialized_evidence.size(), serialized_endorsements.data(),
          serialized_endorsements.size(), serialized_reference_values.data(),
          serialized_reference_values.size(), use_policy_api);
  // Ensure that the Rust buffer gets released when we go out of scope.
  absl::Cleanup free_result_cleanup = [&attestation_result]() {
    fcp_rs_oak_attestation_verification_free_result(attestation_result);
  };

  // Ensure the returned data fits within the `int` size type used by
  // `ParseFromArray`.
  if (attestation_result.size > std::numeric_limits<int>::max()) {
    return absl::InternalError("Unexpectedly large attestation result");
  }

  oak::attestation::v1::AttestationResults result;
  // The result may have a size of 0 (e.g. in case of an
  // empty/default-initialized proto). We shouldn't try to parse the buffer in
  // that case, since the pointer will be null.
  if (attestation_result.size > 0 &&
      !result.ParseFromArray(attestation_result.data,
                             static_cast<int>(attestation_result.size))) {
    return absl::InternalError("Failed to parse AttestationResults");
  }
  return result;
}

absl::StatusOr<EndorsementDetails> VerifyEndorsement(
    absl::Time now,
    const oak::attestation::v1::SignedEndorsement& signed_endorsement,
    const oak::attestation::v1::EndorsementReferenceValue& reference_value) {
  std::string serialized_signed_endorsement =
      signed_endorsement.SerializeAsString();
  std::string serialized_reference_value = reference_value.SerializeAsString();

  SerializedResult r = fcp_rs_oak_attestation_verification_verify_endorsement(
      absl::ToUnixMillis(now), serialized_signed_endorsement.data(),
      serialized_signed_endorsement.size(), serialized_reference_value.data(),
      serialized_reference_value.size());
  // Ensure that the Rust buffer gets released when we go out of scope.
  absl::Cleanup free_result_cleanup = [&r]() {
    fcp_rs_oak_attestation_verification_free_result(r);
  };

  if (r.is_err) {
    return absl::FailedPreconditionError({r.data, r.size});
  }

  EndorsementDetails result;
  if (!result.ParseFromArray(r.data, static_cast<int>(r.size))) {
    return absl::InternalError("Failed to parse EndorsementDetails");
  }
  return result;
}

}  // namespace fcp::client::rust::oak_attestation_verification_ffi
