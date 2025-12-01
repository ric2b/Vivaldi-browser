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

#include "fcp/base/result.h"

#include <memory>
#include <utility>
#include <variant>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "fcp/base/tracing_schema.h"
#include "fcp/base/unique_value.h"
#include "fcp/testing/result_matchers.h"
#include "fcp/testing/testing.h"
#include "fcp/testing/tracing.h"
#include "fcp/tracing/test_tracing_recorder.h"

namespace fcp {

using ::testing::Eq;
using ::testing::VariantWith;

TEST(ResultTest, Constructor) {
  ASSERT_THAT(Result<int>(TraceTestError()), IsError());
  ASSERT_THAT(Result(123), HasValue(123));
}

TEST(ResultTest, CombinatorsToValue) {
  Result<bool> r = Result(123)
                       .Then([](int i) -> Result<bool> { return i != 123; })
                       .Map([](bool b) -> bool { return !b; });
  ASSERT_THAT(r, HasValue(true));
}

TEST(ResultTest, CombinatorsToValue_MoveOnly) {
  Result<bool> r =
      Result(UniqueValue(123))
          .Then([](UniqueValue<int> i) -> Result<UniqueValue<bool>> {
            return UniqueValue(std::move(i).Take() != 123);
          })
          .Map([](UniqueValue<bool> b) -> UniqueValue<bool> {
            return UniqueValue(!std::move(b).Take());
          })
          .Map([](UniqueValue<bool> b) -> bool { return std::move(b).Take(); });
  ASSERT_THAT(r, HasValue(true));
}

TEST(ResultTest, MapToValue_MoveOnly_Const) {
  Result<UniqueValue<int>> r1 = Result(UniqueValue(21));
  Result<int> r2 = r1.Map([](UniqueValue<int> const& v) { return (*v) * 2; });
  Result<bool> r1_still_valid =
      r1.Map([](UniqueValue<int> const& v) { return v.has_value(); });

  ASSERT_THAT(r2, HasValue(42));
  ASSERT_THAT(r1_still_valid, HasValue(true));
}

TEST(ResultTest, ThenToValue_MoveOnly_Const) {
  Result<UniqueValue<int>> r1 = Result(UniqueValue(21));
  Result<int> r2 =
      r1.Then([](UniqueValue<int> const& v) { return Result((*v) * 2); });
  Result<bool> r1_still_valid =
      r1.Then([](UniqueValue<int> const& v) { return Result(v.has_value()); });

  ASSERT_THAT(r2, HasValue(42));
  ASSERT_THAT(r1_still_valid, HasValue(true));
}

void ExpectUnreachable() { FAIL(); }

TEST(ResultTest, CombinatorsToError) {
  Result<Unit> r = Result(123)
                       .Then([](int i) -> Result<int> {
                         if (i > 0) {
                           return TraceTestError();
                         } else {
                           return i;
                         }
                       })
                       .Map([](int i) -> Unit {
                         ExpectUnreachable();
                         return Unit{};
                       });
  ASSERT_THAT(r, IsError());
}

TEST(ResultTest, ResultFrom) {
  static_assert(std::is_same_v<ResultFrom<Result<int>>, Result<int>>);
  static_assert(std::is_same_v<ResultFrom<int>, Result<int>>);
}

template <typename Expect, typename Fn, typename... Args>
constexpr Unit ExpectResultOf(Fn fn, Args... args) {
  using R = ResultOf<Fn, Args...>;
  static_assert(std::is_same_v<R, Expect>);
  return {};
}

namespace result_of_example {

Result<Unit> Result0() { return Unit{}; }
Result<Unit> Result1(int) { return Unit{}; }
Unit Value1(int i) { return Unit{}; }
constexpr auto Generic = [](auto t) { return Result(t); };

}  // namespace result_of_example

TEST(ResultTest, ResultOf) {
  static_assert(
      ExpectResultOf<Result<Unit>>(result_of_example::Result0).True());
  static_assert(
      ExpectResultOf<Result<Unit>>(result_of_example::Result1, 123).True());
  static_assert(
      ExpectResultOf<Result<Unit>>(result_of_example::Value1, 123).True());
  static_assert(
      ExpectResultOf<Result<bool>>(result_of_example::Generic, true).True());
}

Result<bool> Example_OneTryExpression(Result<int> r) {
  int i = FCP_TRY(r);
  if (i < 0) {
    return TraceTestError();
  }

  return i % 2 == 0;
}

TEST(ResultTest, TryExpressionWithError) {
  EXPECT_THAT(Example_OneTryExpression(TraceTestError()), IsError());
}

TEST(ResultTest, TryExpressionWithValue) {
  EXPECT_THAT(Example_OneTryExpression(-1), IsError());
  EXPECT_THAT(Example_OneTryExpression(1), HasValue(false));
  EXPECT_THAT(Example_OneTryExpression(2), HasValue(true));
}

Result<bool> Example_OneTryExpression_UnparenthesizedCommas(
    Result<std::variant<int, bool, Unit>> r) {
  std::variant<int, bool> v = FCP_TRY(r.Then(ExpectOneOf<int, bool>()));
  if (std::holds_alternative<int>(v)) {
    return std::get<int>(v) > 0;
  } else {
    return std::get<bool>(v);
  }
}

TEST(ResultTest, TryExpressionWithValue_UnparenthesizedCommas) {
  using V = std::variant<int, bool, Unit>;
  EXPECT_THAT(Example_OneTryExpression_UnparenthesizedCommas(V(-1)),
              HasValue(false));
  EXPECT_THAT(Example_OneTryExpression_UnparenthesizedCommas(V(1)),
              HasValue(true));
  EXPECT_THAT(Example_OneTryExpression_UnparenthesizedCommas(V(false)),
              HasValue(false));
  EXPECT_THAT(Example_OneTryExpression_UnparenthesizedCommas(V(true)),
              HasValue(true));
  EXPECT_THAT(Example_OneTryExpression_UnparenthesizedCommas(V(Unit{})),
              IsError());
}

Result<int> Example_SumWithTryExpressions(Result<int> a, Result<int> b) {
  return FCP_TRY(a) + FCP_TRY(b);
}

TEST(ResultTest, TwoTryExpressionsWithError) {
  EXPECT_THAT(Example_SumWithTryExpressions(TraceTestError(), 1), IsError());
  EXPECT_THAT(Example_SumWithTryExpressions(41, TraceTestError()), IsError());
  EXPECT_THAT(Example_SumWithTryExpressions(TraceTestError(), TraceTestError()),
              IsError());
}

TEST(ResultTest, TwoTryExpressionsWithValues) {
  EXPECT_THAT(Example_SumWithTryExpressions(1, 41), HasValue(42));
}

Result<int> Example_TryExpression_MoveOnly(Result<std::unique_ptr<int>> r) {
  std::unique_ptr<int> p = FCP_TRY(std::move(r));
  return *p;
}

TEST(ResultTest, TryExpressionWithError_MoveOnly) {
  EXPECT_THAT(Example_TryExpression_MoveOnly(TraceTestError()), IsError());
}

TEST(ResultTest, TryExpressionWithValue_MoveOnly) {
  EXPECT_THAT(Example_TryExpression_MoveOnly(std::make_unique<int>(123)),
              HasValue(123));
}

Result<bool> Example_TryStatement(Result<Unit> r) {
  FCP_TRY(r);
  return true;
}

TEST(ResultTest, TryStatementWithError) {
  EXPECT_THAT(Example_TryStatement(TraceTestError()), IsError());
}

TEST(ResultTest, TryStatementWithValue) {
  EXPECT_THAT(Example_TryStatement(Unit{}), HasValue(true));
}

TEST(ResultTest, ExpectTrue) {
  EXPECT_THAT(Result(true).Then(ExpectTrue()), HasValue(Unit{}));
  EXPECT_THAT(Result(false).Then(ExpectTrue()), IsError());
  EXPECT_THAT(Result<bool>(TraceTestError()).Then(ExpectTrue()), IsError());
}

TEST(ResultTest, ExpectFalse) {
  EXPECT_THAT(Result(false).Then(ExpectFalse()), HasValue(Unit{}));
  EXPECT_THAT(Result(true).Then(ExpectFalse()), IsError());
  EXPECT_THAT(Result<bool>(TraceTestError()).Then(ExpectFalse()), IsError());
}

TEST(ResultTest, ExpectHasValue) {
  using V = std::optional<int>;
  EXPECT_THAT(Result<V>(123).Then(ExpectHasValue()), HasValue(123));
  EXPECT_THAT(Result<V>(V{}).Then(ExpectHasValue()), IsError());
  EXPECT_THAT(Result<V>(TraceTestError()).Then(ExpectHasValue()), IsError());
}

TEST(ResultTest, ExpectIsEmpty) {
  using V = std::optional<int>;
  EXPECT_THAT(Result<V>(123).Then(ExpectIsEmpty()), IsError());
  EXPECT_THAT(Result<V>(V{}).Then(ExpectIsEmpty()), HasValue(Unit{}));
  EXPECT_THAT(Result<V>(TraceTestError()).Then(ExpectIsEmpty()), IsError());
}

TEST(ResultTest, ExpectIs) {
  using V = std::variant<int, char>;
  EXPECT_THAT(Result<V>(123).Then(ExpectIs<int>()), HasValue(123));
  EXPECT_THAT(Result<V>('a').Then(ExpectIs<char>()), HasValue('a'));
  EXPECT_THAT(Result<V>('b').Then(ExpectIs<int>()), IsError());
  EXPECT_THAT(Result<V>(TraceTestError()).Then(ExpectIs<int>()), IsError());
  EXPECT_THAT(Result<V>(TraceTestError()).Then(ExpectIs<char>()), IsError());
}

TEST(ResultTest, ExpectOneOf) {
  using V = std::variant<int, char, bool>;
  EXPECT_THAT(Result<V>(123).Then(ExpectOneOf<int>()),
              HasValue(VariantWith<int>(123)));
  EXPECT_THAT(Result<V>(123).Then(ExpectOneOf<bool>()), IsError());
  EXPECT_THAT((Result<V>(123).Then(ExpectOneOf<int, bool>())),
              HasValue(VariantWith<int>(123)));
  EXPECT_THAT((Result<V>(123).Then(ExpectOneOf<char, bool>())), IsError());
  EXPECT_THAT((Result<V>(TraceTestError()).Then(ExpectOneOf<int, bool>())),
              IsError());
}

TEST(ResultTest, ExpectOk) {
  TestTracingRecorder recorder;
  EXPECT_THAT(Result<Status>(FCP_STATUS(OK)).Then(ExpectOk()),
              HasValue(Unit{}));
}

TEST(ResultTest, ExpectOkReturnsError) {
  TestTracingRecorder recorder;
  recorder.ExpectError<ResultExpectStatusError>();
  EXPECT_THAT(Result<Status>(FCP_STATUS(INVALID_ARGUMENT)).Then(ExpectOk()),
              IsError());
}

TEST(ResultTest, ExpectOkStatusOr) {
  TestTracingRecorder recorder;
  EXPECT_THAT(Result<StatusOr<Unit>>(StatusOr<Unit>(Unit{})).Then(ExpectOk()),
              HasValue(Unit{}));
}

TEST(ResultTest, ExpectOkStatusOrReturnsError) {
  TestTracingRecorder recorder;
  recorder.ExpectError<ResultExpectStatusError>();
  EXPECT_THAT(
      Result<StatusOr<Unit>>(FCP_STATUS(INVALID_ARGUMENT)).Then(ExpectOk()),
      IsError());
  EXPECT_THAT(
      recorder.FindAllEvents<ResultExpectStatusError>(),
      ElementsAre(IsEvent<ResultExpectStatusError>(
          Eq(TracingStatusCode_Ok), Eq(TracingStatusCode_InvalidArgument))));
}

TEST(ResultTest, TraceFailedPrecondition) {
  TestTracingRecorder recorder;
  recorder.ExpectError<ResultExpectStatusError>();
  EXPECT_THAT(
      Result<StatusOr<Unit>>(FCP_STATUS(FAILED_PRECONDITION)).Then(ExpectOk()),
      IsError());
  EXPECT_THAT(
      recorder.FindAllEvents<ResultExpectStatusError>(),
      ElementsAre(IsEvent<ResultExpectStatusError>(
          Eq(TracingStatusCode_Ok), Eq(TracingStatusCode_FailedPrecondition))));
}

}  // namespace fcp
