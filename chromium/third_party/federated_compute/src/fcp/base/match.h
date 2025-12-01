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

// 'Match' expressions for {std, absl}::variant.
//
// {std, absl}::variant is an algebraic sum type. However, the standard library
// does not provide a convenient way to destructure or match on them - unlike in
// Haskell, Rust, etc.
//
// This file provides a way to match on :variant in a way akin to a switch
// statement.
//
// Example:
//
//   using V = std::variant<X, Y, Z>;
//   V v = ...;
//   ...
//   int i = Match(v,
//     [](X const& x) { return 1; },
//     [](Y const& y) { return 2; },
//     [](Z const& z) { return 3; });
//
// It is a compile-time error if the match is not exhaustive. A 'Default' case
// can be provided:
//
//   int i = Match(v,
//     [](X const& x) { return 1; },
//     // Called with the otherwise-unhandled alternative (see decltype(alt)).
//     [](Default, auto const& alt) { ...; });
//
//   int i = Match(v,
//     [](X const& x) { return 1; },
//     // Called with the variant itself.
//     [](Default, V const& v) { ...; });
//
// If constructing the matcher lambdas is non-trivial, it might be worthwhile to
// create a re-usable matcher object. See 'MakeMatcher'.

#ifndef FCP_BASE_MATCH_H_
#define FCP_BASE_MATCH_H_

#include <optional>
#include <type_traits>
#include <variant>

#include "fcp/base/meta.h"

namespace fcp {

// Marker type for default match cases.
struct Default {};

namespace match_internal {

template <typename... CaseFns>
struct MatchCasesCallable : public CaseFns... {
  // Each CaseFn provides operator(). We want to pick one by overload
  // resolution.
  using CaseFns::operator()...;
};

template <typename ToType, typename... CaseFns>
class MatchCases {
 public:
  explicit constexpr MatchCases(MatchCasesCallable<CaseFns...> c)
      : callable_(std::move(c)) {}

  // False by default
  template <typename Enable, typename... T>
  struct IsCaseHandledImpl : public std::false_type {};

  // True when m.MatchCases(args...) is well-formed, for a
  // MatchCases<CaseFns...> m and T arg.
  template <typename... T>
  struct IsCaseHandledImpl<
      std::void_t<decltype(std::declval<MatchCasesCallable<CaseFns...>>()(
          std::declval<T>()...))>,
      T...> : public std::true_type {};

  template <typename... T>
  static constexpr bool IsCaseHandled() {
    return IsCaseHandledImpl<void, T...>::value;
  }

  template <typename ToType_ = ToType, typename... Args>
  constexpr auto operator()(Args&&... args) const {
    if constexpr (std::is_void_v<ToType_>) {
      return callable_(std::forward<Args>(args)...);
    } else {
      return ToType_(callable_(std::forward<Args>(args)...));
    }
  }

 private:
  MatchCasesCallable<CaseFns...> callable_;
};

template <typename ToType, typename... CaseFns>
constexpr MatchCases<ToType, CaseFns...> MakeMatchCases(CaseFns... case_fns) {
  return MatchCases<ToType, CaseFns...>(
      MatchCasesCallable<CaseFns...>{case_fns...});
}

template <typename CasesType, typename VariantType, typename ArgType>
constexpr auto ApplyCase(CasesType const& cases, VariantType&& v,
                         ArgType&& arg) {
  if constexpr (CasesType::template IsCaseHandled<ArgType>()) {
    return cases(std::forward<ArgType>(arg));
  } else if constexpr (CasesType::template IsCaseHandled<Default, ArgType>()) {
    return cases(Default{}, std::forward<ArgType>(arg));
  } else if constexpr (CasesType::template IsCaseHandled<Default,
                                                         VariantType>()) {
    return cases(Default{}, std::forward<VariantType>(v));
  } else if constexpr (CasesType::template IsCaseHandled<Default>()) {
    return cases(Default{});
  } else {
    static_assert(
        FailIfReached<ArgType>(),
        "Provide a case for all variant alternatives, or a 'Default' case");
  }
}

template <typename Traits, typename CasesType>
class VariantMatcherImpl {
 public:
  using ValueType = typename Traits::ValueType;

  explicit constexpr VariantMatcherImpl(CasesType cases)
      : cases_(std::move(cases)) {}

  constexpr auto Match(ValueType* v) const { return MatchInternal(v); }

  constexpr auto Match(ValueType const& v) const { return MatchInternal(v); }

