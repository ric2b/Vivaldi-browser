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

#include "fcp/client/attestation/extract_attestation_records.h"

#include <iostream>
#include <istream>
#include <optional>
#include <string>
#include <utility>

#include "absl/functional/function_ref.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "fcp/base/compression.h"
#include "fcp/base/digest.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/parsing_utils.h"
#include "fcp/protos/confidentialcompute/verification_record.pb.h"

namespace fcp::client::attestation {

namespace {

// Finds and returns a single chunk of serialized attestation record data in the
// given line of text, if there is one.
std::optional<absl::string_view> ExtractSerializedChunk(absl::string_view line,
                                                        int line_num) {
  auto fcp_attest_marker = line.find("fcp.attest");
  auto start_bracket = line.find('<');
  auto end_bracket = line.find('>');
  if (fcp_attest_marker == std::string::npos ||
      start_bracket == std::string::npos) {
    return std::nullopt;
  }
  // A set of normal serialized log record lines look something like this:
  //
  // fcp.attest: <ABC>
  // fcp.attest: <DEF>
  // fcp.attest: <GH>
  // fcp.attest: <>
  //
  // where "ABC" and "DEF" are strings of 200 characters long, and "GH" is a
  // string of less than 200 characters long.
  //
  // However, early versions of `oak_rust_attestation_verifier.cc` erroneously
  // output more than the desired 200 chars per logcat line, on Android, due to
  // a bug in the format string used. Such lines are a) longer than 200 chars,
  // and b) don't always end with an ending bracket, and c) their content
  // overlaps with the next line. Such records look something like this:

  // fcp.attest: <ABCDEFGH
  // fcp.attest: <DEFGH>
  // fcp.attest: <GH>
  // fcp.attest: <>
  //
  // where "ABCDEF" is a string of more than 200 characters long that is not
  // enclosed with a closing bracket, "DEFGH" is string of more than 200
  // characters long that *is* enclosed with a closing bracket, and "GH" is a
  // string that is less than 200 characters long and enclosed.
  //
  // We have to detect such overlong lines, and manually extract the first 200
  // characters after the starting bracket, ignoring all other characters in the
  // line, such that we arrive at a correct extracted value of "ABCDEFGH".
  if ((end_bracket == std::string::npos &&
       line.length() - start_bracket > 200) ||
      (end_bracket != std::string::npos && end_bracket - start_bracket > 200)) {
    FCP_LOG(WARNING) << "Detected overlong line with length " << line.length()
                     << " (with " << line.length() - start_bracket
                     << " chars after the '<' bracket) at line " << line_num;
    end_bracket = start_bracket + 201;
  }
  if (end_bracket == std::string::npos || end_bracket <= start_bracket) {
    return std::nullopt;
  }
  absl::string_view chunk =
      line.substr(start_bracket + 1, end_bracket - start_bracket - 1);
  return chunk;
}

// Parses an extracted record by first base64-decoding it, then decompressing
// it, and then validating that it parses correctly.
//
// Returns the record's decompressed data, as well as some metadata about where
// it was found.
std::optional<Record> ParseRecord(absl::string_view serialized_record,
                                  int start_line, int end_line) {
  // Decode.
  std::string compressed_record;
  if (!absl::Base64Unescape(serialized_record, &compressed_record)) {
    FCP_LOG(ERROR) << "Failed to decode record at lines " << start_line << "-"
                   << end_line;
    return std::nullopt;
  }

  // Decompress.
  absl::StatusOr<absl::Cord> decompressed_record =
      fcp::UncompressWithGzip(compressed_record);
  if (!decompressed_record.ok()) {
    FCP_LOG(ERROR) << "Failed to decompress record at lines " << start_line
                   << "-" << end_line;
    return std::nullopt;
  }

  // Validate that the data successfully parses.
  fcp::confidentialcompute::AttestationVerificationRecord parsed_record;
  if (!ParseFromStringOrCord(parsed_record, *decompressed_record)) {
    FCP_LOG(ERROR) << "Failed to parse record at lines " << start_line << "-"
                   << end_line;
    return std::nullopt;
  }

  // Calculate the digest.
  std::string digest =
      absl::BytesToHexString(fcp::ComputeSHA256(*decompressed_record));

  return Record{.decompressed_record = std::move(*decompressed_record),
                .digest = std::move(digest),
                .start_line = start_line,
                .end_line = end_line};
}

}  // namespace

int ExtractRecords(std::istream& input_stream,
                   absl::FunctionRef<void(const Record&)> record_callback) {
  int num_records = 0;
  int current_line = 0;
  int start_line = 0;
  std::string serialized_record;
  for (std::string line; std::getline(input_stream, line);) {
    current_line++;

    std::optional<absl::string_view> chunk =
        ExtractSerializedChunk(line, current_line);
    // If not chunk was found at all, then we just ignore this line.
    if (!chunk) {
      continue;
    }
    if (!start_line) {
      // We found the start of a new record.
      start_line = current_line;
    }

    // We'll know we've hit the end of a serialized record when we hit an empty
    // chunk. Until that time we should keep accumulating chunk data into the
    // buffer.
    if (!chunk->empty()) {
      FCP_LOG(INFO) << "Read chunk of size " << chunk->size();
      serialized_record += *chunk;
      continue;
    }

    // We've hit the end of serialized record, so parse it and add it to the
    // result vector.
    std::optional<Record> record =
        ParseRecord(serialized_record, start_line, current_line);
    if (record) {
      num_records++;
      record_callback(*record);
    }

    // Reset the state, allowing us to find the next record.
    start_line = 0;
    serialized_record.clear();
  }

  return num_records;
}

}  // namespace fcp::client::attestation
