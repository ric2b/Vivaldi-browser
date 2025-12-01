/*
 * Copyright 2019 Google LLC
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

#include "fcp/testing/result_matchers.h"

#include "gtest/gtest.h"
#include "fcp/base/error.h"
#include "fcp/base/result.h"
#include "fcp/base/tracing_schema.h"
#include "fcp/testing/testing.h"
#include "fcp/testing/tracing.h"
#include "fcp/tracing/test_tracing_recorder.h"

namespace fcp {
using testing::Eq;
using testing::Not;

TEST(ExpectTest, HasValueDirect) {
  EXPECT_THAT(Result<int>(42), HasValue(42));
  EXPECT_THAT(Result<int>(42), Not(HasValue(24)));
  EXPECT_THAT(Result<std::string>("foo"), HasValue("foo"));
  EXPECT_THAT(Result<std::string>("foo"), Not(HasValue("bar")));
}

TEST(ExpectTest, HasValueEq) {
  EXPECT_THAT(Result<int>(42), HasValue(Eq(42)));
  EXPECT_THAT(Result<int>(42), Not(HasValue(Eq(24))));
  EXPECT_THAT(Result<int>(42), HasValue(Not(Eq(24))));
  EXPECT_THAT(Result<std::string>("foo"), HasValue(Eq("foo")));
  EXPECT_THAT(Result<std::string>("foo"), Not(HasValue(Eq("bar"))));
  EXPECT_THAT(Result<std::string>("foo"), HasValue(Not(Eq("bar"))));
}

TEST(ExpectTest, ExpectIsError) {
  EXPECT_THAT(Result<int>(TraceTestError()), IsError());
  EXPECT_THAT(Result<int>(42), Not(IsError()));
}

TEST(ExpectTest, ExpectStatus) {
  TestTracingRecorder recorder;
  EXPECT_THAT(Result<Status>(FCP_STATUS(INVALID_ARGUMENT))
                  .Then(ExpectStatus<INVALID_ARGUMENT>()),
              HasValue(Unit{}));
}

TEST(ExpectTest, ExpectStatusReturnsError) {
  TestTracingRecorder recorder;
  recorder.ExpectError<ResultExpectStatusError>();
  EXPECT_THAT(
      Result<Status>(FCP_STATUS(OK)).Then(ExpectStatus<INVALID_ARGUMENT>()),
      IsError());
}

}  // namespace fcp
