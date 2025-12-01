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

#include <fstream>
#include <string>

#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "fcp/base/monitoring.h"
#include "fcp/base/platform.h"
#include "fcp/testing/testing.h"
#include "fcp/tracing/test/tracing_schema.h"
#include "fcp/tracing/text_tracing_recorder.h"
#include "re2/re2.h"

namespace fcp {
namespace {

constexpr char kBaselineDir[] = "fcp/tracing/test/testdata";

bool PostProcessOutput(std::string* input) {
  RE2 timestamp_pattern("\\d{4}-\\d{2}-\\d{2}T[[:^blank:]]*");
  return RE2::GlobalReplace(input, timestamp_pattern, "${TIME}") > 0;
}

TEST(Tracing, Basic) {
  std::string out_file =
      ConcatPath(testing::TempDir(), absl::StrCat(TestName(), ".out"));
  {
    TextTracingRecorder p(out_file, absl::UTCTimeZone());
    p.InstallAsGlobal();
    Trace<EventFoo>(10, 20);
    {
      TracingSpan<SpanWithId> inner(111);
      Trace<EventFoo>(222, 333);
      auto ignored = TraceError<ErrorEvent>("Oops!");
      (void)ignored;
      {
        TracingSpan<SpanWithId> very_inner(999);
        Trace<EventFoo>(555, 666);
      }
    }
    {
      TracingSpan<SpanWithNoData> inner;
      Trace<EventWithNoData>();
    }
    {
      auto long_running_span =
          std::make_unique<UnscopedTracingSpan<SpanWithNoData>>(
              TracingSpanRef::Top());
      TracingSpan<SpanWithId> foo_inner(long_running_span->Ref(), 222);
      Trace<EventBar>(333, "Hello world!");
    }
  }

  // Reading out file
  std::string report = ReadFileToString(out_file).value();
  ASSERT_TRUE(PostProcessOutput(&report));
  // Producing report which is expected to precisely match .baseline file.
  std::ostringstream expected;
  expected << "" << '\n';

  // Compare produced report with baseline.
  std::string baseline_path =
      ConcatPath(kBaselineDir, absl::StrCat(TestName(), ".baseline"));
  auto status_s = VerifyAgainstBaseline(baseline_path, report);
  ASSERT_TRUE(status_s.ok()) << status_s.status();
  auto& diff = status_s.value();
  if (!diff.empty()) {
    FAIL() << diff;
  }
}

TEST(Tracing, TimestampReplace) {
  std::string timestamp = "2019-10-24T22:07:07.916321247+00:00";
  ASSERT_TRUE(PostProcessOutput(&timestamp));
  ASSERT_EQ(timestamp, "${TIME}");
}

TEST(Tracing, DefaultProvider) {
  // This just triggers default stderr logging codepath, without verifying it
  Trace<EventBar>(444, "Hello world!");
  TracingSpan<SpanWithId> inner(111);
  Trace<EventFoo>(222, 333);
}

}  // namespace
}  // namespace fcp
