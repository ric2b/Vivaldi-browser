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

/**
 * This file provides general utilities for metaprogramming.
 *
 *   - LIFT_MEMBER_TO_TYPE: Generates distinct types in correspondence with
 *     member-pointers (for both fields and functions). For example,
 *     LIFT_MEMBER_TO_TYPE(S, X) != LIFT_MEMBER_TO_TYPE(R, X), even if the
 *     declarations of S::X and R::X are identical.
 *
 *   - Unit: An empty struct (i.e. has a single canonical element). It is useful
 *     in contexts where a non-void return type is necessary but undesired: for
 *     example, in a constexpr function called only for static_asserts.
 *
 *   - Pack<T...>: Helps passing around the 'parameter packs' arising from
 *     variadic templates (they are not first-class). In particular, these allow
 *     writing a function such that F<A, B>() and F<Pack<A, B>>() are equivalent
 *     (Pack<A, B> _is_ first-class).
 *
 *   - MemberPointerTraits: Allows removing the 'container' part from
 *     member-pointer types, e.g.
 *       'R T::*' => 'R'
 *       'R (T::*)(A, B)' => 'R(A, B)'
 *
 *   - FunctionTraits: Allows destructuring function types, e.g. 'bool(int,
 *     int)' into ResultType = bool, ArgPackType = Pack<int, int>.
 *
 *   - FailIfReached: Allows writing static_asserts for templates that should
 *     never be instantiated. This is a workaround for the fact that
 *    'static_assert(false, "")' can trigger regardless of where it's located.
 *
 *   - Identity<T>: An alias useful with higher order templates.
 *
 *   - CastContainerElements: Allows 'casting' homogenous containers to
 *     heterogenous tuples, e.g. vector<X> -> tuple<A, B> - useful when
 *     when the type-list was erased earlier.
 *
 *   - LiftVoidReturn: Wraps a callable object, so that returned 'void' becomes
 *     'Unit' (if applicable). This avoids spread of special cases when handling
 *     callables and function-types generically (e.g. 'auto r = f()' is valid
 *     for f() returning anything _except_ void).
 *
 *   - MAKE_LINK and LinkedType<T>: Given types T, U and MAKE_LINK(T, U),
 *     LinkedType<T> == U. This can often be handled with template
 *     specialization, but (like AbslHashValue) we use ADL so that T (and
 *     MAKE_LINK next to it) can appear in any namespace.
 *
 *   - IsTypeOneOf<T, Us...>: A function to determines if the type T is in the
 *     list of types Us.
 *
 *   - IsSubsetOf<Pack<Ts...>, Pack<Us...>>: Determins if a pack of types Ts
 *     is a subset of a pack of types Us.
 */

#ifndef FCP_BASE_META_H_
#define FCP_BASE_META_H_

#include <tuple>
#include <type_traits>

#include "fcp/base/monitoring.h"

namespace fcp {

/**
 * An empty struct - i.e. there is a single canonical element.
 *
 * It is useful in contexts where a non-void return type is necessary but
 * undesired: for example, in a constexpr function called only for
 * static_asserts.
 *
 * Unit defines equality (they're always equal). True() always returns true,
 * which is convenient for allowing a unit-returning function call in a
 * static_assert.
 *
 * Unit::Ignore(...) sinks any arguments to a Unit. This is useful in C++11's
 * restricted constexpr as well as for parameter-pack expansions.
 */
struct Unit {
  constexpr bool operator==(Unit other) const { return true; }
  constexpr bool True() const { return true; }

  /** Ignores all arguments (of any type), returning Unit */
  template <typename... ArgTypes>
  static constexpr Unit Ignore(ArgTypes... args) {
    return {};
  }
};

/**
 * Pack<T...> facilitates passing around a parameter-pack T...
 *
 * Types are more or less first-class, in that you can place one somewhere (e.g.
 * as a struct member) and use it later. This is not the case for
 * parameter-packs: one can only expand T... within some template<typename...
 * T>.
 *
 * Pack<> is a work-around for that:
 *
 *   - To store a parameter-pack T... in hand: Instead store Pack<T...>, e.g.
 *     'using P = Pack<T...>'
 *
 *   - To revitalize the parameter pack later: Define a target function like
 *        template<typename... T> F(Pack<T...>)
 *     and call it as
 *        F(P{})
 *     (noting P from the prior example). The T... in scope of F arises from
 *     template argument deduction.
 */
template <typename... T>
struct Pack {
  /** Returns the related index-sequence type.
   *
   * Example:
   *
   *     template <typename... T, size_t... Idx>
   *     void Impl(Pack<T...>, absl::index_sequence<Idx...>) {
   *       auto zipped[] = {
   *         F<T>(Idx)... // T... and Idx... are zipped together.
   *       };
   *     }
   *
   *     template <typename... T>
   *     void Foo(Pack<T...> pack) {
   *       Impl(pack, pack.MakeIndexSequence());
   *     }
   */
  static constexpr absl::index_sequence_for<T...> MakeIndexSequence() {
    return {};
  }
};

/**
 * Workaround for static_assert(false) tripping even for un-instantiated
 * templates.
 */
template <typename T>
constexpr bool FailIfReached() {
  return !std::is_same<T, T>::value;
}

namespace meta_internal {

template <typename T, T M>
struct MemberTag {
  static_assert(std::is_member_pointer<T>::value,
                "Expected a member-pointer type");
};

template <typename CastOp, typename... T>
using CastResultType = std::tuple<typename CastOp::template TargetType<T>...>;

template <typename... T, size_t... Idx, typename Container, typename CastOp>
CastResultType<CastOp, T...> CastContainerElementsImpl(
    Container const& container, CastOp const& cast, Pack<T...>,
    absl::index_sequence<Idx...>) {
  FCP_CHECK(sizeof...(T) == container.size());
  return CastResultType<CastOp, T...>{cast.template Cast<T>(container[Idx])...};
}

template <typename F>
class VoidLifter {
 private:
  template <typename T>
  struct Tag {};

