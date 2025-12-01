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

#ifndef FCP_BASE_RESULT_H_
#define FCP_BASE_RESULT_H_

#include <optional>
#include <type_traits>
#include <variant>

#include "fcp/base/error.h"
#include "fcp/base/meta.h"
#include "fcp/base/source_location.h"

namespace fcp {
namespace result_internal {

template <typename R>
struct ResultTraits;

}  // namespace result_internal

// A Result is either a value (T) or an opaque Error. There are two main ways to
// use one.
//
// Transform: Given a Result<T> r, and a callable f from T to Result<U>,
// r.Then(f) returns Result<U>. Note that errors are passed through (without
// calling f).
//
// Similarly, given a Result<T> r, and a callable f from T to U, r.Map(f)
// returns Result<U>. The difference is that Then can introduce new errors (it
// returns Result<U>) whereas Map only transforms values to other values.
//
//  Result<int> some_int = ...
//  Result<bool> b = some_int.Then([](int i) -> Result<int> {
//    if (i < 0) {
//      return TraceError<...>(...);
//    } else {
//      return i;
//    }
//  }).Map([](int i) -> bool) {
//    return i % 2 == 0;
//  });
//
// Propagate: The FCP_TRY macro unwraps results to their values. If a result
// contains an error, it is returned (from the function where FCP_TRY appears).
//
//   Result<int> GetInt();
//
//   Result<bool> F() {
//     int i = FCP_TRY(GetInt());
//     if (i < 0) {
//     }
//
//     return i % 2 == 0;
//   }
//
// Result<T> provides implicit conversions from T and Error. As above, in
// functions returning Result<T>, it is useful and encourage to return a T or
// Error directly.
template <typename T>
class ABSL_MUST_USE_RESULT Result {
 public:
  using ValueType = T;

  // These make Result<> usable as an argument to Match() (see match.h).
  using VariantType = std::variant<Error, T>;
  constexpr VariantType& variant() & { return val_; }
  constexpr VariantType const& variant() const& { return val_; }
  constexpr VariantType&& variant() && { return std::move(val_); }

  // Implicit conversion from T
  constexpr Result(T t) : val_(std::move(t)) {}  // NOLINT

  // Implicit conversion from Error
  Result(Error e) : val_(e) {}  // NOLINT

  constexpr bool is_error() const {
    return std::holds_alternative<Error>(val_);
  }

  // Returns a *reference* to the contained value.
  // Requires (CHECK): !is_error()
  constexpr T const& GetValueOrDie() const& {
    FCP_CHECK(std::holds_alternative<T>(val_));
    return std::get<T>(val_);
  }

  // Returns the contained value (by move).
  // This applies for results which are rvalues.
  //
  // Example:
  //   Result<X> r = f();
  //   X v = std::move(r).GetValueOrDie();
  //
  // Example:
  //   X v = f().GetValueOrDie();
  //
  // Requires (CHECK): !is_error()
  constexpr T GetValueOrDie() && {
    FCP_CHECK(std::holds_alternative<T>(val_));
    return std::get<T>(std::move(val_));
  }

  // Returns the contained error.
  // Requires (CHECK): is_error()
  Error GetErrorOrDie() const {
    FCP_CHECK(std::holds_alternative<Error>(val_));
    return std::get<Error>(val_);
  }

  // Transforms this Result into another (with value type U).
  //
  // If this Result holds an Error, it is passed through.
  // If this Result holds a value, then the callable 'fn' is applied to it.
  // The callable 'fn' is expected to return Result<U>.
  //
  // Example:
  //
  //  Result<int> some_int = ...
  //  Result<bool> b = some_int.Then([](int i) -> Result<bool> {
  //    if (i < 0) {
  //      return TraceError<...>(...);
  //    } else {
  //      return i % 2 == 0;
  //    }
  //  });
  template <typename Fn>
  constexpr auto Then(Fn fn) const& {
    return ThenInternal<false>(*this, std::move(fn));
  }

  template <typename Fn>
  constexpr auto Then(Fn fn) && {
    return ThenInternal<true>(std::move(*this), std::move(fn));
  }

