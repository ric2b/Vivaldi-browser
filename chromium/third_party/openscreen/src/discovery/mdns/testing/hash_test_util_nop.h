// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_TESTING_HASH_TEST_UTIL_NOP_H_
#define DISCOVERY_MDNS_TESTING_HASH_TEST_UTIL_NOP_H_

#include "gtest/gtest.h"

namespace openscreen::discovery {

template <int&..., typename T>
testing::AssertionResult VerifyTypeImplementsAbslHashCorrectly(
    std::initializer_list<T> values) {
  return testing::AssertionSuccess();
}

}  // namespace openscreen::discovery

#endif  // DISCOVERY_MDNS_TESTING_HASH_TEST_UTIL_NOP_H_
