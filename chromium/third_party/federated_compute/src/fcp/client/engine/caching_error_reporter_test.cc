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
#include "fcp/client/engine/caching_error_reporter.h"

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "tensorflow/lite/core/api/error_reporter.h"

namespace fcp {
namespace client {
namespace engine {
namespace {

using ::testing::IsEmpty;

TEST(CachingErrorReporterTest, CachingMultiple) {
  CachingErrorReporter reporter;
  std::string first_error = "Op a is not found.";
  TF_LITE_REPORT_ERROR(&reporter, "%s%d", first_error.c_str(), 1);
  std::string second_error = "Op b is not found.";
  TF_LITE_REPORT_ERROR(&reporter, "%s%d", second_error.c_str(), 2);
  EXPECT_THAT(reporter.GetFirstErrorMessage(), absl::StrCat(first_error, "1"));
}

TEST(CachingErrorReporterTest, Empty) {
  CachingErrorReporter reporter;
  EXPECT_THAT(reporter.GetFirstErrorMessage(), IsEmpty());
}

}  // anonymous namespace
}  // namespace engine
}  // namespace client
}  // namespace fcp
