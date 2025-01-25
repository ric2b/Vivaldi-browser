// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_STRING_PARSE_H_
#define UTIL_STRING_PARSE_H_

#include <charconv>
#include <optional>
#include <string_view>
#include <system_error>

#include "platform/base/type_util.h"

namespace openscreen::string_parse {

// Parses `number` into the numeric type `result` and returns true if
// successful.  `number` must be an ASCII representation of an integer or
// floating point value, and `result` must be compatible with the resulting
// value.  If `number` cannot be parsed, then returns false.
template <typename T, typename = internal::EnableIfArithmetic<T>>
bool ParseAsciiNumber(std::string_view number, T& result) {
  if (number.empty())
    return false;
  auto [unused_ptr, error_code] =
      std::from_chars(number.data(), &number.back() + 1, result);
  return error_code == std::errc();
}

}  // namespace openscreen::string_parse

#endif  // UTIL_STRING_PARSE_H_