  // Maps values of type T to a values of type U.
  //
  // If this Result holds an Error, it is passed through.
  // If this Result holds a value, then the callable 'fn' is applied to it.
  //
  // Example:
  //
  //  Result<int> some_int = ...
  //  Result<bool> b = some_int.Map([](int i) {
  //    return i % 2 == 0;
  //  });
  template <typename Fn>
  constexpr auto Map(Fn fn) const& {
    using U = std::invoke_result_t<Fn, T const&>;
    return ThenInternal<false>(
        *this, [fn = std::move(fn)](T const& t) { return Result<U>(fn(t)); });
  }

  template <typename Fn>
  constexpr auto Map(Fn fn) && {
    using U = std::invoke_result_t<Fn, T&&>;
    return ThenInternal<true>(std::move(*this), [fn = std::move(fn)](T&& t) {
      return Result<U>(fn(std::move(t)));
    });
  }

 private:
  template <bool Move, typename Fn>
  static constexpr auto ThenInternal(
      std::conditional_t<Move, Result<T>&&, Result<T> const&> r, Fn fn) {
    using RefType = std::conditional_t<Move, T&&, T const&>;
    using RetType = std::invoke_result_t<Fn, RefType>;
    static_assert(
        result_internal::ResultTraits<RetType>::is_result(),
        "The callable provided to 'Then' must return Result<U> for "
        "some type U. When always returning a value, use Map instead.");

    if (r.is_error()) {
      return RetType(r.GetErrorOrDie());
    } else {
      return fn(std::get<T>(std::move(r).variant()));
    }
  }

  std::variant<Error, T> val_;
};

// This is a deduction guide so that one can write Result(t) for a value t,
// without explicitly specifying the value type. This one is implicitly
// declared anyway; we make it explicit to suppress -Wctad-maybe-unsupported.
template <typename T>
Result(T) -> Result<T>;

// ResultFrom<T> -> Result<T>
// ResultFrom<Result<T>> -> Result<T>
//
// Note that ResultFrom<Error> is ill-formed (like Result<Error>).
template <typename T>
using ResultFrom = decltype(Result(std::declval<T>()));

// ResultOf applied to the result of calling Fn with Args...
template <typename Fn, typename... Args>
using ResultOf = ResultFrom<std::invoke_result_t<Fn, Args...>>;

namespace result_internal {

template <typename R>
struct ResultTraits {
  using ValueType = void;
};

template <typename T>
struct ResultTraits<Result<T>> {
  static constexpr bool is_result() { return true; }
  using ValueType = T;
};

// This is used in FCP_TRY, to require that the parameter to FCP_TRY has type
// Result<T> for some T.
template <typename T>
constexpr bool ResultIsError(Result<T> const& r) {
  return r.is_error();
}

}  // namespace result_internal

class ExpectBase {
 public:
  constexpr explicit ExpectBase(SourceLocation loc) : loc_(loc) {}

 protected:
  Error TraceExpectError(const char* expectation) const;
  Error TraceUnexpectedStatus(fcp::StatusCode expected_code,
                              const fcp::Status& actual) const;

 private:
  SourceLocation loc_;
};

// Returns Result<T> if the current result has std::variant that holds a
// value of type T; otherwise returns an error Result.
template <typename T>
struct ExpectIs : public ExpectBase {
  using ExpectBase::ExpectBase;
  constexpr explicit ExpectIs(SourceLocation loc = SourceLocation::current())
      : ExpectBase(loc) {}

  template <typename... Us>
  constexpr Result<T> operator()(std::variant<Us...> v) const {
    if (std::holds_alternative<T>(v)) {
      return std::get<T>(std::move(v));
    } else {
      return TraceExpectError("ExpectIs");
    }
  }
};

// Returns Result<std::variant<Us...>> if the current result has
// std::variant that holds a value of one of the types from Us... typelist;
// otherwise returns an error Result. This operation is valid only when the
// set of expected types Us... is a subset of the set of types Ts... in the
// current result.
template <typename... Ts>
struct ExpectOneOf : public ExpectBase {
  using ExpectBase::ExpectBase;
  constexpr explicit ExpectOneOf(SourceLocation loc = SourceLocation::current())
      : ExpectBase(loc) {}

