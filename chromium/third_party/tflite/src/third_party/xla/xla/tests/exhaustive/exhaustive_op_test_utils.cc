/* Copyright 2019 The OpenXLA Authors.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "xla/tests/exhaustive/exhaustive_op_test_utils.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "Eigen/Core"
#include "xla/literal.h"
#include "xla/tsl/lib/core/status_test_util.h"
#include "xla/tsl/util/command_line_flags.h"
#include "xla/types.h"
#include "tsl/platform/env.h"
#include "tsl/platform/file_system.h"
#include "tsl/platform/path.h"
#include "tsl/platform/test.h"

namespace xla {
namespace exhaustive_op_test {

int eup_version = 0;

int GetEupVersion() { return eup_version; }

bool dump_values = false;

bool ShouldDumpValues() { return dump_values; }

void AddExhaustiveFlags(std::vector<tsl::Flag>& flag_list) {
  flag_list.push_back(
      tsl::Flag("dump_values", &xla::exhaustive_op_test::dump_values,
                "Include to dump files of the expected and actual results "
                "(default false)."));
}

bool IsSubnormalReal(xla::complex64 value) { return IsSubnormal(value.real()); }

bool IsSubnormalReal(xla::complex128 value) {
  return IsSubnormal(value.real());
}

bool IsMinNormalReal(xla::complex64 value) { return IsMinNormal(value.real()); }

bool IsMinNormalReal(xla::complex128 value) {
  return IsMinNormal(value.real());
}

bool IsSubnormalImaginary(xla::complex64 value) {
  return IsSubnormal(value.imag());
}

bool IsSubnormalImaginary(xla::complex128 value) {
  return IsSubnormal(value.imag());
}

bool IsMinNormalImaginary(xla::complex64 value) {
  return IsMinNormal(value.imag());
}

bool IsMinPositiveImaginary(xla::complex128 value) {
  return IsMinNormal(value.imag());
}

/*static*/ ErrorSpec::Builder builder() { return ErrorSpecBuilder(); }

ErrorSpecBuilder& ErrorSpecBuilder::abs_err(double abs_err) & {
  spec_.abs_err = abs_err;
  return *this;
}

ErrorSpecBuilder& ErrorSpecBuilder::rel_err(double rel_err) & {
  spec_.rel_err = rel_err;
  return *this;
}

ErrorSpecBuilder& ErrorSpecBuilder::distance_err(int64_t distance_err) & {
  spec_.distance_err = distance_err;
  return *this;
}

ErrorSpecBuilder& ErrorSpecBuilder::strict_signed_zeros(
    bool strict_signed_zeros) & {
  spec_.strict_signed_zeros = strict_signed_zeros;
  return *this;
}

ErrorSpecBuilder& ErrorSpecBuilder::skip_comparison(bool skip_comparison) & {
  spec_.skip_comparison = skip_comparison;
  return *this;
}

ErrorSpecBuilder&& ErrorSpecBuilder::abs_err(double abs_err) && {
  spec_.abs_err = abs_err;
  return std::move(*this);
}

ErrorSpecBuilder&& ErrorSpecBuilder::rel_err(double rel_err) && {
  spec_.rel_err = rel_err;
  return std::move(*this);
}

ErrorSpecBuilder&& ErrorSpecBuilder::distance_err(int64_t distance_err) && {
  spec_.distance_err = distance_err;
  return std::move(*this);
}

ErrorSpecBuilder&& ErrorSpecBuilder::strict_signed_zeros(
    bool strict_signed_zeros) && {
  spec_.strict_signed_zeros = strict_signed_zeros;
  return std::move(*this);
}

ErrorSpecBuilder&& ErrorSpecBuilder::skip_comparison(bool skip_comparison) && {
  spec_.skip_comparison = skip_comparison;
  return std::move(*this);
}

ErrorSpecBuilder::operator ErrorSpec() && { return std::move(*this).build(); }

ErrorSpec ErrorSpecBuilder::build() && { return spec_; }

