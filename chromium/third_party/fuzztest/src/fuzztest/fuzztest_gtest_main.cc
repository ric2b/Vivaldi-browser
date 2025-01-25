// Copyright 2022 Google LLC
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

#include <vector>

#include "gtest/gtest.h"
#include "absl/strings/match.h"
#include "./fuzztest/init_fuzztest.h"

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  // We call fuzztest::ParseAbslFlagsrather than absl::ParseCommandLine
  // since the latter would complain about any unknown flags that need
  // to be passed to legacy fuzzing engines (e.g. libfuzzer).
  fuzztest::ParseAbslFlags(argc, argv);
  fuzztest::InitFuzzTest(&argc, &argv);
  return RUN_ALL_TESTS();
}
