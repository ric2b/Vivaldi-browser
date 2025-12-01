// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// TODO(team): switch to re2 library
#include <regex>  // NOLINT
#include <string>

#include "gtest/gtest.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "fcp/base/monitoring.h"
#include "fcp/base/platform.h"
#include "fcp/testing/testing.h"

ABSL_FLAG(std::string, codegen_tool_path, "", "Path to codegen tool script");

namespace fcp {
namespace {

const char* kBaselineDir = "fcp/tracing/tools/testdata";

std::string PostProcessOutput(const std::string& input) {
  std::regex header_guard_simplifier(
      "(THIRD_PARTY_)?FCP_TRACING_TOOLS_TESTDATA");
  std::string header_guard_replaced = std::regex_replace(
      input, header_guard_simplifier, "THIRD_PARTY_FCP_TRACING_TOOLS_TESTDATA");
  std::regex runfiles_path_simplifier("\".*runfiles.*fcp/tracing");
  // replaces the runfile directory with {RUNFILE_PATH} for testing purposes
  std::string runfiles_replaced = std::regex_replace(
      header_guard_replaced, runfiles_path_simplifier, "\"${RUNFILE_PATH}");
  std::regex path_simplifier_pattern("( |\")(\\w+/)*fcp/tracing");
  // replaces the directory of the .fbs for testing purposes
  std::string directory_replaced = std::regex_replace(
      runfiles_replaced, path_simplifier_pattern, "$1${DIR}");
  std::regex fcp_base_path_simplifier("(\\w+/)?fcp/base");
  return std::regex_replace(directory_replaced, fcp_base_path_simplifier,
                            "${BASE}");
}

void DoTest() {
  std::string source_file = absl::StrCat(TestName(), ".fbs");
  std::string source_path =
      GetTestDataPath(ConcatPath(kBaselineDir, source_file));

  // Read fsb source file derived from the test name:
  StatusOr<std::string> source_s = ReadFileToString(source_path);
  ASSERT_THAT(source_s, IsOk()) << "Can't read " << source_path;
  std::string source = source_s.value();

  std::string out_file =
      ConcatPath(testing::TempDir(), absl::StrCat(TestName(), ".out"));
  std::string err_file =
      ConcatPath(testing::TempDir(), absl::StrCat(TestName(), ".err"));

  // Run codegen script, redirecting stdout to out_file and stderr to err_file
  int exit_code = system(
      absl::StrCat(GetTestDataPath(absl::GetFlag(FLAGS_codegen_tool_path)), " ",
                   source_path, " ", testing::TempDir(), " ", kBaselineDir,
                   " 1> ", out_file, " 2> ", err_file)
          .c_str());

  // Reading error and out files
  std::string out = ReadFileToString(out_file).value();
  std::string err = ReadFileToString(err_file).value();

  if (exit_code != 0) {
    // Codegen failed. This might be expected depending on the test.
    // In the case of failure we're not interested in capturing possible partial
    // output in baseline file.
    out.clear();
    if (err.empty()) {
      // If error is not empty it already contains relevant diagnostics,
      // otherwise adding information about exit code.
      err = absl::StrCat("Exit code ", exit_code);
    }
  }

  // Producing report which is expected to precisely match .baseline file.
  std::ostringstream report;
  report << "============== " << source_file << " ============" << '\n';
  report << PostProcessOutput(source) << '\n';
  report << "============== diagnosis ============" << '\n';
  report << PostProcessOutput(err) << '\n';
  report << "============== result ============" << '\n';
  report << PostProcessOutput(out) << '\n';

  // Compare produced report with baseline.
  std::string baseline_path =
      ConcatPath(kBaselineDir, absl::StrCat(TestName(), ".baseline"));
  auto status_s = VerifyAgainstBaseline(baseline_path, report.str());
  ASSERT_TRUE(status_s.ok()) << status_s.status();
  auto& diff = status_s.value();
  if (!diff.empty()) {
    FAIL() << diff;
  }
}

TEST(Codegen, EmptyTable) { DoTest(); }
TEST(Codegen, FieldsOfDifferentTypes) { DoTest(); }
TEST(Codegen, DeprecatedField) { DoTest(); }
TEST(Codegen, NonTableObjectsAreSkipped) { DoTest(); }
TEST(Codegen, AllTypes) { DoTest(); }
TEST(Codegen, OrderWithIds) { DoTest(); }
TEST(Codegen, NoTag) { DoTest(); }
TEST(Codegen, NoAttributes) { DoTest(); }
TEST(Codegen, TagTooLong) { DoTest(); }
TEST(Codegen, DuplicateTags) { DoTest(); }
TEST(Codegen, UnsupportedType) { DoTest(); }
TEST(Codegen, TableWithNamespace) { DoTest(); }
TEST(Codegen, EnumType) { DoTest(); }

}  // namespace
}  // namespace fcp