namespace {

template <typename Type, typename FuncPtr>
ErrorSpec CallErrorSpec(FuncPtr* func, const std::array<Type, 1>& in) {
  return func(in[0]);
}

template <typename Type, typename FuncPtr>
ErrorSpec CallErrorSpec(FuncPtr* func, const std::array<Type, 2>& in) {
  return func(in[0], in[1]);
}

template <typename Type, typename FuncPtr>
Type CallOperation(FuncPtr* func, const std::array<Type, 1>& in) {
  return func(in[0]);
}

template <typename Type, typename FuncPtr>
Type CallOperation(FuncPtr* func, const std::array<Type, 2>& in) {
  return func(in[0], in[1]);
}

// The number of values that can be substituted for subnormal inputs.
constexpr int kNumSubnormalSubstitutionValues = 4;

// Encodings used to determine where subnormal test values are cached.
constexpr int kPositiveMin = 0;
constexpr int kNegativeMin = 1;
constexpr int kPositiveZero = 2;
constexpr int kNegativeZero = 3;
constexpr int kNonSubnormal = -1;
constexpr int kInvalidCacheIndex = -1;

template <typename T>
struct is_complex_t : absl::disjunction<std::is_same<T, complex64>,
                                        std::is_same<T, complex128>> {};

// When we are testing a value such that all of its components are subnormal,
// we also need to test inputs made up of the Cartesian product of values
// replaced for each subnormal component. These additional test inputs are
// common enough where it will be efficient to just cache the results of these
// Cartesian products. In order to cache these values, we need a one to one
// mapping between these Cartesian products and cache locations.
//
// Our mapping works by assigning each component an integer in
// [0, kNumSubnormalSubstitutionValues) based on its test value. By lining
// these integers up with the n'th component corresponding to the n'th digit,
// then for each Cartesian product element we essentially create a unique base
// kNumSubnormalSubstitutionValues number. This number represents our cache
// index.
//
// In the event that there a component is not a subnormal, the value should
// not be cached, so we return a kNonSubnormal value.

template <
    typename NativeRefT,
    typename std::enable_if<!is_complex_t<NativeRefT>::value>::type* = nullptr>
int GetCacheLocation(NativeRefT value) {
  bool positive = !std::signbit(value);
  if (std::abs(value) == std::numeric_limits<NativeRefT>::min()) {
    return positive ? kPositiveMin : kNegativeMin;
  } else if (value != 0) {
    CHECK(std::fpclassify(value) != FP_SUBNORMAL);
    return kNonSubnormal;
  } else {
    return positive ? kPositiveZero : kNegativeZero;
  }
}

template <
    typename NativeRefT,
    typename std::enable_if<is_complex_t<NativeRefT>::value>::type* = nullptr>
int GetCacheLocation(NativeRefT value) {
  int real_loc =
      GetCacheLocation<typename NativeRefT::value_type>(value.real());
  int imag_loc =
      GetCacheLocation<typename NativeRefT::value_type>(value.imag());
  if (real_loc == kNonSubnormal || imag_loc == kNonSubnormal) {
    return kNonSubnormal;
  } else {
    return real_loc * kNumSubnormalSubstitutionValues + imag_loc;
  }
}

template <bool is_complex, typename NativeRefT, size_t N>
int GetCacheLocation(const std::array<NativeRefT, N>& input) {
  int location = 0;
  int cache_size_per_element = (is_complex ? kNumSubnormalSubstitutionValues *
                                                 kNumSubnormalSubstitutionValues
                                           : kNumSubnormalSubstitutionValues);
  for (int i = 0; i < N; ++i) {
    int comp_loc = GetCacheLocation<NativeRefT>(input[i]);
    if (i == kNonSubnormal) {
      return kNonSubnormal;
    }
    location *= cache_size_per_element;
    location += comp_loc;
  }
  return location;
}

// The inverse function of GetCacheLocation.

template <typename RetT,
          typename std::enable_if<!is_complex_t<RetT>::value>::type* = nullptr>
RetT FromCacheLocationComponent(int cache_loc) {
  switch (cache_loc) {
    case kPositiveMin:
      return std::numeric_limits<RetT>::min();
    case kNegativeMin:
      return -std::numeric_limits<RetT>::min();
    case kPositiveZero:
      return static_cast<RetT>(0.0);
    case kNegativeZero:
      return static_cast<RetT>(-0.0);
    default:
      LOG(FATAL) << "Invalid cache_loc value of " << cache_loc;
  }
}

template <typename RetT,
          typename std::enable_if<is_complex_t<RetT>::value>::type* = nullptr>
RetT FromCacheLocationComponent(int cache_loc) {
  CHECK_LT(cache_loc,
           kNumSubnormalSubstitutionValues * kNumSubnormalSubstitutionValues);
  CHECK_GE(cache_loc, 0);

  RetT value;
  value.real(FromCacheLocationComponent<typename RetT::value_type>(
      cache_loc / kNumSubnormalSubstitutionValues));
  value.imag(FromCacheLocationComponent<typename RetT::value_type>(
      cache_loc % kNumSubnormalSubstitutionValues));
  return std::move(value);
}

template <bool is_complex, typename NativeRefT, size_t N>
std::array<NativeRefT, N> FromCacheLocation(int cache_loc) {
  std::array<NativeRefT, N> input;
  int cache_size_per_element = (is_complex ? kNumSubnormalSubstitutionValues *
                                                 kNumSubnormalSubstitutionValues
                                           : kNumSubnormalSubstitutionValues);
  for (int i = N - 1; i >= 0; --i) {
    input[i] = FromCacheLocationComponent<NativeRefT>(cache_loc %
                                                      cache_size_per_element);
    cache_loc /= cache_size_per_element;
  }

  return input;
}

// Returns a string that describes the test value for the actual value.
template <
    typename NativeRefT,
    typename std::enable_if<!is_complex_t<NativeRefT>::value>::type* = nullptr>
std::string GetSubnormalDescription(NativeRefT test_val,
                                    NativeRefT actual_val) {
  std::string sp_min_normal = "sign-preserving min-normal-float";
  std::string sp_zero = "sign-preserving zero";
  std::string nsp_zero = "non-sign-preserving zero";

  switch (GetCacheLocation<NativeRefT>(test_val)) {
    case kNegativeMin:
    case kPositiveMin:
      return sp_min_normal;
    case kNegativeZero:
    case kPositiveZero:
      return (std::signbit(test_val) == std::signbit(actual_val)) ? sp_zero
                                                                  : nsp_zero;
    default:
      return "";
  }
}

template <
    typename NativeRefT,
    typename std::enable_if<is_complex_t<NativeRefT>::value>::type* = nullptr>
std::string GetSubnormalDescription(NativeRefT test_val,
                                    NativeRefT actual_val) {
  std::string real = GetSubnormalDescription<typename NativeRefT::value_type>(
      test_val.real(), actual_val.real());
  std::string imag = GetSubnormalDescription<typename NativeRefT::value_type>(
      test_val.imag(), actual_val.imag());

  if (real.empty()) {
    if (imag.empty()) {
      return "";
    }
    real = "real";
  } else if (imag.empty()) {
    imag = "imag";
  }

  return absl::StrCat("(", real, ", ", imag, ")");
}

template <bool is_complex, typename NativeRefT, size_t N>
std::string GetSubnormalDescription(std::array<NativeRefT, N> test_vals,
                                    std::array<NativeRefT, N> actual_vals) {
  if (N == 1) {
    return GetSubnormalDescription<NativeRefT>(test_vals[0], actual_vals[0]);
  }

  std::array<std::string, N> str_vals;
  for (int i = 0; i < N; ++i) {
    str_vals[i] =
        GetSubnormalDescription<NativeRefT>(test_vals[i], actual_vals[i]);
    if (str_vals[i].empty()) {
      str_vals[i] = "original";
    }
  }

  return absl::StrCat("(", absl::StrJoin(str_vals, ", "), ")");
}

template <
    typename NativeT, typename IntegralType,
    typename std::enable_if<!is_complex_t<NativeT>::value>::type* = nullptr>
std::string StringifyNum(NativeT x) {
  return absl::StrFormat("%0.*g (0x%0*x)",
                         std::numeric_limits<NativeT>::max_digits10,
                         static_cast<double>(x),
                         // N.B.: `|bytes| * 8 / 4` multiplied by 8 for bytes to
                         // bits and then divided by 4 for bits to hexdigits.
                         sizeof(IntegralType) * 2, BitCast<IntegralType>(x));
}

template <
    typename NativeT, typename IntegralType,
    typename std::enable_if<is_complex_t<NativeT>::value>::type* = nullptr>
std::string StringifyNum(NativeT x) {
  return absl::StrCat(
      "(", StringifyNum<typename NativeT::value_type, IntegralType>(x.real()),
      ", ", StringifyNum<typename NativeT::value_type, IntegralType>(x.imag()),
      ")");
}

template <typename NativeT, typename IntegralType, size_t N>
std::string StringifyNum(const std::array<NativeT, N>& inputs) {
  if (N == 1) {
    return StringifyNum<NativeT, IntegralType>(inputs[0]);
  }

  std::array<std::string, N> str_vals;
  for (int i = 0; i < N; ++i) {
    str_vals[i] = StringifyNum<NativeT, IntegralType>(inputs[i]);
  }

  return absl::StrCat("(", absl::StrJoin(str_vals, ", "), ")");
}

template <typename ErrorGenerator>
void PrintSkipped(int64_t* skipped, const ErrorGenerator& err_generator) {
  // We send some fixed amount of skipped messages to the log. The remainder we
  // squelch unless we're at vlog level 2.
  constexpr int64_t kMaxMismatchesLoggedToErr = 1000;

  (*skipped)++;
  if (*skipped < kMaxMismatchesLoggedToErr || VLOG_IS_ON(2)) {
    LOG(WARNING) << err_generator();
  } else if (*skipped == kMaxMismatchesLoggedToErr) {
    LOG(WARNING) << "Not printing any more skipped messages; pass "
                    "--vmodule=exhaustive_op_test=2 to see "
                    "all of them.";
  }
}

template <typename ErrorGenerator>
void PrintMismatch(int64_t* mismatches, const ErrorGenerator& err_generator) {
  // We send a few mismatches to gunit so they show up nicely in test logs.
  // Then we send more to LOG(ERROR).  The remainder we squelch unless we're
  // at vlog level 2.
  constexpr int64_t kMaxMismatchesLoggedToGunit = 10;
  constexpr int64_t kMaxMismatchesLoggedToErr = 1000;

  (*mismatches)++;
  if (*mismatches < kMaxMismatchesLoggedToGunit) {
    FAIL() << err_generator();
  } else if (*mismatches < kMaxMismatchesLoggedToErr || VLOG_IS_ON(2)) {
    LOG(ERROR) << err_generator();
  } else if (*mismatches == kMaxMismatchesLoggedToErr) {
    LOG(ERROR) << "Not printing any more mismatches; pass "
                  "--vmodule=exhaustive_op_test=2 to see "
                  "all of them.";
  }
}

}  // namespace

