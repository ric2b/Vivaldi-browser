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

#include "fcp/base/meta.h"

#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace fcp {

using ::testing::Eq;
using ::testing::Not;

struct R {
  virtual ~R() = default;
  virtual bool Virt1(int i) = 0;
  virtual bool Virt2(int i, int j) = 0;
  virtual void Virt3() = 0;
  int NonVirt1() { return 1; }
  int NonVirt2() { return 2; }
  char field;
};

struct S {
  virtual ~S() = default;
  virtual bool Virt1(int i) = 0;
  virtual bool Virt2(int i, int j) = 0;
  virtual void Virt3() = 0;
  int NonVirt1() { return 1; }
  int NonVirt2() { return 2; }
  char field;
};

//
// Compile-time tests for MemberPointerTraits
//
#define STATIC_ASSERT_TARGET_TYPE(member, type)                           \
  static_assert(                                                          \
      std::is_same<MemberPointerTraits<decltype(&S::member)>::TargetType, \
                   type>::value,                                          \
      "Incorrect target type from MemberPointerTraits");

// For some reason the linter otherwise thinks e.g. 'bool(int)' is an old-style
// cast.
template<typename T>
struct Id { using Type = T; };

STATIC_ASSERT_TARGET_TYPE(Virt1, Id<bool(int)>::Type);
STATIC_ASSERT_TARGET_TYPE(Virt2, Id<bool(int, int)>::Type);
STATIC_ASSERT_TARGET_TYPE(Virt3, Id<void()>::Type);
STATIC_ASSERT_TARGET_TYPE(NonVirt1, Id<int()>::Type);
STATIC_ASSERT_TARGET_TYPE(NonVirt2, Id<int()>::Type);
STATIC_ASSERT_TARGET_TYPE(field, Id<char>::Type);
//
// End compile-time tests for MemberPointerTraits
//

template<typename T>
struct TypeIdHolder {
  static constexpr char kTarget = '\0';
};
template<typename T>
constexpr char TypeIdHolder<T>::kTarget;

// This gives us a unique runtime value per unique type - with which its much
// easier to verify uniqueness (below).
template<typename T>
static constexpr char const* TypeId() {
  return &TypeIdHolder<T>::kTarget;
}

//
// LIFT_MEMBER_TO_TYPE
//

TEST(MetaTest, MemberTagUniqueness) {
  std::vector<char const*> type_ids = {
    TypeId<LIFT_MEMBER_TO_TYPE(S, Virt1)>(),
    TypeId<LIFT_MEMBER_TO_TYPE(S, Virt2)>(),
    TypeId<LIFT_MEMBER_TO_TYPE(S, Virt3)>(),
    TypeId<LIFT_MEMBER_TO_TYPE(S, NonVirt1)>(),
    TypeId<LIFT_MEMBER_TO_TYPE(S, NonVirt2)>(),
    TypeId<LIFT_MEMBER_TO_TYPE(S, field)>(),
    TypeId<LIFT_MEMBER_TO_TYPE(R, Virt1)>(),
    TypeId<LIFT_MEMBER_TO_TYPE(R, Virt2)>(),
    TypeId<LIFT_MEMBER_TO_TYPE(R, Virt3)>(),
    TypeId<LIFT_MEMBER_TO_TYPE(R, NonVirt1)>(),
    TypeId<LIFT_MEMBER_TO_TYPE(R, NonVirt2)>(),
    TypeId<LIFT_MEMBER_TO_TYPE(R, field)>(),
  };

  for (int i = 0; i < type_ids.size(); i++) {
    for (int j = 0; j < type_ids.size(); j++) {
      if (i == j) {
        continue;
      }
      EXPECT_THAT(type_ids[i], Not(Eq(type_ids[j])))
          << "Member tags must be unique";
    }
  }
}

int PokeMemberCase(LIFT_MEMBER_TO_TYPE(S, Virt1)) {
  return 1;
}

int PokeMemberCase(LIFT_MEMBER_TO_TYPE(S, Virt2)) {
  return 2;
}

template<typename R, R S::* M>
int PokeMember() {
  return PokeMemberCase(LIFT_MEMBER_POINTER_TO_TYPE(M){});
}

