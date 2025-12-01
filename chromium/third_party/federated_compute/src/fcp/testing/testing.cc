/*
 * Copyright 2017 Google LLC
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

#include "fcp/testing/testing.h"

#include <stdio.h>
#include <stdlib.h>

#include <filesystem>
#include <string>

#include "gtest/gtest.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "fcp/base/base_name.h"
#include "fcp/base/monitoring.h"
#include "fcp/base/platform.h"

namespace fcp {

std::string TestName() {
  auto test_info = testing::UnitTest::GetInstance()->current_test_info();
  return absl::StrReplaceAll(test_info->name(), {{"/", "_"}});
}

std::string TestCaseName() {
  auto test_info = testing::UnitTest::GetInstance()->current_test_info();
  return absl::StrReplaceAll(test_info->test_case_name(), {{"/", "_"}});
}

std::string GetTestDataPath(absl::string_view relative_path) {
  auto env = getenv("TEST_SRCDIR");
  std::string test_srcdir = env ? env : "";
  return ConcatPath(test_srcdir, ConcatPath("com_google_fcp", relative_path));
}

std::string TemporaryTestFile(absl::string_view suffix) {
  return ConcatPath(StripTrailingPathSeparator(testing::TempDir()),
                    absl::StrCat(TestName(), suffix));
}

namespace {

absl::Status EnsureDirExists(absl::string_view path) {
  if (FileExists(path)) {
    return absl::OkStatus();
  }
  auto path_str = std::string(path);
  int error;
#ifndef _WIN32
  error = mkdir(path_str.c_str(), 0733);
#else
  error = _mkdir(path_str.c_str());
#endif
  if (error) {
    return absl::InternalError(absl::StrCat(
        "cannot create directory ", path_str, "(error code ", error, ")"));
  }
  return absl::OkStatus();
}

}  // namespace

bool ShouldUpdateBaseline() {
  return getenv("FCP_UPDATE_BASELINE");
}

namespace {

std::string MakeTempFileName() {
#ifdef __APPLE__
// Apple has marked tmpnam as deprecated. As we are compiling with -Werror,
// turning this off for this case. Apple recommends to use mkstemp instead,
// but because this opens a file, it's not exactly what we want, and it's not
// portable. std::filesystem in C++17 should fix this issue.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
  return tmpnam(nullptr);
#ifdef __APPLE__
#pragma clang diagnostic pop
#endif
}

absl::Status ShellCommand(absl::string_view command, std::string* stdout_result,
                          std::string* stderr_result) {
#ifdef _WIN32
  return absl::UnimplementedError("ShellCommand not implemented for Windows");
#else
  // Prepare command for output redirection.
  std::string command_str = std::string(command);
  std::string stdout_file;
  if (stdout_result != nullptr) {
    stdout_file = MakeTempFileName();
    absl::StrAppend(&command_str, " 1>", stdout_file);
  }
  std::string stderr_file;
  if (stderr_result != nullptr) {
    stderr_file = MakeTempFileName();
    absl::StrAppend(&command_str, " 2>", stderr_file);
  }

  // Call the command.
  int result = std::system(command_str.c_str());

  // Read and remove redirected output.
  if (stdout_result != nullptr) {
    auto status_or_result = ReadFileToString(stdout_file);
    if (status_or_result.ok()) {
      *stdout_result = status_or_result.value();
      std::remove(stdout_file.c_str());
    } else {
      *stdout_result = "";
    }
  }
  if (stderr_result != nullptr) {
    auto status_or_result = ReadFileToString(stderr_file);
    if (status_or_result.ok()) {
      *stderr_result = status_or_result.value();
      std::remove(stderr_file.c_str());
    } else {
      *stderr_result = "";
    }
  }

  // Construct result.
  if (result != 0) {
    return absl::InternalError(absl::StrCat(
        "command execution failed: ", command_str, " returns ", result));
  } else {
    return absl::OkStatus();
  }
#endif
}

}  // namespace

absl::StatusOr<std::string> ComputeDiff(absl::string_view baseline_file,
                                        absl::string_view content) {
  std::string diff_result;
  std::string baseline_file_str = GetTestDataPath(baseline_file);
  if (!FileExists(baseline_file_str)) {
    diff_result = absl::StrCat("no recorded baseline file ", baseline_file_str);
  } else {
#ifndef _WIN32
    // Expect Unix diff command to be available.
    auto provided_file = TemporaryTestFile(".provided");
    auto status = WriteStringToFile(provided_file, content);
    if (!status.ok()) {
      return status;
    }
    std::string std_out, std_err;
    status = ShellCommand(
        absl::StrCat("diff -u ", baseline_file_str, " ", provided_file),
        &std_out, &std_err);
    std::remove(provided_file.c_str());
    if (status.code() != OK) {
      if (!std_err.empty()) {
        // Indicates a failure in diff execution itself.
        return absl::InternalError(absl::StrCat("command failed: ", std_err));
      }
      diff_result = std_out;
    }
#else  // _WIN32
    // For now we do a simple string compare on Windows.
    auto status_or_string = ReadFileToString(baseline_file_str);
    if (!status_or_string.ok()) {
      return status_or_string.status();
    }
    if (status_or_string.value() != content) {
      diff_result = "baseline and actual differ (see respective files)";
    }
#endif
  }
  return diff_result;
}

StatusOr<std::string> VerifyAgainstBaseline(absl::string_view baseline_file,
                                            absl::string_view content) {
  auto status_or_diff_result = ComputeDiff(baseline_file, content);
  if (!status_or_diff_result.ok()) {
    return status_or_diff_result;
  }
  auto& diff_result = status_or_diff_result.value();
  if (diff_result.empty()) {
    // success
    return status_or_diff_result;
  }

  // Determine the location where to store the new baseline.
  std::string new_baseline_file;
  bool auto_update = false;

  if (new_baseline_file.empty() && ShouldUpdateBaseline()) {
    new_baseline_file = GetTestDataPath(baseline_file);
    diff_result =
        absl::StrCat("\nAutomatically updated baseline file: ", baseline_file);
    auto_update = true;
  }

  if (new_baseline_file.empty()) {
    // Store new baseline file in a TMP location.
#ifndef _WIN32
    const char* temp_dir = "/tmp";
#else
    const char* temp_dir = getenv("TEMP");
#endif
    auto temp_output_dir =
        ConcatPath(temp_dir, absl::StrCat("fcp_", TestCaseName()));
    FCP_CHECK_STATUS(EnsureDirExists(temp_output_dir));
    new_baseline_file = ConcatPath(temp_output_dir, BaseName(baseline_file));
    absl::StrAppend(&diff_result, "\nNew baseline file: ", new_baseline_file);
    absl::StrAppend(&diff_result, "\nTo update, use:");
    absl::StrAppend(&diff_result, "\n\n cp ", new_baseline_file, " ",
                    baseline_file, "\n");
  }

  if (!auto_update) {
    absl::StrAppend(&diff_result,
                    "\nTo automatically update baseline files, use");
    absl::StrAppend(&diff_result,
                    "\nenvironment variable FCP_UPDATE_BASELINE.");
  }

  // Write the new baseline.
  auto status = WriteStringToFile(new_baseline_file, content);
  if (!status.ok()) {
    return status;
  }

  // Deliver result.
  if (auto_update) {
    FCP_LOG(INFO) << diff_result;
    diff_result = "";  // make test pass
  }
  return diff_result;
}

StatusMatcher IsCode(StatusCode code) { return StatusMatcher(code); }
StatusMatcher IsOk() { return IsCode(OK); }

}  // namespace fcp