// If we are in debug mode, we fail the test execution at the first
// comparison failure to avoid dumping too much log data and ensure the
// relevant debugging information is the last logged data.
//
// If we are not in debug mode, we will continue to the next loop iteration.
#define EXPECT_NEAR_FAIL_OR_CONTINUE() \
  if (should_emit_debug_logging_) {    \
    FAIL();                            \
  } else {                             \
    continue;                          \
  }                                    \
  static_assert(true, "")

template <PrimitiveType T, size_t N>
void ExhaustiveOpTestBase<T, N>::ExpectNear(
    const LiteralInputs& input_literals, const Literal& result_literal,
    EvaluateOp evaluate_op, ErrorSpecGen error_spec_gen,
    OutputRangeCheck check_valid_range) {
  // Cache for when all components are subnormal testing values.
  std::vector<NativeRefT> pure_subnormal_cache;
  // TODO(b/353790524): Subnormal cache does not seem to work properly with
  // more than 1 input.
  if constexpr (N == 1) {
    // Since we take the cross product of all possible test values, and each
    // component has kNumSubnormalSubstitutionValues possible test values, then
    // the total number of different cache locations are
    // kNumSubnormalSubstitutionValues raised to the num_components.
    // num_components = N for the reals, and 2*N for the complex.
    int64_t max_cache_size =
        pow(kNumSubnormalSubstitutionValues, N * (Traits::kIsComplex ? 2 : 1));
    pure_subnormal_cache.reserve(max_cache_size);
    for (int i = 0; i < max_cache_size; ++i) {
      pure_subnormal_cache.push_back(CallOperation(
          evaluate_op,
          FromCacheLocation<Traits::kIsComplex, NativeRefT, N>(i)));
    }
  }

  // Dump file for the test. This is unused unless this->should_dump_values is
  // true.
  std::unique_ptr<tsl::WritableFile> dump_file;
  if (should_dump_values_) {
    auto* env = tsl::Env::Default();

    std::string cleaned_suite_name =
        absl::StrReplaceAll(SuiteName(), {{"/", "__"}});
    std::string cleaned_test_name =
        absl::StrReplaceAll(TestName(), {{"/", "__"}});
    std::string dump_filename = absl::StrFormat(
        "%s_%s_dump.txt", cleaned_suite_name, cleaned_test_name);

    std::string outdir;
    if (tsl::io::GetTestUndeclaredOutputsDir(&outdir)) {
      dump_filename = tsl::io::JoinPath(outdir, dump_filename);
    }

    TF_EXPECT_OK(env->NewWritableFile(dump_filename, &dump_file));
    TF_EXPECT_OK(
        dump_file->Append("input values -> actual output {expected output}\n"
                          "-----------------------------------------------\n"));
  }

  NativeListInputs inputs_arr;
  for (int i = 0; i < N; ++i) {
    const Literal& literal = input_literals[i];
    inputs_arr[i] = literal.data<NativeT>();
  }

  absl::Span<const NativeT> result_arr = result_literal.data<NativeT>();

  int64_t skipped = 0;
  int64_t mismatches = 0;

  for (int64_t i = 0; i < result_arr.size(); ++i) {
    NativeInputs inputs;
    NativeRefInputs inputs_ref_ty;

    for (int j = 0; j < N; ++j) {
      inputs[j] = inputs_arr[j][i];
      inputs_ref_ty[j] = static_cast<NativeRefT>(inputs[j]);
    }

    NativeT actual = result_arr[i];
    NativeT expected =
        static_cast<NativeT>(CallOperation(evaluate_op, inputs_ref_ty));

    // Dump input, actual, and expected values _before_ we do error checking to
    // avoid the continues.
    if (should_dump_values_) {
      std::string result_string;
      absl::StrAppend(
          &result_string,
          StringifyNum<NativeT, ComponentIntegralNativeT, N>(inputs), " -> ",
          StringifyNum<NativeT, ComponentIntegralNativeT>(actual));
      absl::StrAppend(&result_string, " {",
                      StringifyNum<NativeT, ComponentIntegralNativeT>(expected),
                      "}");
      absl::StrAppend(&result_string, "\n");
      TF_EXPECT_OK(dump_file->Append(result_string));
    }

    ErrorSpec error_spec = CallErrorSpec(error_spec_gen, inputs);
    ASSERT_GE(error_spec.abs_err, 0.0);
    ASSERT_GE(error_spec.rel_err, 0.0);
    ASSERT_GE(error_spec.distance_err, 0.0);

    if (error_spec.skip_comparison) {
      PrintSkipped(&skipped, [&] {
        return absl::StrFormat(
            "skipping tolerance check for input %s due to "
            "ErrorSpec::skip_comparison",
            StringifyNum<NativeT, ComponentIntegralNativeT, N>(inputs));
      });
      continue;
    }

    if (check_valid_range != nullptr && !check_valid_range(inputs, actual)) {
      PrintMismatch(&mismatches, [&] {
        return absl::StrFormat(
            "mismatch on input: %s. output: %s, output is not in valid range",
            StringifyNum<NativeT, ComponentIntegralNativeT, N>(inputs),
            StringifyNum<NativeT, ComponentIntegralNativeT>(actual));
      });
      EXPECT_NEAR_FAIL_OR_CONTINUE();
    }

    if (IsClose(static_cast<NativeRefT>(expected),
                static_cast<NativeRefT>(actual), error_spec)) {
      continue;
    }

    std::vector<NativeRefInputs> subnormal_test_inputs =
        GetTestValuesWithSubnormalSubstitutions(inputs_ref_ty);

    // Easy case: If `input` is not subnormal and !IsClose(expected, actual,
    // error_spec), print an error.
    if (subnormal_test_inputs.size() == 1) {
      PrintMismatch(&mismatches, [&] {
        return absl::StrFormat(
            "Mismatch on %s. Expected %s, but got %s.",
            StringifyNum<NativeT, ComponentIntegralNativeT, N>(inputs),
            StringifyNum<NativeT, ComponentIntegralNativeT>(expected),
            StringifyNum<NativeT, ComponentIntegralNativeT>(actual));
      });
      EXPECT_NEAR_FAIL_OR_CONTINUE();
    }

    // Otherwise, we need to test the additional subnormal test values.
    std::vector<NativeRefT> subnormal_test_results;
    subnormal_test_results.reserve(subnormal_test_inputs.size());
    bool passed_subnormal_test = false;

    for (NativeRefInputs test_value : subnormal_test_inputs) {
      NativeRefT result;
      // TODO(b/353790524): Subnormal cache does not seem to work properly with
      // more than 1 input.
      if constexpr (N == 1) {
        int cache_loc =
            GetCacheLocation<Traits::kIsComplex,
                             typename NativeRefInputs::value_type, N>(
                test_value);
        if (cache_loc == kInvalidCacheIndex) {
          result = CallOperation(evaluate_op, test_value);
        } else {
          result = pure_subnormal_cache[cache_loc];
        }
      } else {
        result = CallOperation(evaluate_op, test_value);
      }

      if (IsClose(result, static_cast<NativeRefT>(actual), error_spec)) {
        passed_subnormal_test = true;
        break;
      }
      subnormal_test_results.push_back(std::move(result));
    }

    if (passed_subnormal_test) {
      continue;
    }

    std::string mismatch = absl::StrFormat(
        "Mismatch on subnormal value %s.  Expected one of:\n"
        "  %10s (evaluated at full-precision value)\n",
        StringifyNum<NativeT, ComponentIntegralNativeT, N>(inputs),
        StringifyNum<NativeT, ComponentIntegralNativeT>(expected));

    CHECK_EQ(subnormal_test_inputs.size(), subnormal_test_results.size());
    for (int i = 0; i < subnormal_test_inputs.size(); ++i) {
      absl::StrAppend(
          &mismatch,
          absl::StrFormat(
              "  %10s (evaluated at %s)\n",
              StringifyNum<NativeRefT, ComponentIntegralNativeRefT>(
                  subnormal_test_results[i]),
              GetSubnormalDescription<Traits::kIsComplex, NativeRefT, N>(
                  subnormal_test_inputs[i], inputs_ref_ty)));
    }
    absl::StrAppend(
        &mismatch,
        absl::StrFormat(
            "but got %s",
            StringifyNum<NativeT, ComponentIntegralNativeT>(actual)));

    PrintMismatch(&mismatches, [mismatch] { return mismatch; });
    EXPECT_NEAR_FAIL_OR_CONTINUE();
  }
  EXPECT_EQ(mismatches, 0);

  if (should_dump_values_) {
    TF_EXPECT_OK(dump_file->Close());
  }
}

template class ExhaustiveOpTestBase<C128, 1>;
template class ExhaustiveOpTestBase<C64, 1>;
template class ExhaustiveOpTestBase<F64, 1>;
template class ExhaustiveOpTestBase<F32, 1>;
template class ExhaustiveOpTestBase<F16, 1>;
template class ExhaustiveOpTestBase<BF16, 1>;

template class ExhaustiveOpTestBase<F64, 2>;
template class ExhaustiveOpTestBase<F32, 2>;
template class ExhaustiveOpTestBase<F16, 2>;
template class ExhaustiveOpTestBase<BF16, 2>;

}  // namespace exhaustive_op_test
}  // namespace xla