TEST(MetaTest, MemberTagDispatch) {
  EXPECT_THAT((PokeMember<bool(int), &S::Virt1>()), Eq(1));
  EXPECT_THAT((PokeMember<bool(int, int), &S::Virt2>()), Eq(2));
}

//
// CastContainerElements
//

struct X {
  static constexpr int tag() { return 1; }
};

struct Y {
  static constexpr int tag() { return 2; }
};

template<typename T>
struct TypedVal {
  int value;

  bool operator==(TypedVal<T> const& other) const {
    return other.value == value;
  }
};

struct UntypedVal {
  int tag;
  int value;
};

struct CastValByTag {
  template <typename T>
  using TargetType = std::optional<TypedVal<T>>;

  template <typename T>
  TargetType<T> Cast(UntypedVal const& val) const {
    if (val.tag == T::tag()) {
      return TypedVal<T>{val.value};
    } else {
      return std::nullopt;
    }
  }
};

TEST(MetaTest, CastContainerElements_AllSuccess) {
  std::vector<UntypedVal> v{
    {X::tag(), 123},
    {Y::tag(), 456},
    {X::tag(), 789}
  };

  auto actual = CastContainerElements<X, Y, X>(v, CastValByTag{});
  auto expected = std::make_tuple(std::make_optional(TypedVal<X>{123}),
                                  std::make_optional(TypedVal<Y>{456}),
                                  std::make_optional(TypedVal<X>{789}));

  EXPECT_THAT(actual, Eq(expected));
}

TEST(MetaTest, CastContainerElements_AllSuccess_Pack) {
  std::vector<UntypedVal> v{
    {X::tag(), 123},
    {Y::tag(), 456},
    {X::tag(), 789}
  };

  // This uses the Pack<> overload instead.
  auto actual = CastContainerElements(Pack<X, Y, X>{}, v, CastValByTag{});
  auto expected = std::make_tuple(std::make_optional(TypedVal<X>{123}),
                                  std::make_optional(TypedVal<Y>{456}),
                                  std::make_optional(TypedVal<X>{789}));

  EXPECT_THAT(actual, Eq(expected));
}

TEST(MetaTest, CastContainerElements_OneFails) {
  std::vector<UntypedVal> v{
    {X::tag(), 123},
    {X::tag(), 456},
    {X::tag(), 789}
  };

  // Second element does not have the tag for Y.
  auto actual = CastContainerElements<X, Y, X>(v, CastValByTag{});
  auto expected = std::make_tuple(std::make_optional(TypedVal<X>{123}),
                                  std::optional<TypedVal<Y>>(std::nullopt),
                                  std::make_optional(TypedVal<X>{789}));

  EXPECT_THAT(actual, Eq(expected));
}

//
// MAKE_LINK and LinkedType<>
//

namespace links {

namespace a {

struct A1 {};
struct A2 {};
struct A3 {};

MAKE_LINK(A1, A2);

}  // namespace a

namespace b {

struct B1 {};

MAKE_LINK(B1, a::A3);

}  // namespace b

}  // namespace links

static_assert(std::is_same<LinkedType<links::a::A1>, links::a::A2>::value,
              "A1 -> A2");
static_assert(HasLinkedType<links::a::A1>(), "A1 -> A2");
static_assert(std::is_same<LinkedTypeOrVoid<links::a::A2>, void>::value,
              "A2 -/>");
static_assert(!HasLinkedType<links::a::A2>(), "A2 -/>");
static_assert(std::is_same<LinkedTypeOrVoid<links::a::A3>, void>::value,
              "A3 -/>");
static_assert(!HasLinkedType<links::a::A3>(), "A3 -/>");
static_assert(std::is_same<LinkedType<links::b::B1>, links::a::A3>::value,
              "b::B1 -> a::A3");
static_assert(HasLinkedType<links::b::B1>(), "b::B1 -> a::A3");

//
// Pack<>
//

template<typename A1, typename A2, size_t I1, size_t I2>
constexpr Unit CheckUnpack() {
  static_assert(std::is_same<A1, X>::value, "A1 == X");
  static_assert(std::is_same<A2, Y>::value, "A2 == Y");
  static_assert(I1 == 0, "I1 == 0");
  static_assert(I2 == 1, "I2 == 0");
  return {};
}

