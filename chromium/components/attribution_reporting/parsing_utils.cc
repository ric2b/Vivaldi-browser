// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/parsing_utils.h"

#include <stddef.h>
#include <stdint.h>

#include <cmath>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/containers/flat_tree.h"
#include "base/functional/overloaded.h"
#include "base/not_fatal_until.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/abseil_string_number_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/aggregation_service/parsing_utils.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "url/origin.h"

namespace attribution_reporting {

namespace {

constexpr char kDebugKey[] = "debug_key";
constexpr char kDebugReporting[] = "debug_reporting";

template <typename T>
base::expected<std::optional<T>, ParseError> ParseIntegerFromString(
    const base::Value::Dict& dict,
    std::string_view key,
    bool (*parse)(std::string_view, T*)) {
  const base::Value* value = dict.Find(key);
  if (!value) {
    return std::nullopt;
  }

  T parsed_val;
  if (const std::string* str = value->GetIfString();
      !str || !parse(*str, &parsed_val)) {
    return base::unexpected(ParseError());
  }
  return parsed_val;
}

}  // namespace

base::expected<absl::uint128, ParseError> ParseAggregationKeyPiece(
    const base::Value& value) {
  const std::string* str = value.GetIfString();
  if (!str) {
    return base::unexpected(ParseError());
  }

  absl::uint128 key_piece;

  if (!base::StartsWith(*str, "0x", base::CompareCase::INSENSITIVE_ASCII) ||
      !base::HexStringToUInt128(*str, &key_piece)) {
    return base::unexpected(ParseError());
  }

  return key_piece;
}

bool AggregationKeyIdHasValidLength(const std::string& key) {
  return key.size() <= kMaxBytesPerAggregationKeyId;
}

std::string HexEncodeAggregationKey(absl::uint128 value) {
  std::ostringstream out;
  out << "0x";
  out.setf(out.hex, out.basefield);
  out << value;
  return out.str();
}

base::expected<std::optional<uint64_t>, ParseError> ParseUint64(
    const base::Value::Dict& dict,
    std::string_view key) {
  return ParseIntegerFromString<uint64_t>(dict, key, &base::StringToUint64);
}

base::expected<std::optional<int64_t>, ParseError> ParseInt64(
    const base::Value::Dict& dict,
    std::string_view key) {
  return ParseIntegerFromString<int64_t>(dict, key, &base::StringToInt64);
}

base::expected<int64_t, ParseError> ParsePriority(
    const base::Value::Dict& dict) {
  return ParseInt64(dict, kPriority).transform(&ValueOrZero<int64_t>);
}

std::optional<uint64_t> ParseDebugKey(const base::Value::Dict& dict) {
  return ParseUint64(dict, kDebugKey).value_or(std::nullopt);
}

base::expected<std::optional<uint64_t>, ParseError> ParseDeduplicationKey(
    const base::Value::Dict& dict) {
  return ParseUint64(dict, kDeduplicationKey);
}

bool ParseDebugReporting(const base::Value::Dict& dict) {
  return dict.FindBool(kDebugReporting).value_or(false);
}

base::expected<base::TimeDelta, ParseError> ParseLegacyDuration(
    const base::Value& value) {
  // Note: The full range of uint64 seconds cannot be represented in the
  // resulting `base::TimeDelta`, but this is fine because `base::Seconds()`
  // properly clamps out-of-bound values and because the Attribution
  // Reporting API itself clamps values to 30 days:
  // https://wicg.github.io/attribution-reporting-api/#valid-source-expiry-range

  return value.Visit(base::Overloaded{
      [](int int_value) -> base::expected<base::TimeDelta, ParseError> {
        if (int_value < 0) {
          return base::unexpected(ParseError());
        }
        return base::Seconds(int_value);
      },
      [](const std::string& str)
          -> base::expected<base::TimeDelta, ParseError> {
        uint64_t seconds;
        if (!base::StringToUint64(str, &seconds)) {
          return base::unexpected(ParseError());
        }
        return base::Seconds(seconds);
      },
      [](const auto&) -> base::expected<base::TimeDelta, ParseError> {
        return base::unexpected(ParseError());
      },
  });
}

base::expected<std::optional<SuitableOrigin>, ParseError>
ParseAggregationCoordinator(const base::Value::Dict& dict) {
  const base::Value* value = dict.Find(kAggregationCoordinatorOrigin);

  // The default value is used for backward compatibility prior to this
  // attribute being added, but ideally this would invalidate the registration
  // if other aggregatable fields were present.
  if (!value) {
    return std::nullopt;
  }

  const std::string* str = value->GetIfString();
  if (!str) {
    return base::unexpected(ParseError());
  }

  std::optional<url::Origin> aggregation_coordinator =
      aggregation_service::ParseAggregationCoordinator(*str);
  if (!aggregation_coordinator.has_value()) {
    return base::unexpected(ParseError());
  }
  auto aggregation_coordinator_origin =
      SuitableOrigin::Create(*aggregation_coordinator);
  CHECK(aggregation_coordinator_origin.has_value(), base::NotFatalUntil::M128);
  return *std::move(aggregation_coordinator_origin);
}

void SerializeUint64(base::Value::Dict& dict,
                     std::string_view key,
                     uint64_t value) {
  dict.Set(key, base::NumberToString(value));
}

void SerializeInt64(base::Value::Dict& dict,
                    std::string_view key,
                    int64_t value) {
  dict.Set(key, base::NumberToString(value));
}

void SerializePriority(base::Value::Dict& dict, int64_t priority) {
  SerializeInt64(dict, kPriority, priority);
}

void SerializeDebugKey(base::Value::Dict& dict,
                       std::optional<uint64_t> debug_key) {
  if (debug_key) {
    SerializeUint64(dict, kDebugKey, *debug_key);
  }
}

void SerializeDebugReporting(base::Value::Dict& dict, bool debug_reporting) {
  dict.Set(kDebugReporting, debug_reporting);
}

void SerializeDeduplicationKey(base::Value::Dict& dict,
                               std::optional<uint64_t> dedup_key) {
  if (dedup_key) {
    SerializeUint64(dict, kDeduplicationKey, *dedup_key);
  }
}

void SerializeTimeDeltaInSeconds(base::Value::Dict& dict,
                                 std::string_view key,
                                 base::TimeDelta value) {
  int64_t seconds = value.InSeconds();
  if (base::IsValueInRangeForNumericType<int>(seconds)) {
    dict.Set(key, static_cast<int>(seconds));
  } else {
    SerializeInt64(dict, key, seconds);
  }
}

base::expected<uint32_t, ParseError> ParseUint32(const base::Value& value) {
  // We use `base::Value::GetIfDouble()`, which coerces if the value is an
  // integer, because not all `uint32_t` can be represented by 32-bit `int`.
  // We use `std::modf` to check that the fractional part of the `double` is 0.
  //
  // Assumes that all `uint32_t` can be represented either by `int` or `double`,
  // and that when represented internally by `base::Value` as an `int`, can be
  // precisely represented by `double`.
  //
  // TODO(apaseltiner): Consider test coverage for all `uint32_t` values, or
  // some kind of fuzzer.
  std::optional<double> double_value = value.GetIfDouble();
  if (double int_part;
      !double_value.has_value() || std::modf(*double_value, &int_part) != 0 ||
      !base::IsValueInRangeForNumericType<uint32_t>(*double_value)) {
    return base::unexpected(ParseError());
  }

  return static_cast<uint32_t>(*double_value);
}

base::expected<uint32_t, ParseError> ParsePositiveUint32(
    const base::Value& value) {
  ASSIGN_OR_RETURN(uint32_t int_value, ParseUint32(value));
  if (int_value == 0) {
    return base::unexpected(ParseError());
  }
  return int_value;
}

base::Value Uint32ToJson(uint32_t value) {
  // All `uint32_t` can be represented exactly by `double`.
  return base::IsValueInRangeForNumericType<int>(value)
             ? base::Value(static_cast<int>(value))
             : base::Value(static_cast<double>(value));
}

base::expected<base::flat_set<std::string>, StringSetError> ExtractStringSet(
    base::Value::List list,
    const size_t max_string_size,
    const size_t max_set_size) {
  for (const base::Value& item : list) {
    const std::string* string = item.GetIfString();
    if (!string) {
      return base::unexpected(StringSetError::kWrongType);
    }

    if (string->size() > max_string_size) {
      return base::unexpected(StringSetError::kStringTooLong);
    }
  }

  base::ranges::sort(list);
  list.erase(base::ranges::unique(list), list.end());

  if (list.size() > max_set_size) {
    return base::unexpected(StringSetError::kSetTooLong);
  }

  std::vector<std::string> values;
  values.reserve(list.size());

  for (base::Value& item : list) {
    values.emplace_back(std::move(item).TakeString());
  }

  return base::flat_set<std::string>(base::sorted_unique, std::move(values));
}

}  // namespace attribution_reporting