  constexpr auto Match(ValueType&& v) const {
    return MatchInternal(std::move(v));
  }

 private:
  template <typename FromType>
  constexpr auto MatchInternal(FromType&& v) const {
    return Traits::Visit(std::forward<FromType>(v), [this, &v](auto&& alt) {
      return ApplyCase(cases_, std::forward<FromType>(v),
                       std::forward<decltype(alt)>(alt));
    });
  }

  CasesType cases_;
};

template <typename T, typename Enable = void>
struct MatchTraits {
  static_assert(FailIfReached<T>(),
                "Only variant-like (e.g. std::variant<...> types can be "
                "matched. See MatchTraits.");
};

template <typename... AltTypes>
struct MatchTraits<std::variant<AltTypes...>> {
  using ValueType = std::variant<AltTypes...>;

  template <typename VisitFn>
  static constexpr auto Visit(ValueType const& v, VisitFn&& fn) {
    return std::visit(std::forward<VisitFn>(fn), v);
  }

  template <typename VisitFn>
  static constexpr auto Visit(ValueType&& v, VisitFn&& fn) {
    return std::visit(std::forward<VisitFn>(fn), std::move(v));
  }

  template <typename VisitFn>
  static constexpr auto Visit(ValueType* v, VisitFn&& fn) {
    return std::visit([fn = std::forward<VisitFn>(fn)](
                          auto& alt) mutable { return fn(&alt); },
                      *v);
  }
};

template <typename T>
struct MatchTraits<std::optional<T>> {
  using ValueType = std::optional<T>;

  static constexpr auto Wrap(std::optional<T>* o)
      -> std::variant<T*, std::nullopt_t> {
    if (o->has_value()) {
      return &**o;
    } else {
      return std::nullopt;
    }
  }

  static constexpr auto Wrap(std::optional<T> const& o)
      -> std::variant<std::reference_wrapper<T const>, std::nullopt_t> {
    if (o.has_value()) {
      return std::ref(*o);
    } else {
      return std::nullopt;
    }
  }

  static constexpr auto Wrap(std::optional<T>&& o)
      -> std::variant<T, std::nullopt_t> {
    if (o.has_value()) {
      return *std::move(o);
    } else {
      return std::nullopt;
    }
  }

  template <typename V, typename VisitFn>
  static constexpr auto Visit(V&& v, VisitFn&& fn) {
    return std::visit(std::forward<VisitFn>(fn), Wrap(std::forward<V>(v)));
  }
};

template <typename T>
struct MatchTraits<T, std::void_t<typename T::VariantType>> {
  using ValueType = T;

  template <typename VisitFn>
  static constexpr auto Visit(ValueType const& v, VisitFn&& fn) {
    return MatchTraits<typename T::VariantType>::Visit(
        v.variant(), std::forward<VisitFn>(fn));
  }

  template <typename VisitFn>
  static constexpr auto Visit(ValueType&& v, VisitFn&& fn) {
    return MatchTraits<typename T::VariantType>::Visit(
        std::move(v).variant(), std::forward<VisitFn>(fn));
  }

  template <typename VisitFn>
  static constexpr auto Visit(ValueType* v, VisitFn&& fn) {
    return MatchTraits<typename T::VariantType>::Visit(
        &v->variant(), std::forward<VisitFn>(fn));
  }
};

template <typename VariantType, typename CasesType>
constexpr auto CreateMatcherImpl(CasesType cases) {
  return VariantMatcherImpl<MatchTraits<VariantType>, CasesType>(
      std::move(cases));
}

}  // namespace match_internal

// See file remarks.
template <typename From, typename To = void, typename... CaseFnTypes>
constexpr auto MakeMatcher(CaseFnTypes... fns) {
  return match_internal::CreateMatcherImpl<From>(
      match_internal::MakeMatchCases<To>(fns...));
}

// See file remarks.
//
// Note that the order of template arguments differs from MakeMatcher; it is
// expected that 'From' is always deduced (but it can be useful to specify 'To'
// explicitly).
template <typename To = void, typename From, typename... CaseFnTypes>
constexpr auto Match(From&& v, CaseFnTypes... fns) {
  // 'From' is intended to be deduced. For MakeMatcher, we want V (not e.g. V
  // const&).
  auto m = MakeMatcher<std::decay_t<From>, To>(fns...);
  // The full type is still relevant for forwarding.
  return m.Match(std::forward<From>(v));
}

}  // namespace fcp

#endif  // FCP_BASE_MATCH_H_
