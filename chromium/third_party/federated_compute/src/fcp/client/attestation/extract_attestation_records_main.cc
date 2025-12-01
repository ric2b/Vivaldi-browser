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

#include <filesystem>  // NOLINT(build/c++17)
#include <fstream>
#include <iostream>
#include <istream>
#include <optional>
#include <string>
#include <utility>


#include "absl/flags/flag.h"
#include "absl/flags/parse.h"  // IWYU pragma: keep
#include "absl/flags/usage.h"  // IWYU pragma: keep
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/attestation/extract_attestation_records.h"

ABSL_FLAG(std::string, input, "-",
          "The input file to extract verification records from (defaults to "
          "'-', which reads from stdin)");
ABSL_FLAG(std::string, output, ".",
          "The path to write extracted verification records to (defaults to "
          "the current working directory)");

static constexpr char kUsageString[] =
    "Extracts serialized attestation verification records text from a stream "
    "of text.\n\n"
    "Note that this utility currently does not correctly handle interleaved "
    "attestation records (such as when two serialized records were output to "
    "the same log stream by two concurrent processes, resulting in interleaved "
    "log lines). Such cases need to be manually pre-processed before using this"
    "tool.";

using fcp::client::attestation::ExtractRecords;
using fcp::client::attestation::Record;

// Writes an extracted record to a file with a descriptive filename.
//
// Returns the path to the file written, if writing was successful.
std::optional<std::string> WriteRecordToFile(std::filesystem::path output_path,
                                             const Record& record) {
  std::string file_name =
      absl::StrCat("record_l", record.start_line, "_to_l", record.end_line,
                   "_digest", record.digest.substr(0, 8), ".pb");
  auto file_path = std::filesystem::path(std::move(output_path)) / file_name;
  std::ofstream output_stream(file_path);
  if (output_stream.fail()) {
    FCP_LOG(ERROR) << "Failed to open output file: " << file_path;
    return std::nullopt;
  }
  output_stream << record.decompressed_record;
  if (output_stream.fail()) {
    FCP_LOG(ERROR) << "Failed to write to output file: " << file_path;
    return std::nullopt;
  }
  output_stream.close();
  return file_path;
}

int main(int argc, char** argv) {
  // Parse the command line flags.
  absl::SetProgramUsageMessage(kUsageString);
  absl::ParseCommandLine(argc, argv);
  std::string input = absl::GetFlag(FLAGS_input);
  std::string output_path = absl::GetFlag(FLAGS_output);
  FCP_LOG(INFO) << "Extracting serialized verification records from " << input
                << " and outputting them to " << output_path;

  // Decide on the input stream to read from (either stdin or a file).
  std::optional<std::ifstream> input_fstream;
  std::istream* input_stream;
  if (input == "-") {
    input_stream = &std::cin;
  } else {
    input_fstream = std::ifstream(std::filesystem::path(input));
    if (input_fstream->fail()) {
      FCP_LOG(ERROR) << "Failed to open input file: " << input;
      return 1;
    }
    input_stream = &*input_fstream;
  }

  // Check that the output path exists.
  if (!std::filesystem::exists(output_path)) {
    FCP_LOG(ERROR) << "Output path does not exist: " << output_path;
    return 1;
  }

  // Extract records and write them to files.
  int num_records = ExtractRecords(*input_stream, [&](const Record& record) {
    std::optional<std::string> file_path =
        WriteRecordToFile(output_path, record);
    if (!file_path) {
      return;
    }

    std::cout << "Wrote a verification record extracted from lines "
              << record.start_line << "-" << record.end_line << " to "
              << *file_path << std::endl;
  });

  // Print a summary.
  std::cout << "Found " << num_records << " record(s)." << std::endl;
  return 0;
}
