// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CERTIFICATE_TESTING_TEST_HELPERS_H_
#define CAST_COMMON_CERTIFICATE_TESTING_TEST_HELPERS_H_

#include <string_view>

#include "platform/base/span.h"

namespace openscreen::cast {
namespace testing {

class SignatureTestData {
 public:
  SignatureTestData();
  ~SignatureTestData();

  ByteBuffer message;
  ByteBuffer sha1;
  ByteBuffer sha256;
};

SignatureTestData ReadSignatureTestData(std::string_view filename);

}  // namespace testing
}  // namespace openscreen::cast

#endif  // CAST_COMMON_CERTIFICATE_TESTING_TEST_HELPERS_H_
