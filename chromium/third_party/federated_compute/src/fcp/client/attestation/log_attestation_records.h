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

#ifndef FCP_CLIENT_ATTESTATION_LOG_ATTESTATION_RECORDS_H_
#define FCP_CLIENT_ATTESTATION_LOG_ATTESTATION_RECORDS_H_

#include "absl/functional/function_ref.h"
#include "absl/strings/string_view.h"
#include "fcp/protos/confidentialcompute/verification_record.pb.h"

namespace fcp::client::attestation {

// A verification record logger which simply logs the pretty printed
// `AttestationVerificationRecord` to `FCP_LOG(INFO)`.
//
// Note: this is not intended for use in production, but is convenient for use
// in tests etc.
void LogPrettyPrintedVerificationRecord(
    const fcp::confidentialcompute::AttestationVerificationRecord& record);

// A verification record logger which serializes, compresses, and base64-encodes
// the record before logging it to either `FCP_VLOG(1)` (non-Android builds) or
// to Android Logcat using the DEBUG log level (for Android builds).
void LogSerializedVerificationRecord(
    const fcp::confidentialcompute::AttestationVerificationRecord& record);

namespace internal {

// A helper function that serializes, compresses, and base64-encodes the record
// and chunks the result up into small chunks which are handed to the `logger`
// for actual printing.
void LogSerializedVerificationRecordWith(
    const fcp::confidentialcompute::AttestationVerificationRecord& record,
    absl::FunctionRef<void(absl::string_view message,
                           bool enclose_with_brackets)>
        logger);
}  // namespace internal

}  // namespace fcp::client::attestation

#endif  // FCP_CLIENT_ATTESTATION_LOG_ATTESTATION_RECORDS_H_
