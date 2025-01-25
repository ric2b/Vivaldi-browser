// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/simple_fraction.h"

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "absl/strings/str_split.h"
#include "util/osp_logging.h"
#include "util/string_parse.h"
#include "util/stringprintf.h"

namespace openscreen {

// static
ErrorOr<SimpleFraction> SimpleFraction::FromString(std::string_view value) {
  std::vector<std::string_view> fields = absl::StrSplit(value, '/');
  if (fields.size() != 1 && fields.size() != 2) {
    return Error::Code::kParameterInvalid;
  }

  int numerator;
  int denominator = 1;
  if (!string_parse::ParseAsciiNumber(fields[0], numerator)) {
    return Error::Code::kParameterInvalid;
  }

  if (fields.size() == 2) {
    if (!string_parse::ParseAsciiNumber(fields[1], denominator)) {
      return Error::Code::kParameterInvalid;
    }
  }

  return SimpleFraction(numerator, denominator);
}

std::string SimpleFraction::ToString() const {
  if (denominator_ == 1) {
    return std::to_string(numerator_);
  }
  return StringPrintf("%d/%d", numerator_, denominator_);
}

}  // namespace openscreen