  template <typename... A>
  Unit DoCall(Tag<void>, A&&... args) {
    f_(std::forward<A>(args)...);
    return {};
  }

  template <typename R, typename... A>
  R DoCall(Tag<R>, A&&... args) {
    return f_(std::forward<A>(args)...);
  }

 public:
  explicit VoidLifter(F f) : f_(std::move(f)) {}

  template <typename... A>
  auto operator()(A&&... args) -> decltype(
      DoCall(Tag<decltype(std::declval<F>()(std::forward<A>(args)...))>{},
             std::forward<A>(args)...)) {
    return DoCall(Tag<decltype(std::declval<F>()(std::forward<A>(args)...))>{},
                  std::forward<A>(args)...);
  }

 private:
  F f_;
};

template <typename U, typename Dummy = void>
struct FailIfLinkMissing {
  using Type = U;
};

template <typename Dummy>
struct FailIfLinkMissing<void, Dummy> {
  static_assert(FailIfReached<Dummy>(),
                "Expected a type linked from T, via MAKE_LINK(T, U). Note that "
                "MAKE_LINK must appear in the same namespace as T.");
};

template <typename T>
struct LinkedTypeToken {
  using Type = T;
};

/**
 * Default case for LookupTypeLink. MAKE_LINK creates overloads which are more
 * specific (argument type matches without needing a template).
 */
template <typename T>
inline LinkedTypeToken<void> TypeLink_(LinkedTypeToken<T>) {
  return {};
}

/**
 * Resolves MAKE_LINK at the level of values (i.e. the link target is
 * represented in the return type). May be called qualified, i.e.
 * fcp::meta_internal::LookupTypeLink.
 *
 * This depends on ADL. TypeLink_ is an unqualified name, so those next to T are
 * overload candidates. As such, it's fine to call meta_internal::LookupTypeLink
 * but *not* meta_internal::TypeLink_ (hence this indirection).
 */
template <typename T>
constexpr auto LookupTypeLink(LinkedTypeToken<T> t) -> decltype(TypeLink_(t)) {
  return {};
}

template <template <typename> class M, typename Z>
struct UnwrapTemplateImpl {
  static constexpr bool kValid = false;
};

template <template <typename> class M, typename T>
struct UnwrapTemplateImpl<M, M<T>> {
  static constexpr bool kValid = true;
  using Type = T;
};

template <template <typename> class M, typename Z>
using UnwrapTemplate = meta_internal::UnwrapTemplateImpl<M, std::decay_t<Z>>;

}  // namespace meta_internal

/**
 * Generates distinct types in correspondence with member-pointers (for both
 * fields and functions).
 *
 * For example, LIFT_MEMBER_TO_TYPE(S, X) != LIFT_MEMBER_TO_TYPE(R, X), even if
 * the declarations of S::X and R::X are identical.
 *
 * The lifted type is always an empty struct, so it can be instantiated with {}
 * (for use in overload resolution) at no cost.
 */
#define LIFT_MEMBER_TO_TYPE(type, member) \
  LIFT_MEMBER_POINTER_TO_TYPE(&type::member)

/**
 * Same as LIFT_MEMBER_TO_TYPE, but invoked as e.g.
 * LIFT_MEMBER_POINTER_TO_TYPE(&S::X)
 */
#define LIFT_MEMBER_POINTER_TO_TYPE(ptr) \
  ::fcp::meta_internal::MemberTag<decltype(ptr), ptr>

/**
 * Allows removing the 'container' part from member-pointer types, e.g.
 *   'R T::*' => 'R' 'R (T::*)(A, B)' => 'R(A, B)'
 */
template <typename T>
struct MemberPointerTraits {
  static_assert(
      FailIfReached<T>(),
      "Expected a member pointer (both fields and functions are accepted)");
};

template <typename T, typename R>
struct MemberPointerTraits<R T::*> {
  using TargetType = R;
};

template <typename T>
struct FunctionTraits {
  static_assert(FailIfReached<T>(), "Expected a function type");
};

template <typename R, typename... A>
struct FunctionTraits<R(A...)> {
  using ResultType = R;
  using ArgPackType = Pack<A...>;
};

/** Type-level identity function; useful for higher order templates */
template <typename T>
using Identity = T;

/** See other overload; this one takes a Pack<T...> instead of explicit T... */
template <typename... T, typename Container, typename CastOp>
auto CastContainerElements(Pack<T...> pack, Container const& container,
                           CastOp const& cast)
    -> decltype(meta_internal::CastContainerElementsImpl(
        container, cast, pack, pack.MakeIndexSequence())) {
  return meta_internal::CastContainerElementsImpl(container, cast, pack,
                                                  pack.MakeIndexSequence());
}

/**
 * Allows 'casting' homogenous containers to heterogenous tuples, e.g.
 * vector<X> -> tuple<A, B> - useful when when the type-list was erased
 * earlier.
 *
 * 'CastOp' determines how to cast each element. It should be a type like the
 * following:
 *
 *    struct FooCast {
 *     template<typename T>
 *     using TargetType = Y<T>;
 *
 *     template <typename T>
 *     TargetType<T> Cast(X const& val) const {
 *        ...
 *      }
 *     };
 *
 * Supposing vector<X> vx, CastContainerElements<A, B>(vx, FooCast{}) would
 * yield a tuple<Y<A>, Y<B>> with values {Cast<A>(vx[0]), Cast<B>(vx[1])}.
 *
 * This function supports the 'Pack' wrapper. For example, the previous example
 * could also be written as CastContainerElements(Pack<X, Y>{}, vx, FooCast{}).
 */
template <typename... T, typename Container, typename CastOp>
auto CastContainerElements(Container const& container, CastOp const& cast)
    -> decltype(CastContainerElements(Pack<T...>{}, container, cast)) {
  return CastContainerElements(Pack<T...>{}, container, cast);
}

/**
 * Wraps a callable object, so that returned 'void' becomes 'Unit' (if
 * applicable). This avoids spread of special cases when handling callables and
 * function-types generically (e.g. 'auto r = f()' is valid for f() returning
 * anything _except_ void).
 */
template <typename F>
meta_internal::VoidLifter<F> LiftVoidReturn(F f) {
  return meta_internal::VoidLifter<F>(std::move(f));
}

/** See LinkedType<T> */
#define MAKE_LINK(a, b)                                      \
  inline ::fcp::meta_internal::LinkedTypeToken<b> TypeLink_( \
      ::fcp::meta_internal::LinkedTypeToken<a>) {            \
    return {};                                               \
  }

/**
 * See LinkedType<T>. This form returns void instead of failing when a link is
 * missing
 */
template <typename T>
using LinkedTypeOrVoid = typename decltype(meta_internal::LookupTypeLink(
    std::declval<meta_internal::LinkedTypeToken<T>>()))::Type;

/**
 * Indicates if some MAKE_LINK(T, ...) is visible.
 */
template <typename T>
constexpr bool HasLinkedType() {
  return !std::is_same<LinkedTypeOrVoid<T>, void>::value;
}

/**
 * Given types T, U and MAKE_LINK(T, U), LinkedType<T> == U.
 *
 * This can often be handled with template specialization, but (like
 * AbslHashValue) we use ADL to avoid restrictions on the namespaces in which
 * specializations can appear.
 *
 * The type T can appear in any namespace, but MAKE_LINK(T, U) must appear in
 * the same namespace (ideally, place it right after the declaration of T).
 * LinkedType<T> then works in any namespace.
 *
 * It is an error to use this alias for a T without a MAKE_LINK. See
 * HasLinkedType() and LinkedTypeOrVoid.
 */
template <typename T>
using LinkedType =
    typename meta_internal::FailIfLinkMissing<LinkedTypeOrVoid<T>>::Type;


/*
 * Given type T and typelist Us... determines if T is one of the types in Us.
 */
template <typename T, typename... Us>
struct IsTypeOneOfT : std::disjunction<std::is_same<T, Us>...> {};

template <typename T, typename... Us>
constexpr bool IsTypeOneOf() {
  return IsTypeOneOfT<T, Us...>::value;
}

/*
 * Given two typelists Ts... and Us... determines if Ts is a subset of Us.
 */
template <typename Ts, typename Us>
struct IsSubsetOf : std::false_type {};

template <typename... Ts, typename... Us>
struct IsSubsetOf<Pack<Ts...>, Pack<Us...>>
    : std::conjunction<IsTypeOneOfT<Ts, Us...>...> {};

template <template <typename> class M, typename Z>
using UnapplyTemplate =
    typename meta_internal::UnwrapTemplateImpl<M, std::decay_t<Z>>::Type;

}  // namespace fcp

#endif  // FCP_BASE_META_H_
