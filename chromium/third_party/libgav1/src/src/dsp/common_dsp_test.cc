// Copyright 2023 The libgav1 Authors
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

#include "absl/strings/match.h"
#include "gtest/gtest.h"
#include "src/dsp/x86/common_avx2_test.h"
#include "src/dsp/x86/common_sse4_test.h"
#include "src/utils/cpu.h"

namespace libgav1 {
namespace dsp {
namespace {

class CommonDspTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    const char* const test_case = test_info->name();
    if (absl::StartsWith(test_case, "SSE41")) {
      if ((GetCpuInfo() & kSSE4_1) == 0) GTEST_SKIP() << "No SSE4.1 support!";
    } else if (absl::StartsWith(test_case, "AVX2")) {
      if ((GetCpuInfo() & kAVX2) == 0) GTEST_SKIP() << "No AVX2 support!";
    } else {
      FAIL() << "Unrecognized architecture prefix in test case name: "
             << test_case;
    }
  }
};

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CommonDspTest);

#if LIBGAV1_ENABLE_AVX2
TEST_F(CommonDspTest, AVX2RightShiftWithRoundingS16) {
  AVX2RightShiftWithRoundingS16Test();
}
#endif  // LIBGAV1_ENABLE_AVX2

#if LIBGAV1_ENABLE_SSE4_1
TEST_F(CommonDspTest, SSE41RightShiftWithRoundingS16) {
  SSE41RightShiftWithRoundingS16Test();
}
#endif  // LIBGAV1_ENABLE_SSE41

}  // namespace
}  // namespace dsp
}  // namespace libgav1
