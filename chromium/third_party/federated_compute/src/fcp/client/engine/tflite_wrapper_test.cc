/*
 * Copyright 2021 Google LLC
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
#include "fcp/client/engine/tflite_wrapper.h"

#include <cstdint>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/client/test_helpers.h"
#include "fcp/testing/testing.h"
#include "tensorflow/core/platform/tstring.h"

namespace fcp {
namespace client {
namespace engine {
namespace {

const absl::string_view kAssetsPath = "fcp/client/engine/data/";
const absl::string_view kJoinModelFile = "join_model.flatbuffer";

const int32_t kNumThreads = 4;

class RunTfLiteModelThreadSafeTest : public testing::Test {
 protected:
  absl::StatusOr<std::string> ReadFileAsString(const std::string& path) {
    std::ifstream input_istream(path);
    if (!input_istream) {
      return absl::InternalError("Failed to create input stream.");
    }
    std::stringstream output_stream;
    output_stream << input_istream.rdbuf();
    return output_stream.str();
  }

  MockLogManager mock_log_manager_;
  InterruptibleRunner::TimingConfig default_timing_config_ =
      InterruptibleRunner::TimingConfig{
          .polling_period = absl::Milliseconds(1000),
          .graceful_shutdown_period = absl::Milliseconds(1000),
          .extended_shutdown_period = absl::Milliseconds(2000),
      };
  std::vector<std::string> output_names_ = {"Identity"};
  TfLiteInterpreterOptions options_ = {
      .ensure_dynamic_tensors_are_released = true,
      .large_tensor_threshold_for_dynamic_allocation = 1000};
};

TEST_F(RunTfLiteModelThreadSafeTest, InvalidModel) {
  EXPECT_THAT(
      RunTfLiteModelThreadSafe(
          "INVALID_FLATBUFFER", []() { return false; }, default_timing_config_,
          &mock_log_manager_,
          std::make_unique<absl::flat_hash_map<std::string, std::string>>(),
          output_names_, options_, kNumThreads),
      IsCode(INVALID_ARGUMENT));
}

TEST_F(RunTfLiteModelThreadSafeTest, InputNotSet) {
  auto plan = ReadFileAsString(absl::StrCat(kAssetsPath, kJoinModelFile));
  ASSERT_OK(plan);
  // The plan that we use here join two strings. It requires two string tensors
  // as input.  We didn't pass the required tensor, therefore, we expect an
  // internal error to be thrown.
  EXPECT_THAT(
      RunTfLiteModelThreadSafe(
          *plan, []() { return false; }, default_timing_config_,
          &mock_log_manager_,
          std::make_unique<absl::flat_hash_map<std::string, std::string>>(),
          output_names_, options_, kNumThreads),
      IsCode(INVALID_ARGUMENT));
}

TEST_F(RunTfLiteModelThreadSafeTest, WrongNumberOfOutputs) {
  auto plan = ReadFileAsString(absl::StrCat(kAssetsPath, kJoinModelFile));
  ASSERT_OK(plan);
  // The plan that we use here join two strings. It requires two string tensors
  // as input.  We didn't pass the required tensor, therefore, we expect an
  // internal error to be thrown.
  EXPECT_THAT(
      RunTfLiteModelThreadSafe(
          *plan, []() { return false; }, default_timing_config_,
          &mock_log_manager_,
          std::make_unique<absl::flat_hash_map<std::string, std::string>>(),
          {"Identity", "EXTRA"}, options_, kNumThreads),
      IsCode(INVALID_ARGUMENT));
}

TEST_F(RunTfLiteModelThreadSafeTest, Aborted) {
  auto plan = ReadFileAsString(absl::StrCat(kAssetsPath, kJoinModelFile));
  ASSERT_OK(plan);
  auto inputs =
      std::make_unique<absl::flat_hash_map<std::string, std::string>>();
  (*inputs)["x"] = "abc";
  (*inputs)["y"] = "def";
  // The should_abort function is set to always return true, therefore we expect
  // to see a CANCELLED status when we run the plan.
  EXPECT_THAT(RunTfLiteModelThreadSafe(
                  *plan, []() { return true; }, default_timing_config_,
                  &mock_log_manager_, std::move(inputs), output_names_,
                  options_, kNumThreads),
              IsCode(CANCELLED));
}

TEST_F(RunTfLiteModelThreadSafeTest, Success) {
  auto plan = ReadFileAsString(absl::StrCat(kAssetsPath, kJoinModelFile));
  ASSERT_OK(plan);
  auto inputs =
      std::make_unique<absl::flat_hash_map<std::string, std::string>>();
  (*inputs)["x"] = "abc";
  (*inputs)["y"] = "def";
  auto outputs = RunTfLiteModelThreadSafe(
      *plan, []() { return false; }, default_timing_config_, &mock_log_manager_,
      std::move(inputs), output_names_, options_, kNumThreads);
  ASSERT_OK(outputs);
  EXPECT_EQ(outputs->output_tensor_names.size(), 1);
  EXPECT_EQ(
      *static_cast<tensorflow::tstring*>(outputs->output_tensors.at(0).data()),
      "abcdef");
}

}  // anonymous namespace
}  // namespace engine
}  // namespace client
}  // namespace fcp
