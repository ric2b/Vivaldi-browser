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

#ifndef FCP_TESTING_RESULT_MATCHERS_H_
#define FCP_TESTING_RESULT_MATCHERS_H_

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "fcp/base/error.h"
#include "fcp/base/result.h"

namespace fcp {

// Allows to formulate test expectation on a result containing error as:
// EXPECT_THAT(result, IsError());
MATCHER(IsError, "") { return arg.is_error(); }

// Allows to formulate test expectation on a non-error result with existing
// gtest matchers (such as Eq) as:
// EXPECT_THAT(result, HasValue(Eq(value)));
template <typename MatcherType>
class HasValueMatcher {
 public:
  explicit HasValueMatcher(MatcherType matcher)
      : matcher_(std::move(matcher)) {}

  template <typename TargetType>
  operator testing::Matcher<TargetType>() const {  // NOLINT
    using D = std::remove_cv_t<std::remove_reference_t<TargetType>>;
    static_assert(result_internal::ResultTraits<D>::is_result());
    using V = typename result_internal::ResultTraits<D>::ValueType;
    return testing::Matcher<TargetType>(
        new Impl<V>(testing::SafeMatcherCast<V const&>(matcher_)));
  }

 private:
  template <typename ValueType>
  class Impl : public testing::MatcherInterface<Result<ValueType> const&> {
   public:
    explicit Impl(testing::Matcher<ValueType const&> matcher)
        : concrete_matcher_(std::move(matcher)) {}

    bool MatchAndExplain(
        Result<ValueType> const& arg,
        testing::MatchResultListener* result_listener) const override;

    void DescribeTo(std::ostream* os) const override {
      *os << FormatDescription(false);
    }

    void DescribeNegationTo(std::ostream* os) const override {
      *os << FormatDescription(true);
    }

   private:
    std::string FormatDescription(bool negation) const;
    testing::Matcher<ValueType const&> concrete_matcher_;
  };

  MatcherType matcher_;
};

template <typename MatcherType>
HasValueMatcher<MatcherType> HasValue(MatcherType matcher) {
  return HasValueMatcher<MatcherType>(std::move(matcher));
}

template <typename MatcherType>
template <typename ValueType>
bool HasValueMatcher<MatcherType>::Impl<ValueType>::MatchAndExplain(
    Result<ValueType> const& arg,
    testing::MatchResultListener* result_listener) const {
  if (arg.is_error()) {
    *result_listener << "is error";
    return false;
  } else {
    ValueType const& value = arg.GetValueOrDie();
    *result_listener << "value = " << testing::PrintToString(value);
    return testing::ExplainMatchResult(concrete_matcher_, value,
                                       result_listener);
  }
}

template <typename MatcherType>
template <typename ValueType>
std::string HasValueMatcher<MatcherType>::Impl<ValueType>::FormatDescription(
    bool negation) const {
  std::stringstream desc;
  if (negation) {
    concrete_matcher_.DescribeNegationTo(&desc);
  } else {
    concrete_matcher_.DescribeTo(&desc);
  }
  return desc.str();
}

// Expect a particular status for testing failure modes of protocols.
// Prefer ExpectOk (defined in result.h) for OK status.
template <fcp::StatusCode Code>
struct ExpectStatus : public ExpectBase {
  using ExpectBase::ExpectBase;
  constexpr explicit ExpectStatus(
      SourceLocation loc = SourceLocation::current())
      : ExpectBase(loc) {}

  Result<Unit> operator()(const Status& s) const {
    if (s.code() == Code) {
      return Unit{};
    } else {
      return TraceUnexpectedStatus(Code, s);
    }
  }
};

}  // namespace fcp

#endif  // FCP_TESTING_RESULT_MATCHERS_H_
