// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/certificate/testing/test_helpers.h"

#include <openssl/bytestring.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <stdio.h>

#include <cstring>

#include "util/osp_logging.h"

namespace openscreen::cast {
namespace testing {

SignatureTestData::SignatureTestData() = default;

SignatureTestData::~SignatureTestData() {
  OPENSSL_free(message.data());
  OPENSSL_free(sha1.data());
  OPENSSL_free(sha256.data());
}

SignatureTestData ReadSignatureTestData(std::string_view filename) {
  FILE* fp = fopen(filename.data(), "r");
  OSP_CHECK(fp);
  SignatureTestData result = {};
  char* name;
  char* header;
  unsigned char* data;
  long length;  // NOLINT
  while (PEM_read(fp, &name, &header, &data, &length) == 1) {
    if (std::strcmp(name, "MESSAGE") == 0) {
      OSP_CHECK(result.message.empty());
      result.message = ByteBuffer(data, length);
    } else if (std::strcmp(name, "SIGNATURE SHA1") == 0) {
      OSP_CHECK(result.sha1.empty());
      result.sha1 = ByteBuffer(data, length);
    } else if (std::strcmp(name, "SIGNATURE SHA256") == 0) {
      OSP_CHECK(result.sha256.empty());
      result.sha256 = ByteBuffer(data, length);
    } else {
      OPENSSL_free(data);
    }
    OPENSSL_free(name);
    OPENSSL_free(header);
  }
  OSP_CHECK(!result.message.empty());
  OSP_CHECK(!result.sha1.empty());
  OSP_CHECK(!result.sha256.empty());

  return result;
}

}  // namespace testing
}  // namespace openscreen::cast
