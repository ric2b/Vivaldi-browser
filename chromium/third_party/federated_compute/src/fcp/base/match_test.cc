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

#include "fcp/base/match.h"

#include <memory>
#include <optional>
#include <variant>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "fcp/base/result.h"
#include "fcp/testing/result_matchers.h"
#include "fcp/testing/testing.h"
#include "fcp/testing/tracing.h"

namespace fcp {

namespace {

using ::testing::Eq;
using ::testing::Optional;

struct X {
  int x;
};

struct Y {
  int y;
};

struct Z {
  int z;
};

using V = std::variant<X, Y, Z>;

using VMoveOnly =
    std::variant<std::unique_ptr<X>, std::unique_ptr<Y>, std::unique_ptr<Z>>;

TEST(MatchTest, AllDefault) {
  constexpr auto matcher =
      MakeMatcher<V>([](Default, V const&) { return 1; });

  static_assert(matcher.Match(X{}) == 1);
  static_assert(matcher.Match(Z{}) == 1);
  static_assert(matcher.Match(Y{}) == 1);
}

TEST(MatchTest, SingleDefault) {
  constexpr auto matcher =
      MakeMatcher<V>([](X const& x) { return 10 + x.x; },  //
                     [](Z const& z) { return 20 + z.z; },
                     [](Default, V const& v) { return 30 + std::get<Y>(v).y; });
  static_assert(matcher.Match(X{1}) == 11);
  static_assert(matcher.Match(Z{2}) == 22);
  static_assert(matcher.Match(Y{3}) == 33);
}

TEST(MatchTest, SingleDefault_Pointer) {
  constexpr auto matcher =
      MakeMatcher<V>([](X* x) { return 10 + x->x; },  //
                     [](Z* z) { return 20 + z->z; },
                     [](Default, V* v) { return 30 + std::get<Y>(*v).y; });

  V x = X{1};
  V z = Z{2};
  V y = Y{3};

  EXPECT_THAT(matcher.Match(&x), Eq(11));
  EXPECT_THAT(matcher.Match(&z), Eq(22));
  EXPECT_THAT(matcher.Match(&y), Eq(33));
}

TEST(MatchTest, Exhaustive) {
  constexpr auto matcher = MakeMatcher<V>(
      [](X const& x) { return 10 + x.x; }, [](Z const& z) { return 20 + z.z; },
      [](Y const& y) { return 30 + y.y; });
  static_assert(matcher.Match(X{1}) == 11);
  static_assert(matcher.Match(Z{2}) == 22);
  static_assert(matcher.Match(Y{3}) == 33);
}

TEST(MatchTest, Exhaustive_Pointer) {
  constexpr auto matcher = MakeMatcher<V>([](X* x) { return 10 + x->x; },
                                          [](Z* z) { return 20 + z->z; },
                                          [](Y* y) { return 30 + y->y; });

  V x = X{1};
  V z = Z{2};
  V y = Y{3};

  EXPECT_THAT(matcher.Match(&x), Eq(11));
  EXPECT_THAT(matcher.Match(&z), Eq(22));
  EXPECT_THAT(matcher.Match(&y), Eq(33));
}

TEST(MatchTest, Exhaustive_MatchInsteadOfMatcher) {
  constexpr auto do_match = [](V const& v) {
    return Match(
        v,  //
        [](X const& x) { return 10 + x.x; },
        [](Z const& z) { return 20 + z.z; },
        [](Y const& y) { return 30 + y.y; });
  };

  static_assert(do_match(X{1}) == 11);
  static_assert(do_match(Z{2}) == 22);
  static_assert(do_match(Y{3}) == 33);
}

TEST(MatchTest, CoerceViaExplicitReturnType) {
  constexpr auto do_match = [](V const& v) {
    return Match<std::optional<int>>(
        v,  //
        [](X const& x) { return 10 + x.x; },
        [](Z const& z) { return 20 + z.z; },
        [](Y const& y) { return std::nullopt; });
  };

  static_assert(*do_match(X{1}) == 11);
  static_assert(*do_match(Z{2}) == 22);
  static_assert(!do_match(Y{3}).has_value());
}

TEST(MatchTest, MoveOnly_Borrow_Exhaustive) {
  constexpr auto matcher = MakeMatcher<VMoveOnly>(
      [](std::unique_ptr<X> const& x) { return 10 + x->x; },
      [](std::unique_ptr<Z> const& z) { return 20 + z->z; },
      [](std::unique_ptr<Y> const& y) { return 30 + y->y; });

  VMoveOnly v_x = std::make_unique<X>(X{1});
  VMoveOnly v_z = std::make_unique<Z>(Z{2});
  VMoveOnly v_y = std::make_unique<Y>(Y{3});

  EXPECT_THAT(matcher.Match(v_x), Eq(11));
  EXPECT_THAT(matcher.Match(v_z), Eq(22));
  EXPECT_THAT(matcher.Match(v_y), Eq(33));
}

TEST(MatchTest, MoveOnly_Consume_Exhaustive) {
  constexpr auto matcher = MakeMatcher<VMoveOnly>(
      [](std::unique_ptr<X> x) { return 10 + x->x; },
      [](std::unique_ptr<Z> z) { return 20 + z->z; },
      [](std::unique_ptr<Y> y) { return 30 + y->y; });

  VMoveOnly v_x = std::make_unique<X>(X{1});
  VMoveOnly v_z = std::make_unique<Z>(Z{2});
  VMoveOnly v_y = std::make_unique<Y>(Y{3});

  EXPECT_THAT(matcher.Match(std::move(v_x)), Eq(11));
  EXPECT_THAT(matcher.Match(std::move(v_z)), Eq(22));
  EXPECT_THAT(matcher.Match(std::move(v_y)), Eq(33));
}

// std::optional is handled with a special MatchTraits implementation.
// The corresponding std::variant has to be synthesized on the fly, so that
// implementation is trickier than usual.

TEST(MatchTest, Optional_Ref) {
  using O = std::optional<std::unique_ptr<X>>;
  constexpr auto matcher =
      MakeMatcher<O>([](std::unique_ptr<X> const& x) { return x->x; },
                     [](std::nullopt_t) { return 0; });

  O const engaged = std::make_unique<X>(X{123});
  O const empty = std::nullopt;

  EXPECT_THAT(matcher.Match(engaged), Eq(123));
  EXPECT_THAT(matcher.Match(empty), Eq(0));
}

TEST(MatchTest, Optional_Pointer) {
  using O = std::optional<std::unique_ptr<X>>;
  constexpr auto matcher = MakeMatcher<O>(
      [](std::unique_ptr<X>* x) {
        int v = (*x)->x;
        x->reset();
        return v;
      },
      [](std::nullopt_t) { return 0; });

  O engaged = std::make_unique<X>(X{123});
  O empty = std::nullopt;

  EXPECT_THAT(matcher.Match(&engaged), Eq(123));
  EXPECT_THAT(engaged, Optional(Eq(nullptr)));
  EXPECT_THAT(matcher.Match(&empty), Eq(0));
}

TEST(MatchTest, Optional_Consume) {
  using O = std::optional<std::unique_ptr<X>>;
  constexpr auto matcher =
      MakeMatcher<O>([](std::unique_ptr<X> x) { return x->x; },
                     [](std::nullopt_t) { return 0; });

  EXPECT_THAT(matcher.Match(O{std::make_unique<X>(X{123})}), Eq(123));
  EXPECT_THAT(matcher.Match(O{std::nullopt}), Eq(0));
}

// Result<T> uses the extensibility mechanism provided by MatchTrait
// (VariantType alias and a variant() accessor). These tests demonstrate that
// MatchTraits is extensible (in addition to testing the particular
// implementation for Result).

TEST(MatchTest, Result_Ref) {
  using R = Result<std::unique_ptr<X>>;
  constexpr auto matcher =
      MakeMatcher<R>([](std::unique_ptr<X> const& x) { return x->x; },
                     [](Error) { return 0; });

  R const val = std::make_unique<X>(X{123});
  R const err = TraceTestError();

  EXPECT_THAT(matcher.Match(val), Eq(123));
  EXPECT_THAT(matcher.Match(err), Eq(0));
}

TEST(MatchTest, Result_Pointer) {
  using R = Result<std::unique_ptr<X>>;
  constexpr auto matcher = MakeMatcher<R>(
      [](std::unique_ptr<X>* x) {
        int v = (*x)->x;
        x->reset();
        return v;
      },
      [](Error*) { return 0; });

  R val = std::make_unique<X>(X{123});
  R err = TraceTestError();

  EXPECT_THAT(matcher.Match(&val), Eq(123));
  EXPECT_THAT(val, HasValue(Eq(nullptr)));
  EXPECT_THAT(matcher.Match(&err), Eq(0));
}

TEST(MatchTest, Result_Consume) {
  using R = Result<std::unique_ptr<X>>;
  constexpr auto matcher = MakeMatcher<R>(
      [](std::unique_ptr<X> x) { return x->x; }, [](Error) { return 0; });

  EXPECT_THAT(matcher.Match(R(std::make_unique<X>(X{123}))), Eq(123));
  EXPECT_THAT(matcher.Match(R(TraceTestError())), Eq(0));
}

}  // namespace

}  // namespace fcp
