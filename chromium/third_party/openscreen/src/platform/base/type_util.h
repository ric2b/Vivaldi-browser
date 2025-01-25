// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_TYPE_UTIL_H_
#define PLATFORM_BASE_TYPE_UTIL_H_

#include <type_traits>

// File for defining generally useful type predicates for templatized classes
// and functions.
namespace openscreen::internal {

template <typename T>
using EnableIfArithmetic =
    std::enable_if_t<std::is_arithmetic<T>::value>;  // NOLINT

template <typename From, typename To>
using EnableIfConvertible = std::enable_if_t<
    std::is_convertible<From (*)[], To (*)[]>::value>;  // NOLINT

}  // namespace openscreen::internal

#endif  // PLATFORM_BASE_TYPE_UTIL_H_