  template <typename... Us>
  constexpr Result<std::variant<Ts...>> operator()(
      std::variant<Us...> v) const {
    static_assert(IsSubsetOf<Pack<Ts...>, Pack<Us...>>::value);

    // TODO(team): This should be expressible with Match
    return std::visit(
        [this](auto arg) -> Result<std::variant<Ts...>> {
          if constexpr (IsTypeOneOf<std::decay_t<decltype(arg)>, Ts...>()) {
            return std::variant<Ts...>(std::move(arg));
          } else {
            return TraceExpectError("ExpectOneOf<>");
          }
        },
        std::move(v));
  }
};

// Returns Result<Unit> if the current result has boolean 'true' value;
// otherwise returns an error Result.
struct ExpectTrue : public ExpectBase {
  using ExpectBase::ExpectBase;
  constexpr explicit ExpectTrue(SourceLocation loc = SourceLocation::current())
      : ExpectBase(loc) {}

  template <typename... Us>
  constexpr Result<Unit> operator()(bool b) const {
    if (b) {
      return Unit{};
    } else {
      return TraceExpectError("ExpectTrue");
    }
  }
};

// Returns Result<Unit> if the current result has boolean 'false' value;
// otherwise returns an error Result.
struct ExpectFalse : public ExpectBase {
  using ExpectBase::ExpectBase;
  constexpr explicit ExpectFalse(SourceLocation loc = SourceLocation::current())
      : ExpectBase(loc) {}

  template <typename... Us>
  constexpr Result<Unit> operator()(bool b) const {
    if (!b) {
      return Unit{};
    } else {
      return TraceExpectError("ExpectTrue");
    }
  }
};

// Returns Result<T> if the current result has std::optional<T> has a value;
// otherwise returns an error Result.
struct ExpectHasValue : public ExpectBase {
  using ExpectBase::ExpectBase;
  constexpr explicit ExpectHasValue(
      SourceLocation loc = SourceLocation::current())
      : ExpectBase(loc) {}

  template <typename T>
  constexpr Result<T> operator()(std::optional<T> v) const {
    if (v.has_value()) {
      return *std::move(v);
    } else {
      return TraceExpectError("ExpectHasValue");
    }
  }
};

// Returns Result<Unit> if the current result has an empty std::optional;
// otherwise returns an error Result.
struct ExpectIsEmpty : public ExpectBase {
  using ExpectBase::ExpectBase;
  constexpr explicit ExpectIsEmpty(
      SourceLocation loc = SourceLocation::current())
      : ExpectBase(loc) {}

  template <typename T>
  constexpr Result<Unit> operator()(std::optional<T> v) const {
    if (!v.has_value()) {
      return Unit{};
    } else {
      return TraceExpectError("ExpectIsEmpty");
    }
  }
};

struct ExpectOk : public ExpectBase {
  using ExpectBase::ExpectBase;
  constexpr explicit ExpectOk(SourceLocation loc = SourceLocation::current())
      : ExpectBase(loc) {}

  template <typename T>
  constexpr Result<T> operator()(StatusOr<T> s) const {
    if (s.ok()) {
      return std::move(s).value();
    } else {
      return TraceUnexpectedStatus(fcp::OK, s.status());
    }
  }

  Result<Unit> operator()(const Status& s) const {
    if (s.code() == fcp::OK) {
      return Unit{};
    } else {
      return TraceUnexpectedStatus(fcp::OK, s);
    }
  }
};

}  // namespace fcp

#define FCP_TRY(...)                                             \
  ({                                                             \
    auto try_tmp_value_ = (__VA_ARGS__);                         \
    if (::fcp::result_internal::ResultIsError(try_tmp_value_)) { \
      return try_tmp_value_.GetErrorOrDie();                     \
    }                                                            \
    std::move(try_tmp_value_).GetValueOrDie();                   \
  })

#endif  // FCP_BASE_RESULT_H_
