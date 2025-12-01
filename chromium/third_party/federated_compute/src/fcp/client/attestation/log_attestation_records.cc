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

#include "fcp/client/attestation/log_attestation_records.h"

#include <ostream>
#include <string>

#include "absl/functional/function_ref.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "fcp/base/compression.h"
#include "fcp/base/monitoring.h"
#include "fcp/protos/confidentialcompute/access_policy.pb.h"
#include "fcp/protos/confidentialcompute/verification_record.pb.h"
#include "fcp/protos/federatedcompute/confidential_aggregations.pb.h"
#include "proto/attestation/endorsement.pb.h"
#include "proto/attestation/evidence.pb.h"
#include "proto/attestation/reference_value.pb.h"
#include "proto/attestation/verification.pb.h"

#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace fcp::client::attestation {

inline constexpr char kVerificationRecordLogInfoMessage[] =
    "This device is contributing data via the confidential aggregation "
    "protocol. The attestation verification record follows.";
inline constexpr char kSerializedVerificationRecordLogTag[] = "fcp.attest";

void LogPrettyPrintedVerificationRecord(
    const confidentialcompute::AttestationVerificationRecord& record) {
  FCP_LOG(INFO) << kVerificationRecordLogInfoMessage << std::endl
                << record.DebugString();
}

// When printing a serialized verification record, we split it up into equal
// chunks of this length. This ensures that if we print to Android logcat,
// we never go above per-log-message length limit of around 4000 characters.
inline constexpr int kMaxSerializedVerificationChunkLength = 200;
// The "%.*s" format string uses a ".*" precision specifier, which indicates
// that the actual precision value is being supplied as an additional integer
// argument. Since this is a string-formatted value, the precision value
// determines the number of characters that will be printed from the `const
// char*` argument.
inline constexpr char kBracketedChunkFmtString[] = "<%.*s>";
inline constexpr char kNonBracketedChunkFmtString[] = "%.*s";

void LogSerializedVerificationRecord(
    const confidentialcompute::AttestationVerificationRecord& record) {
  // By default we use FCP_VLOG(1) to log messages, unless we're on Android
  // where we log directly to logcat instead.
  auto logger = [](absl::string_view message, bool enclose_with_brackets) {
#ifndef __ANDROID__
    FCP_LOG(INFO) << kSerializedVerificationRecordLogTag << ": "
                  << (enclose_with_brackets
                          ? absl::StreamFormat(kBracketedChunkFmtString,
                                               static_cast<int>(message.size()),
                                               message.data())
                          : absl::StreamFormat(kNonBracketedChunkFmtString,
                                               static_cast<int>(message.size()),
                                               message.data()));
#else
    // We log at DEBUG log level, which means that the log will not actually be
    // emitted unless the user calls `adb shell setprop log.tag.fcp.attest D`.
    // See https://developer.android.com/tools/logcat#Overview for more info.
    //
    // This is important because a serialized verification record could measure
    // multiple kilobytes in size, the system-wide logcat buffer has a limited
    // capacity, and hence we don't want to spam the logcat buffer
    // unnecessarily.
    __android_log_print(ANDROID_LOG_DEBUG, kSerializedVerificationRecordLogTag,
                        enclose_with_brackets ? kBracketedChunkFmtString
                                              : kNonBracketedChunkFmtString,
                        static_cast<int>(message.size()), message.data());
#endif
  };

  // Do the actual logging of the record, using the logger above to log
  // individual messages.
  internal::LogSerializedVerificationRecordWith(record, logger);
}

namespace internal {
void LogSerializedVerificationRecordWith(
    const confidentialcompute::AttestationVerificationRecord& record,
    absl::FunctionRef<void(absl::string_view, bool)> logger) {
  absl::StatusOr<std::string> compressed_record =
      CompressWithGzip(record.SerializeAsString());
  // If we fail to compress the record then something must've gone horribly
  // wrong, and we should just bail.
  FCP_CHECK_STATUS(compressed_record.status());
  // We base64-encode the record to ensure it's easily printable.
  std::string encoded_record = absl::Base64Escape(*compressed_record);

  logger(kVerificationRecordLogInfoMessage, /*enclose_with_brackets=*/false);
  for (absl::string_view chunk :
       absl::StrSplit(encoded_record,
                      absl::ByLength(kMaxSerializedVerificationChunkLength))) {
    // Note: base64-encoding is guaranteed to not have any '<' or '>' characters
    // in it. By surrounding the chunks in these angle brackets one can more
    // easily reconstruct the full string from a series of log entries.
    logger(chunk, /*enclose_with_brackets=*/true);
  }
  // Emit one last "empty" chunk to the logs. This will make it easier to
  // separate sequential serialized attestation records in a single log stream.
  logger("", /*enclose_with_brackets=*/true);
}

}  // namespace internal

}  // namespace fcp::client::attestation
