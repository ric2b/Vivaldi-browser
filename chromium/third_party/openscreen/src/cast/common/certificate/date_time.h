// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CERTIFICATE_DATE_TIME_H_
#define CAST_COMMON_CERTIFICATE_DATE_TIME_H_

#include <stdint.h>

#include <chrono>

#include "cast/common/public/certificate_types.h"

namespace openscreen::cast {

bool operator<(const DateTime& a, const DateTime& b);
bool operator>(const DateTime& a, const DateTime& b);
bool DateTimeFromSeconds(uint64_t seconds, DateTime* time);

// `time` is assumed to be valid.
std::chrono::seconds DateTimeToSeconds(const DateTime& time);

}  // namespace openscreen::cast

#endif  // CAST_COMMON_CERTIFICATE_DATE_TIME_H_
