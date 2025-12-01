// Copyright 2020 Google LLC
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

#include <fstream>
#include <memory>
#include <string>

#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "fcp/base/monitoring.h"
#include "fcp/base/platform.h"
#include "fcp/base/scheduler.h"
#include "fcp/testing/testing.h"
#include "fcp/tracing/scoped_tracing_recorder.h"
#include "fcp/tracing/test/tracing_schema.h"
#include "fcp/tracing/text_tracing_recorder.h"
#include "re2/re2.h"

constexpr char kBaselineDir[] = "fcp/tracing/test/testdata";

namespace fcp {
namespace {

// Replaces timestamp with ${TIME} and span ID with ${ID} in text trace output.
// Span IDs need to be replaced because of the lack of determinism in running
// multiple threads in parallel.
inline bool PostProcessOutput(std::string* input) {
  RE2 timestamp_and_id_pattern("\\d{4}-\\d{2}-\\d{2}T[[:^blank:]]*\\s\\d+");
  return RE2::GlobalReplace(input, timestamp_and_id_pattern, "${TIME} ${ID}") >
         0;
}

std::string GetOutFileName(int id) {
  return ConcatPath(testing::TempDir(), absl::StrCat(TestName(), id, ".out"));
}

std::string GetBaselineFileName(int id) {
  return ConcatPath(kBaselineDir, absl::StrCat(TestName(), id, ".baseline"));
}

absl::StatusOr<std::string> VerifyAgainstBaseline(int id) {
  // Reading out file
  std::string report = ReadFileToString(GetOutFileName(id)).value();
  EXPECT_TRUE(PostProcessOutput(&report));
  // Producing report which is expected to precisely match .baseline file.
  std::ostringstream expected;
  expected << "" << '\n';

  // Compare produced report with baseline.
  std::string baseline_path = GetBaselineFileName(id);
  return ::fcp::VerifyAgainstBaseline(baseline_path, report);
}

// Verifies that thread local tracing recorder can be changed on the same
// thread.
TEST(Tracing, ChangeThreadLocal) {
  const int kCount = 2;
  for (int i = 0; i < kCount; i++) {
    const int id = i + 1;
    TextTracingRecorder local_recorder(GetOutFileName(id), absl::UTCTimeZone());
    ScopedTracingRecorder scoped_recorder(&local_recorder);
    TracingSpan<SpanWithId> inner(id);
  }

  for (int i = 0; i < kCount; i++) {
    const int id = i + 1;
    auto status_s = VerifyAgainstBaseline(id);
    ASSERT_TRUE(status_s.ok()) << status_s.status();
    auto& diff = status_s.value();
    if (!diff.empty()) {
      FAIL() << diff;
    }
  }
}

TEST(Tracing, PerThread) {
  const int kThreadCount = 2;
  auto scheduler = CreateThreadPoolScheduler(kThreadCount);

  for (int i = 0; i < kThreadCount; i++) {
    scheduler->Schedule([&, i]() {
      const int id = i + 1;
      TextTracingRecorder local_recorder(GetOutFileName(id),
                                         absl::UTCTimeZone());
      ScopedTracingRecorder scoped_recorder(&local_recorder);
      TracingSpan<SpanWithId> inner(id);
      for (int k = 0; k < 5; k++) {
        absl::SleepFor(absl::Milliseconds(10));
        Trace<EventFoo>(id * 11, id * 111);
      }
    });
  }

  scheduler->WaitUntilIdle();

  for (int i = 0; i < kThreadCount; i++) {
    const int id = i + 1;
    auto status_s = VerifyAgainstBaseline(id);
    ASSERT_TRUE(status_s.ok()) << status_s.status();
    auto& diff = status_s.value();
    if (!diff.empty()) {
      FAIL() << diff;
    }
  }
}

TEST(Tracing, UninstallRequired) {
  auto local_recorder =
      std::make_shared<TextTracingRecorder>(absl::UTCTimeZone());
  local_recorder->InstallAsThreadLocal();
  ASSERT_DEATH(
      local_recorder.reset(),
      "Trace recorder must not be set as thread local at destruction time");
  // Note that ASSERT_DEATH statement above runs in a separate process so it is
  // still OK to uninstall the trace recorder here; otherwise this process
  // would crash too on destruction of the trace recorder.
  local_recorder->UninstallAsThreadLocal();
}

// Tests that setting the same tracing recorder is OK and that the number of
// InstallAsThreadLocal and UninstallAsThreadLocal must be matching.
TEST(Tracing, ReentrancySuccess) {
  auto local_recorder =
      std::make_shared<TextTracingRecorder>(absl::UTCTimeZone());
  local_recorder->InstallAsThreadLocal();
  local_recorder->InstallAsThreadLocal();
  local_recorder->UninstallAsThreadLocal();
  local_recorder->UninstallAsThreadLocal();
}

// Verifies that not matching the number of InstallAsThreadLocal with
// UninstallAsThreadLocal results in a failure.
TEST(Tracing, ReentrancyFailure) {
  auto local_recorder =
      std::make_shared<TextTracingRecorder>(absl::UTCTimeZone());
  local_recorder->InstallAsThreadLocal();
  // This simulates re-entracy by setting the same tracing recorder as
  // thread local again.
  local_recorder->InstallAsThreadLocal();
  local_recorder->UninstallAsThreadLocal();
  // At this point UninstallAsThreadLocal has been called only once, which isn't
  // sufficient.
  ASSERT_DEATH(
      local_recorder.reset(),
      "Trace recorder must not be set as thread local at destruction time");
  // Note that ASSERT_DEATH statement above runs in a separate process so it is
  // still necessary to uninstall the trace recorder here to make sure that
  // the test doesn't crash in the main test process.
  local_recorder->UninstallAsThreadLocal();
}

// Test that changing per-thread tracing recorder isn't allowed without
// uninstalling first.
TEST(Tracing, ChangingThreadLocalRecorderFails) {
  TextTracingRecorder local_recorder1(absl::UTCTimeZone());
  TextTracingRecorder local_recorder2(absl::UTCTimeZone());
  local_recorder1.InstallAsThreadLocal();
  ASSERT_DEATH(local_recorder2.InstallAsThreadLocal(),
               "Only one tracing recorder instance per thread is supported");
  // Note that ASSERT_DEATH statement above runs in a separate process so
  // uninstalling local_recorder1 is still needed in the main test process.
  local_recorder1.UninstallAsThreadLocal();
}

TEST(Tracing, UninstallingWrongThreadLocalRecorderFails) {
  TextTracingRecorder local_recorder1(absl::UTCTimeZone());
  TextTracingRecorder local_recorder2(absl::UTCTimeZone());
  local_recorder1.InstallAsThreadLocal();
  ASSERT_DEATH(local_recorder2.UninstallAsThreadLocal(),
               "Attempting to uninstall thread local tracing recorder that "
               "isn't currently installed");
  // Note that ASSERT_DEATH statement above runs in a separate process so
  // uninstalling local_recorder1 is still needed in the main test process.
  local_recorder1.UninstallAsThreadLocal();
}

}  // namespace
}  // namespace fcp
