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

#ifndef FCP_CLIENT_ATTESTATION_EXTRACT_ATTESTATION_RECORDS_H_
#define FCP_CLIENT_ATTESTATION_EXTRACT_ATTESTATION_RECORDS_H_

#include <istream>
#include <string>

#include "absl/functional/function_ref.h"
#include "absl/strings/cord.h"

namespace fcp::client::attestation {

// A (serialized) attestation record extracted from an input stream.
struct Record {
  // The decompressed record data.
  absl::Cord decompressed_record;
  // The SHA256 digest of the data (can be used to easily identify identical
  // records in an input stream).
  std::string digest;
  // The start and end line numbers describing where in the input stream the
  // record was found.
  int start_line = 0;
  int end_line = 0;
};

// Extracts one or more records from the given input stream.
//
// Returns the number of records that were extracted.
int ExtractRecords(std::istream& input_stream,
                   absl::FunctionRef<void(const Record&)> record_callback);

}  // namespace fcp::client::attestation

#endif  // FCP_CLIENT_ATTESTATION_EXTRACT_ATTESTATION_RECORDS_H_