template<typename... A, size_t... I>
constexpr Unit UsePack(Pack<A...>, absl::index_sequence<I...>) {
  return CheckUnpack<A..., I...>();
}

template<typename... A>
constexpr Unit MakeAndUsePack() {
  return UsePack(Pack<A...>{}, Pack<A...>::MakeIndexSequence());
}

static_assert(MakeAndUsePack<X, Y>().True(), "Pack<>");

//
// LiftVoidReturn
//

TEST(MetaTest, LiftVoidReturn_Void) {
  int counter = 0;
  std::function<void()> f = [&counter]() { counter++; };

  f();
  EXPECT_THAT(counter, Eq(1));
  auto f_wrapped = LiftVoidReturn(f);
  EXPECT_THAT(f_wrapped(), Eq(Unit{}));
  EXPECT_THAT(counter, Eq(2));
}

TEST(MetaTest, LiftVoidReturn_Void_Args) {
  int counter = 0;
  std::function<void(int)> f = [&counter](int i) { counter += i; };

  f(10);
  EXPECT_THAT(counter, Eq(10));
  auto f_wrapped = LiftVoidReturn(f);
  EXPECT_THAT(f_wrapped(32), Eq(Unit{}));
  EXPECT_THAT(counter, Eq(42));
}

TEST(MetaTest, LiftVoidReturn_NonVoid) {
  int counter = 0;
  std::function<int(int)> f = [&counter](int i) {
    counter += i;
    return counter;
  };

  EXPECT_THAT(f(10), Eq(10));
  EXPECT_THAT(counter, Eq(10));
  auto f_wrapped = LiftVoidReturn(f);
  EXPECT_THAT(f_wrapped(32), Eq(42));
  EXPECT_THAT(counter, Eq(42));
}

TEST(MetaTest, LiftVoidReturn_Mutable) {
  int r = -1;
  auto f = [&r, counter = 0]() mutable {
    counter++;
    r = counter;
  };

  f();
  EXPECT_THAT(r, Eq(1));
  auto f_wrapped = LiftVoidReturn(f);
  EXPECT_THAT(f_wrapped(), Eq(Unit{}));
  EXPECT_THAT(r, Eq(2));
}

TEST(MetaTest, LiftVoidReturn_MutableAndMoveOnly) {
  int r = -1;
  auto f = [&r, counter = std::make_unique<int>(0)]() mutable {
    (*counter)++;
    r = *counter;
  };

  f();
  EXPECT_THAT(r, Eq(1));
  auto f_wrapped = LiftVoidReturn(std::move(f));
  EXPECT_THAT(f_wrapped(), Eq(Unit{}));
  EXPECT_THAT(r, Eq(2));
}

//
// FunctionTraits
//

#define STATIC_ASSERT_FUNCTION_TRAITS(fn, r, ...)                              \
  static_assert(std::is_same<FunctionTraits<fn>::ResultType, r>::value,        \
                "Incorrect result type from FunctionTraits");                  \
  static_assert(                                                               \
      std::is_same<FunctionTraits<fn>::ArgPackType, Pack<__VA_ARGS__>>::value, \
      "Incorrect arg pack from FunctionTraits")

STATIC_ASSERT_FUNCTION_TRAITS(void(), void);
STATIC_ASSERT_FUNCTION_TRAITS(void(int, char), void, int, char);
STATIC_ASSERT_FUNCTION_TRAITS(Identity<bool(char const*, int)>, bool,
                              char const*, int);

TEST(MetaTest, IsTypeOneOf) {
  static_assert(IsTypeOneOf<int, int>());
  static_assert(IsTypeOneOf<int, int, double>());
  static_assert(IsTypeOneOf<int, double, int>());
  static_assert(!IsTypeOneOf<int, bool>());
  static_assert(!IsTypeOneOf<int, double, char>());
}

TEST(MetaTest, IsSubsetOf) {
  using T1 = Pack<int, double>;
  using T2 = Pack<double, int>;
  using T3 = Pack<int, double, char>;

  static_assert(IsSubsetOf<T1, T1>::value);
  static_assert(IsSubsetOf<T1, T2>::value);
  static_assert(IsSubsetOf<T2, T1>::value);
  static_assert(IsSubsetOf<T2, T3>::value);
  static_assert(!IsSubsetOf<T3, T2>::value);
}

}  // namespace fcp
