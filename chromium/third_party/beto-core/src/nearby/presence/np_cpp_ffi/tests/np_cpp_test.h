// Copyright 2023 Google LLC
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

#ifndef NEARBY_PRESENCE_NP_CPP_FFI_TESTS_NP_CPP_TEST_H_
#define NEARBY_PRESENCE_NP_CPP_FFI_TESTS_NP_CPP_TEST_H_

#include "gtest/gtest.h"
#include "nearby_protocol.h"
#include "shared_test_util.h"

class NpCppTest : public testing::Test {
 protected:
  static void SetUpTestSuite() {
    if (!panic_handler_set) {
      ASSERT_TRUE(
          nearby_protocol::GlobalConfig::SetPanicHandler(test_panic_handler));
      panic_handler_set = true;
    } else {
      ASSERT_FALSE(
          nearby_protocol::GlobalConfig::SetPanicHandler(test_panic_handler));
    }
  }
  static bool panic_handler_set;
};

#endif  // NEARBY_PRESENCE_NP_CPP_FFI_TESTS_NP_CPP_TEST_H_
